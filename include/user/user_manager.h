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
class HttpResponse;

class UserManager {
public:
    UserManager(std::filesystem::path path, Logger* logger, SessionManager* session_manager,
                const std::string& drive_dir);
    ~UserManager();

    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;
    UserManager(UserManager&&) = delete;
    UserManager& operator=(UserManager&&) = delete;

    [[nodiscard]] HttpResponse registerUser(const HttpRequest& request);

    [[nodiscard]] HttpResponse loginUser(const HttpRequest& request);

    [[nodiscard]] HttpResponse changePassword(const HttpRequest& request);

    [[nodiscard]] HttpResponse logoutUser(const HttpRequest& request) const;

    [[nodiscard]] bool isLoggedIn(const HttpRequest& request) const;
    [[nodiscard]] bool isLoggedIn(const std::string& session_id) const;

private:
    size_t loadUsers();
    size_t saveUsers();

    struct UserInfo {
        std::string salt;
        std::string password;
    };

    std::filesystem::path path_;
    Logger* logger_;
    SessionManager* session_manager_;
    std::unordered_map<std::string, UserInfo> users_;
    std::mutex users_mutex_;

    const std::string drive_dir_;
};

#endif  // USER_USER_MANAGER_H
