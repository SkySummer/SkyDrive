#ifndef CORE_CONNECTION_H
#define CORE_CONNECTION_H

#include <atomic>
#include <functional>

#include <netinet/in.h>

#include "core/address.h"
#include "core/http_request.h"

// 前向声明
class EpollManager;
class Logger;
class StaticFile;
class UserManager;
class HttpResponse;

class Connection {
public:
    Connection(int client_fd, const sockaddr_in& addr, EpollManager* epoll, Logger* logger, StaticFile* static_file,
               UserManager* user_manager, bool linger = false);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    [[nodiscard]] int fd() const;
    [[nodiscard]] const Address& info() const;

    void handleRead() const;
    void handleWrite() const;

    void setCloseRequestCallback(std::function<void(int)> callback);

private:
    int client_fd_;
    Address info_;
    EpollManager* epoll_manager_;
    Logger* logger_;
    StaticFile* static_file_;
    UserManager* user_manager_;

    mutable std::string request_buffer_;  // 用于存储请求数据
    mutable HttpRequest request_;         // 用于解析请求

    mutable std::string write_buffer_;         // 用于存储响应数据
    mutable std::string_view pending_buffer_;  // 用于存储待发送的数据

    std::atomic<bool> closed_{false};  // 是否关闭连接

    std::function<void(int)> callback_;

    void readAndHandleRequest() const;
    void tryParseAndHandleRequest() const;

    [[nodiscard]] HttpResponse handleRequest(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse handleGetRequest(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse handlePostRequest(const HttpRequest& request) const;

    void requestCloseConnection() const;
    void closeConnection();
    void applyLinger(bool flag) const;
};

#endif  // CORE_CONNECTION_H
