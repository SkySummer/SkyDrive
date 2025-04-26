#ifndef USER_USER_MANAGER_H
#define USER_USER_MANAGER_H

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

// 前向声明
class Logger;

class UserManager {
public:
    explicit UserManager(std::filesystem::path path, Logger* logger);
    ~UserManager();

    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;
    UserManager(UserManager&&) = delete;
    UserManager& operator=(UserManager&&) = delete;

    std::string registerUser(const std::string& body);

    std::string loginUser(const std::string& body);

    std::string changePassword(const std::string& body);

private:
    size_t loadUsers();
    void saveUsers();

    struct UserInfo {
        std::string salt;
        std::string password;
    };

    std::filesystem::path path_;
    Logger* logger_;
    std::unordered_map<std::string, UserInfo> users_;
    std::mutex users_mutex_;
};

#endif  // USER_USER_MANAGER_H
