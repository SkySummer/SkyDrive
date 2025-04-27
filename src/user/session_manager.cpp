#include "user/session_manager.h"

#include <optional>
#include <random>
#include <sstream>
#include <string>

std::string SessionManager::createSession(const std::string& username) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::ostringstream oss;

    static thread_local std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dis;

    const uint64_t random_number = dis(gen);
    oss << std::hex << now << std::hex << random_number;

    const std::string session_id = oss.str();

    {
        std::lock_guard lock(mutex_);
        sessions_[session_id] = username;
    }

    return session_id;
}

std::optional<std::string> SessionManager::getUsername(const std::string& session_id) {
    std::lock_guard lock(mutex_);
    if (const auto iter = sessions_.find(session_id); iter != sessions_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

void SessionManager::removeSession(const std::string& session_id) {
    std::lock_guard lock(mutex_);
    sessions_.erase(session_id);
}
