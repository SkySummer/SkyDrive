#ifndef USER_USER_MANAGER_H
#define USER_USER_MANAGER_H

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

// 前向声明
class Logger;
class SessionManager;
class HttpRequest;

class UserManager {
public:
    UserManager(std::filesystem::path path, Logger* logger, SessionManager* session_manager);
    ~UserManager();

    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;
    UserManager(UserManager&&) = delete;
    UserManager& operator=(UserManager&&) = delete;

    std::string registerUser(const HttpRequest& request);

    std::string loginUser(const HttpRequest& request);

    std::string changePassword(const HttpRequest& request);

    std::string logoutUser(const HttpRequest& request) const;

private:
    size_t loadUsers();
    void saveUsers();

    struct UserInfo {
        std::string salt;
        std::string password;
    };

    std::filesystem::path path_;
    Logger* logger_;
    SessionManager* session_manager_;
    std::unordered_map<std::string, UserInfo> users_;
    std::mutex users_mutex_;
};

#endif  // USER_USER_MANAGER_H
