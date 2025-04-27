#ifndef UTILS_COOKIE_PARSER_H
#define UTILS_COOKIE_PARSER_H

#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "core/http_request.h"

class CookieParser {
public:
    static std::unordered_map<std::string, std::string> parse(const HttpRequest& request) {
        auto cookie_header = request.getHeader("Cookie");
        if (!cookie_header) {
            return {};
        }
        return parse(*cookie_header);
    }

    static std::unordered_map<std::string, std::string> parse(const std::string& cookie_header) {
        std::unordered_map<std::string, std::string> cookies;
        std::istringstream iss(cookie_header);
        std::string pair;

        while (std::getline(iss, pair, ';')) {
            const auto pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                trim(key);
                trim(value);

                cookies[key] = value;
            }
        }

        return cookies;
    }

    static std::optional<std::string> get(const HttpRequest& request, const std::string& key) {
        const auto cookies = parse(request);
        if (const auto iter = cookies.find(key); iter != cookies.end()) {
            return iter->second;
        }
        return std::nullopt;
    }

    static std::optional<std::string> get(const std::string& cookie_header, const std::string& key) {
        const auto cookies = parse(cookie_header);
        if (const auto iter = cookies.find(key); iter != cookies.end()) {
            return iter->second;
        }
        return std::nullopt;
    }

private:
    static void trim(std::string& str) {
        str.erase(0, str.find_first_not_of(" \t"));
        str.erase(str.find_last_not_of(" \t") + 1);
    }
};

#endif  // UTILS_COOKIE_PARSER_H
