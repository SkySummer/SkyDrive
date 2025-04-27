#ifndef USER_SESSION_MANAGER_H
#define USER_SESSION_MANAGER_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class SessionManager {
public:
    std::string createSession(const std::string& username);

    [[nodiscard]] std::optional<std::string> getUsername(const std::string& session_id);

    void removeSession(const std::string& session_id);

private:
    std::unordered_map<std::string, std::string> sessions_;  // session_id -> username
    std::mutex mutex_;
};

#endif  // USER_SESSION_MANAGER_H
