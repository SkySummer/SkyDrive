#ifndef USER_USER_MANAGER_H
#define USER_USER_MANAGER_H

#include <mutex>
#include <string>
#include <unordered_map>

// 前向声明
class Logger;

class UserManager {
public:
    explicit UserManager(Logger* logger);

    std::string registerUser(const std::string& body);

    std::string loginUser(const std::string& body);

    std::string changePassword(const std::string& body);

private:
    struct UserInfo {
        std::string salt;
        std::string password;
    };

    Logger* logger_;
    std::unordered_map<std::string, UserInfo> users_;
    std::mutex users_mutex_;
};

#endif  // USER_USER_MANAGER_H
