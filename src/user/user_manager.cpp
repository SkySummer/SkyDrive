#include "user/user_manager.h"

#include <format>

#include "core/http_response.h"
#include "utils/hash.h"
#include "utils/http_form_data.h"
#include "utils/logger.h"

UserManager::UserManager(Logger* logger) : logger_(logger) {
    logger_->log(LogLevel::INFO, "UserManager initialized");
}

std::string UserManager::registerUser(const std::string& body) {
    const auto form_data = HttpFormData(body);
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

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Registration successful.")
        .build();
}

std::string UserManager::loginUser(const std::string& body) {
    const auto form_data = HttpFormData(body);
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

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Login successful.")
        .build();
}

std::string UserManager::changePassword(const std::string& body) {
    const auto form_data = HttpFormData(body);
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

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Password changed successfully.")
        .build();
}
