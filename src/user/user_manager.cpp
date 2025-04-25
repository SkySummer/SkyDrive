#include "user/user_manager.h"

#include <format>

#include "core/http_response.h"
#include "utils/http_form_data.h"
#include "utils/logger.h"

UserManager::UserManager(Logger* logger) : logger_(logger) {
    logger_->log(LogLevel::INFO, "UserManager initialized.");
}

std::string UserManager::registerUser(const std::string& body) {
    const auto form_data = HttpFormData(body);
    if (auto invalid = form_data.check({"username", "password", "confirm_password"})) {
        return *invalid;
    }

    const std::string username = form_data.get("username").value_or("");
    const std::string password = form_data.get("password").value_or("");

    if (password != form_data.get("confirm_password").value_or("-")) {
        constexpr int error_code = 400;
        return HttpResponse::buildErrorResponse(error_code, "Passwords do not match.");
    }

    bool exists = false;
    {
        std::lock_guard lock(users_mutex_);
        exists = users_.contains(username);
        if (!exists) {
            users_[username] = {.password = password};
        }
    }

    if (exists) {
        constexpr int error_code = 400;
        return HttpResponse::buildErrorResponse(error_code, "Username already exists.");
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
        checked = iter != users_.end() && iter->second.password == password;
    }

    if (!checked) {
        constexpr int error_code = 401;
        return HttpResponse::buildErrorResponse(error_code, "Invalid username or password.");
    }

    logger_->log(LogLevel::INFO, std::format("User logged in successfully: {}", username));

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/plain; charset=UTF-8")
        .setBody("Login successful.")
        .build();
}
