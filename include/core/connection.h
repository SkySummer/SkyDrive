#ifndef CORE_CONNECTION_H
#define CORE_CONNECTION_H

#include <atomic>
#include <functional>

#include <netinet/in.h>

#include "core/address.h"

// 前向声明
class EpollManager;
class Logger;
class StaticFile;
class UserManager;
class HttpRequest;
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

    void handle() const;

    void setCloseRequestCallback(std::function<void(int)> callback);

private:
    int client_fd_;
    Address info_;
    EpollManager* epoll_manager_;
    Logger* logger_;
    StaticFile* static_file_;
    UserManager* user_manager_;

    std::atomic<bool> closed_{false};  // 是否关闭连接

    std::function<void(int)> callback_;

    void readAndHandleRequest() const;

    [[nodiscard]] HttpResponse handleRequest(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse handleGetRequest(const HttpRequest& request) const;
    [[nodiscard]] HttpResponse handlePostRequest(const HttpRequest& request) const;

    void closeConnection();
    void applyLinger(bool flag) const;
};

#endif  // CORE_CONNECTION_H
