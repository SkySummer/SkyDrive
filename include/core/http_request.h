#ifndef CORE_HTTP_REQUEST_H
#define CORE_HTTP_REQUEST_H

#include <optional>
#include <string>
#include <unordered_map>

class HttpRequest {
public:
    HttpRequest() = default;
    explicit HttpRequest(const std::string& request);
    void parse(const std::string& request);

    [[nodiscard]] const std::string& method() const;
    [[nodiscard]] const std::string& path() const;
    [[nodiscard]] const std::string& version() const;
    [[nodiscard]] const std::unordered_map<std::string, std::string>& headers() const;
    [[nodiscard]] const std::string& body() const;

    [[nodiscard]] std::optional<std::string> getHeader(const std::string& key) const;
    
    [[nodiscard]] std::optional<std::string> getBoundary() const;

    void clear();

private:
    std::string method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    static void trim(std::string& str);
};

#endif  // CORE_HTTP_REQUEST_H
