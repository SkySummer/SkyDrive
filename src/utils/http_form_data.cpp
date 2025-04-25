#include "utils/http_form_data.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

#include "core/http_response.h"
#include "utils/url.h"

HttpFormData::HttpFormData(const std::string& body) {
    data_ = parse(body);
}

std::unordered_map<std::string, std::string> HttpFormData::parse(const std::string& body) {
    std::unordered_map<std::string, std::string> result;
    std::istringstream iss(body);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        const auto pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);
            result[Url::decode(key)] = Url::decode(value);
        }
    }

    return result;
}

std::optional<std::string> HttpFormData::get(const std::string& key) const {
    auto iter = data_.find(key);
    if (iter != data_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

bool HttpFormData::contains(const std::string& key) const {
    return data_.contains(key);
}

const std::unordered_map<std::string, std::string>& HttpFormData::getData() const {
    return data_;
}

size_t HttpFormData::size() const {
    return data_.size();
}

bool HttpFormData::empty() const {
    return data_.empty();
}

bool HttpFormData::validateRequiredFields(const std::initializer_list<std::string>& required_fields) const {
    return std::ranges::all_of(required_fields, [this](const auto& field) { return contains(field); });
}

std::optional<std::string> HttpFormData::check(const std::initializer_list<std::string>& required_fields) const {
    constexpr int error_code = 400;
    if (empty()) {
        return HttpResponse::buildErrorResponse(error_code, "No form data received.");
    }
    if (!validateRequiredFields(required_fields)) {
        return HttpResponse::buildErrorResponse(error_code, "Missing required fields.");
    }
    if (size() != required_fields.size()) {
        return HttpResponse::buildErrorResponse(error_code, "Invalid form data.");
    }

    return std::nullopt;
}
