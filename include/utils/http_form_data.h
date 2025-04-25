#ifndef UTILS_HTTP_FORM_DATA_H
#define UTILS_HTTP_FORM_DATA_H

#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>

class HttpFormData {
public:
    HttpFormData() = default;

    explicit HttpFormData(const std::string& body);

    [[nodiscard]] static std::unordered_map<std::string, std::string> parse(const std::string& body);

    [[nodiscard]] std::optional<std::string> get(const std::string& key) const;

    bool contains(const std::string& key) const;

    [[nodiscard]] const std::unordered_map<std::string, std::string>& getData() const;

    [[nodiscard]] size_t size() const;

    [[nodiscard]] bool empty() const;

    [[nodiscard]] bool validateRequiredFields(const std::initializer_list<std::string>& required_fields) const;

    [[nodiscard]] std::optional<std::string> check(const std::initializer_list<std::string>& required_fields) const;

private:
    std::unordered_map<std::string, std::string> data_;
};

#endif  // UTILS_HTTP_FORM_DATA_H
