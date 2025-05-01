#include "core/connection.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include <unistd.h>

#include "core/epoll_manager.h"
#include "core/http_request.h"
#include "core/http_response.h"
#include "core/static_file.h"
#include "user/user_manager.h"
#include "utils/logger.h"
#include "utils/upload_file.h"
#include "utils/url.h"

namespace {
    std::string formatSize(const size_t bytes) {
        return std::format("{} {}", bytes, bytes == 1 ? "byte" : "bytes");
    }
}  // namespace

Connection::Connection(const int client_fd, const sockaddr_in& addr, EpollManager* epoll, Logger* logger,
                       StaticFile* static_file, UserManager* user_manager, const bool linger)
    : client_fd_(client_fd),
      info_(addr, client_fd),
      epoll_manager_(epoll),
      logger_(logger),
      static_file_(static_file),
      user_manager_(user_manager) {
    // 设置 linger 选项
    applyLinger(linger);

    // 将客户端 socket 添加到 epoll 中，监听读写事件
    epoll_manager_->addFd(client_fd_, EPOLLIN | EPOLLET | EPOLLONESHOT);

    logger_->log(LogLevel::INFO, info_, "New client connected.");
}

Connection::~Connection() {
    closeConnection();
}

int Connection::fd() const {
    return client_fd_;
}

const Address& Connection::info() const {
    return info_;
}

void Connection::handle() const {
    readAndHandleRequest();

    // 处理完请求后，重新注册 epoll 事件
    epoll_manager_->modFd(client_fd_, EPOLLIN | EPOLLET | EPOLLONESHOT);
}

void Connection::readAndHandleRequest() const {
    if (closed_) {
        logger_->log(LogLevel::WARNING, info_, "Connection already closed.");
        return;
    }

    constexpr std::size_t buffer_size = 8192;
    std::array<char, buffer_size> buffer{};  // 用于存储从客户端接收到的数据

    while (true) {
        const ssize_t bytes_read = recv(client_fd_, buffer.data(), buffer_size, 0);

        if (bytes_read == 0) {
            // 客户端关闭连接
            if (callback_) {
                callback_(client_fd_);
            }
            break;
        }

        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            if (errno == ECONNRESET) {
                logger_->log(LogLevel::INFO, info_, "Connection reset by peer.");
            } else {
                logger_->log(LogLevel::ERROR, info_, std::format("Failed to read from client: {}", strerror(errno)));
            }

            if (callback_) {
                callback_(client_fd_);
            }
            break;
        }

        request_buffer_.append(buffer.data(), bytes_read);
    }

    tryParseAndHandleRequest();
}

void Connection::tryParseAndHandleRequest() const {
    std::string response;

    try {
        if (!request_.isHeaderParsed()) {
            if (!request_.parseHeader(request_buffer_)) {
                // 请求头不完整
                return;
            }
        }

        if (request_buffer_.size() < request_.totalExpectedLength()) {
            // 请求体不完整
            return;
        }

        logger_->log(LogLevel::DEBUG, info_,
                     std::format("Received {} from client.", formatSize(request_buffer_.size())));

        request_.parseBody(request_buffer_);
        response = handleRequest(request_).build();
    } catch (const std::invalid_argument& e) {
        logger_->log(LogLevel::INFO, info_, std::format("Invalid HTTP request: {}", e.what()));
        constexpr int error_code = 400;
        response = HttpResponse::responseError(error_code).build();
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, info_, std::format("Exception during request parsing: {}", e.what()));
        constexpr int error_code = 500;
        response = HttpResponse::responseError(error_code).build();
    }

    if (response.empty()) {
        logger_->log(LogLevel::ERROR, info_, "Generated response is empty.");
        constexpr int error_code = 500;
        response = HttpResponse::responseError(error_code).build();
    }

    if (send(client_fd_, response.c_str(), response.size(), 0) < static_cast<ssize_t>(response.size())) {
        logger_->log(LogLevel::ERROR, info_, std::format("Failed to send response: {}", strerror(errno)));
    }

    if (callback_) {
        callback_(client_fd_);
    }
}

HttpResponse Connection::handleRequest(const HttpRequest& request) const {
    const std::string& method = request.method();
    const std::string& path = request.path();

    logger_->log(LogLevel::DEBUG, info_, std::format("Handling {} for path: {}", method, path));

    if (method == "GET") {
        return handleGetRequest(request);
    }

    if (method == "POST") {
        return handlePostRequest(request);
    }

    logger_->log(LogLevel::DEBUG, info_, std::format("Unsupported method: {} on path: {}", method, path));
    constexpr int error_code = 405;
    return HttpResponse::responseError(error_code);
}

HttpResponse Connection::handleGetRequest(const HttpRequest& request) const {
    const std::string& path = Url::decode(request.path());

    if (user_manager_->isLoggedIn(request)) {
        static const std::string drive_url = '/' + static_file_->getDriveUrl() + '/';
        static const std::unordered_map<std::string, std::string> redirect_map = {
            {"/login", drive_url},    {"/login.htm", drive_url},    {"/login.html", drive_url},
            {"/register", drive_url}, {"/register.htm", drive_url}, {"/register.html", drive_url},
        };

        if (redirect_map.contains(path)) {
            const std::string& location = redirect_map.at(path);
            logger_->log(LogLevel::DEBUG, info_, std::format("Redirecting to: {}", location));
            constexpr int redirect_code = 302;
            return HttpResponse::responseRedirect(redirect_code, location);
        }
    } else {
        if (static_file_->isDriveUrl(path)) {
            // 如果用户未登录，则重定向到登录页面
            logger_->log(LogLevel::DEBUG, info_, std::format("Redirecting to login page for path: {}", path));
            constexpr int redirect_code = 302;
            return HttpResponse::responseRedirect(redirect_code, "/login");
        }
    }

    return static_file_->serve(request, info_);
}

HttpResponse Connection::handlePostRequest(const HttpRequest& request) const {
    const std::string& path = request.path();

    if (path == "/login") {
        return user_manager_->loginUser(request);
    }

    if (path == "/register") {
        return user_manager_->registerUser(request);
    }

    if (path == "/reset-password") {
        return user_manager_->changePassword(request);
    }

    if (path == "/logout") {
        return user_manager_->logoutUser(request);
    }

    if (path.ends_with("/upload")) {
        if (!user_manager_->isLoggedIn(request)) {
            // 如果用户未登录，则返回 401 错误
            logger_->log(LogLevel::DEBUG, info_, "Unauthorized upload attempt.");
            constexpr int error_code = 401;
            return HttpResponse::responseError(error_code, "You must be logged in to upload files.");
        }

        const UploadFile upload(request, logger_, static_file_, info_);
        return upload.process();
    }

    constexpr int error_code = 405;
    return HttpResponse::responseError(error_code);
}

void Connection::closeConnection() {
    if (closed_.exchange(true)) {
        return;
    }

    // 从 epoll 中删除客户端 socket
    epoll_manager_->delFd(client_fd_);
    close(client_fd_);

    logger_->log(LogLevel::INFO, info_, "Client disconnected.");
}

void Connection::applyLinger(const bool flag) const {
    if (flag) {
        linger so_linger{};
        so_linger.l_onoff = 1;
        so_linger.l_linger = 1;
        setsockopt(client_fd_, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
    }
}

void Connection::setCloseRequestCallback(std::function<void(int)> callback) {
    callback_ = std::move(callback);
}
