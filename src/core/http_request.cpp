#include "core/http_request.h"

#include <sstream>

HttpRequest::HttpRequest(const std::string& request) {
    parse(request);
}

void HttpRequest::parse(const std::string& request) {
    clear();

    const size_t request_line_end = request.find("\r\n");
    if (request_line_end == std::string::npos) {
        throw std::invalid_argument("Invalid HTTP request format");
    }

    // 提取请求行
    const std::string request_line = request.substr(0, request_line_end);
    {
        std::istringstream iss(request_line);
        iss >> method_ >> path_ >> version_;
        if (version_.empty()) {
            throw std::invalid_argument("Invalid HTTP request format");
        }
    }

    // 提取请求头
    const size_t headers_start = request_line_end + 2;  // 跳过 "\r\n"
    const size_t headers_end = request.find("\r\n\r\n", headers_start);
    if (headers_end != std::string::npos) {
        std::istringstream iss(request.substr(headers_start, headers_end - headers_start));
        std::string header_line;
        while (std::getline(iss, header_line) && !header_line.empty()) {
            if (header_line.back() == '\r') {
                header_line.pop_back();  // 去掉末尾的 '\r'
            }

            const size_t colon_pos = header_line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = header_line.substr(0, colon_pos);
                std::string value = header_line.substr(colon_pos + 1);

                trim(key);
                trim(value);
                headers_[key] = value;
            }
        }
    } else {
        throw std::invalid_argument("Invalid HTTP request format");
    }

    // 提取请求体
    if (headers_end != std::string::npos && headers_end + 4 < request.size()) {
        body_ = request.substr(headers_end + 4);
    }
}

const std::string& HttpRequest::method() const {
    return method_;
}

const std::string& HttpRequest::path() const {
    return path_;
}

const std::string& HttpRequest::version() const {
    return version_;
}

const std::unordered_map<std::string, std::string>& HttpRequest::headers() const {
    return headers_;
}

const std::string& HttpRequest::body() const {
    return body_;
}

std::optional<std::string> HttpRequest::getHeader(const std::string& key) const {
    if (const auto iter = headers_.find(key); iter != headers_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

void HttpRequest::clear() {
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
}

void HttpRequest::trim(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);
}
