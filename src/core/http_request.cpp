#include "core/http_request.h"

#include <cstddef>
#include <sstream>

bool HttpRequest::parseHeader(const std::string& raw) {
    if (header_parsed_) {
        return true;
    }

    header_end_pos_ = raw.find("\r\n\r\n");
    if (header_end_pos_ == std::string::npos) {
        return false;  // 请求头不完整
    }

    // 提取请求行
    const size_t request_line_end = raw.find("\r\n");
    {
        std::istringstream iss(raw.substr(0, request_line_end));
        iss >> method_ >> path_ >> version_;
        if (method_.empty() || path_.empty() || version_.empty()) {
            throw std::invalid_argument("Invalid HTTP request line");
        }
    }

    // 提取请求头
    const size_t headers_start = request_line_end + 2;
    const size_t headers_end = header_end_pos_;
    std::istringstream iss(raw.substr(headers_start, headers_end - headers_start));

    std::string line;
    while (std::getline(iss, line) && !line.empty()) {
        if (line.back() == '\r') {
            line.pop_back();
        }

        if (const size_t colon_pos = line.find(':'); colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            trim(key);
            trim(value);
            headers_[key] = value;
        }
    }

    if (const auto iter = headers_.find("Content-Length"); iter != headers_.end()) {
        content_length_ = std::stoul(iter->second);
    }

    header_parsed_ = true;
    return true;
}

void HttpRequest::parseBody(const std::string& raw) {
    if (!header_parsed_) {
        throw std::logic_error("Cannot parse body before parsing headers");
    }

    // 提取请求体
    if (const size_t body_start = header_end_pos_ + 4; body_start < raw.size()) {
        body_ = raw.substr(body_start, content_length_);
    } else {
        body_.clear();
    }
}

size_t HttpRequest::totalExpectedLength() const {
    return header_end_pos_ != std::string::npos ? header_end_pos_ + 4 + content_length_ : std::string::npos;
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

std::optional<std::string> HttpRequest::getBoundary() const {
    if (auto content_type = getHeader("Content-Type")) {
        const std::string boundary_prefix = "boundary=";
        if (const size_t pos = content_type->find(boundary_prefix); pos != std::string::npos) {
            const size_t start = pos + boundary_prefix.length();
            const size_t end = content_type->find(';', start);
            return content_type->substr(start, end - start);
        }
    }

    return std::nullopt;
}

bool HttpRequest::isHeaderParsed() const {
    return header_parsed_;
}

void HttpRequest::reset() {
    method_.clear();
    path_.clear();
    version_.clear();
    headers_.clear();
    body_.clear();
    header_parsed_ = false;
    header_end_pos_ = std::string::npos;
    content_length_ = 0;
}

void HttpRequest::trim(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);
}
