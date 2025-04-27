#include "core/connection.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#include "core/epoll_manager.h"
#include "core/http_request.h"
#include "core/static_file.h"
#include "user/user_manager.h"
#include "utils/http_form_data.h"
#include "utils/logger.h"

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
    epoll_manager_->addFd(client_fd_, EPOLLIN);

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
}

void Connection::readAndHandleRequest() const {
    if (closed_) {
        logger_->log(LogLevel::WARNING, info_, "Connection already closed.");
        return;
    }

    constexpr std::size_t buffer_size = 4096;
    std::array<char, buffer_size> buffer{};  // 用于存储从客户端接收到的数据
    const ssize_t bytes_read = read(client_fd_, buffer.data(), buffer.size());

    if (bytes_read == 0) {
        // 如果读到 0 字节，说明客户端关闭连接
        if (callback_) {
            callback_(client_fd_);
        }
        return;
    }

    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;  // 没有更多数据可读
        }
        if (errno == ECONNRESET) {
            logger_->log(LogLevel::INFO, info_, "Connection reset by peer.");
        } else {
            logger_->log(LogLevel::ERROR, info_, std::format("Failed to read from client: {}", strerror(errno)));
        }

        if (callback_) {
            callback_(client_fd_);
        }
        return;
    }

    std::string response;

    try {
        // 将读取的内容转换为 std::string 以便处理
        const std::string request(buffer.data(), bytes_read);

        const HttpRequest http_request(request);

        response = handleRequest(http_request);
    } catch (const std::invalid_argument& e) {
        logger_->log(LogLevel::INFO, info_, std::format("Invalid HTTP request: {}", e.what()));
        constexpr int error_code = 400;
        response = HttpResponse::buildErrorResponse(error_code);
    } catch (const std::exception& e) {
        logger_->log(LogLevel::ERROR, info_, std::format("Exception: {}", e.what()));
        constexpr int error_code = 500;
        response = HttpResponse::buildErrorResponse(error_code);
    }

    if (response.empty()) {
        logger_->log(LogLevel::ERROR, info_, "Empty response generated.");
        constexpr int error_code = 500;
        response = HttpResponse::buildErrorResponse(error_code);
    }

    write(client_fd_, response.c_str(), response.size());

    if (callback_) {
        callback_(client_fd_);
    }
}

std::string Connection::handleRequest(const HttpRequest& request) const {
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
    return HttpResponse::buildErrorResponse(error_code);
}

std::string Connection::handleGetRequest(const HttpRequest& request) const {
    return static_file_->serve(request, info_);
}

std::string Connection::handlePostRequest(const HttpRequest& request) const {
    if (HttpFormData(request.body()).empty()) {
        constexpr int error_code = 400;
        return HttpResponse::buildErrorResponse(error_code, "No form data received.");
    }

    const std::string& path = request.path();

    if (path == "/login") {
        return user_manager_->loginUser(request.body());
    }

    if (path == "/register") {
        return user_manager_->registerUser(request.body());
    }

    if (path == "/change_password") {
        return user_manager_->changePassword(request.body());
    }

    constexpr int error_code = 405;
    return HttpResponse::buildErrorResponse(error_code);
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
