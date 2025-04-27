#include "user/user_manager.h"

#include <cstddef>
#include <format>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/http_request.h"
#include "core/http_response.h"
#include "user/session_manager.h"
#include "utils/base64.h"
#include "utils/cookie_parser.h"
#include "utils/hash.h"
#include "utils/http_form_data.h"
#include "utils/logger.h"

#define STR_HELPER(x) #x      // NOLINT(cppcoreguidelines-macro-usage)
#define STR(x) STR_HELPER(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace {
    std::string formatUserCount(size_t user_count) {
        return std::format("{} {}", user_count, user_count == 1 ? "user" : "users");
    }
}  // namespace

UserManager::UserManager(std::filesystem::path path, Logger* logger, SessionManager* session_manager)
    : path_(std::move(path)), logger_(logger), session_manager_(session_manager) {
    const size_t user_count = loadUsers();
    logger_->log(LogLevel::INFO, std::format("UserManager initialized with {}", formatUserCount(user_count)));
    logger_->log(LogLevel::INFO, std::format("-- user_file: {}", path_.string()));
}

UserManager::~UserManager() {
    saveUsers();
    logger_->log(LogLevel::INFO, "UserManager saved and destroyed");
}

std::string UserManager::registerUser(const HttpRequest& request) {
    const auto form_data = HttpFormData(request.body());
    if (auto invalid = form_data.check({"username", "password", "confirm_password"})) {
        return *invalid;
    }

    const std::string username = form_data.get("username").value_or("");
    const std::string password = form_data.get("password").value_or("");

    if (password != form_data.get("confirm_password").value_or("-")) {
        return HttpResponse::buildAlertResponse("两次输入的密码不一致，请重新输入。");
    }

    const std::string salt = Hash::randomSalt();
    const std::string hashed = Hash::saltedHash(salt, password);

    bool exists = false;
    {
        std::lock_guard lock(users_mutex_);
        exists = users_.contains(username);
        if (!exists) {
            users_[username] = {.salt = salt, .password = hashed};
        }
    }

    if (exists) {
        return HttpResponse::buildAlertResponse("用户名已存在，请重新输入。");
    }

    logger_->log(LogLevel::INFO, std::format("User registered successfully: {}", username));
    saveUsers();

    auto session_id = session_manager_->createSession(username);

    return HttpResponse{}
        .setStatus("200 OK")
        .addHeader("Set-Cookie", std::format("session_id={}; Path=/; HttpOnly", session_id))
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Registration successful.")
        .build();
}

std::string UserManager::loginUser(const HttpRequest& request) {
    const auto form_data = HttpFormData(request.body());
    if (auto invalid = form_data.check({"username", "password"})) {
        return *invalid;
    }

    const std::string username = form_data.get("username").value_or("");
    const std::string password = form_data.get("password").value_or("");

    bool checked = false;
    {
        std::lock_guard lock(users_mutex_);
        const auto iter = users_.find(username);
        checked = iter != users_.end() && iter->second.password == Hash::saltedHash(users_[username].salt, password);
    }

    if (!checked) {
        return HttpResponse::buildAlertResponse("用户名或密码错误，请重新输入。");
    }

    logger_->log(LogLevel::INFO, std::format("User logged in successfully: {}", username));

    auto session_id = session_manager_->createSession(username);

    return HttpResponse{}
        .setStatus("200 OK")
        .addHeader("Set-Cookie", std::format("session_id={}; Path=/; HttpOnly", session_id))
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Login successful.")
        .build();
}

std::string UserManager::changePassword(const HttpRequest& request) {
    const auto form_data = HttpFormData(request.body());
    if (auto invalid = form_data.check({"username", "old_password", "new_password", "confirm_password"})) {
        return *invalid;
    }

    const std::string username = form_data.get("username").value_or("");
    const std::string old_password = form_data.get("old_password").value_or("");
    const std::string new_password = form_data.get("new_password").value_or("");

    if (new_password != form_data.get("confirm_password").value_or("-")) {
        return HttpResponse::buildAlertResponse("两次输入的新密码不一致，请重新输入。");
    }

    const std::string salt = Hash::randomSalt();

    bool changed = false;
    {
        std::lock_guard lock(users_mutex_);
        if (const auto iter = users_.find(username);
            iter != users_.end() && iter->second.password == Hash::saltedHash(iter->second.salt, old_password)) {
            iter->second = {.salt = salt, .password = Hash::saltedHash(salt, new_password)};
            changed = true;
        }
    }

    if (!changed) {
        return HttpResponse::buildAlertResponse("用户名或旧密码错误，请重新输入。");
    }

    logger_->log(LogLevel::INFO, std::format("User password changed successfully: {}", username));
    saveUsers();

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Password changed successfully.")
        .build();
}

std::string UserManager::logoutUser(const HttpRequest& request) const {
    const auto session_id = CookieParser::get(request, "session_id");
    if (!session_id) {
        return HttpResponse::buildAlertResponse("未登录或会话已过期，请重新登录。");
    }
    if (!session_manager_->getUsername(*session_id)) {
        return HttpResponse::buildAlertResponse("未登录或会话已过期，请重新登录。");
    }

    session_manager_->removeSession(*session_id);
    logger_->log(LogLevel::INFO, std::format("User logged out. Session id: {}", *session_id));

    return HttpResponse{}
        .setStatus("200 OK")
        .addHeader("Set-Cookie", "session_id=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Logout successful.")
        .build();
}

size_t UserManager::loadUsers() {
    std::ifstream file(path_);
    if (!file.is_open()) {
        if (exists(path_)) {
            logger_->log(LogLevel::ERROR, "User data file exists but cannot be opened. Check permissions.");
        } else {
            logger_->log(LogLevel::INFO, "User data file not found. Starting with empty user database.");
        }
        return 0;
    }

    std::string line;
    std::lock_guard lock(users_mutex_);
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string part;
        std::vector<std::string> parts;

        while (std::getline(iss, part, '|')) {
            parts.push_back(part);
        }

        if (parts.size() != 3) {
            logger_->log(LogLevel::ERROR, std::format("Invalid user data: {}", line));
            continue;
        }

        const std::string username = Base64::decode(parts[0]);
        const std::string salt = Base64::decode(parts[1]);
        const std::string password = Base64::decode(parts[2]);

        users_[username] = {.salt = salt, .password = password};
    }

    return users_.size();
}

void UserManager::saveUsers() {
    std::ofstream file(path_, std::ios::trunc);
    if (!file.is_open()) {
        logger_->log(LogLevel::ERROR, std::format("Failed to open user file for writing: {}", path_.string()));
        return;
    }

    std::lock_guard lock(users_mutex_);
    for (const auto& [username, user_info] : users_) {
        const std::string encoded_username = Base64::encode(username);
        const std::string encoded_salt = Base64::encode(user_info.salt);
        const std::string encoded_password = Base64::encode(user_info.password);

        file << encoded_username << "|" << encoded_salt << "|" << encoded_password << "\n";
    }
}
