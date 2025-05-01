#ifndef CORE_HTTP_REQUEST_H
#define CORE_HTTP_REQUEST_H

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

class HttpRequest {
public:
    HttpRequest() = default;

    bool parseHeader(const std::string& raw);
    void parseBody(const std::string& raw);

    [[nodiscard]] size_t totalExpectedLength() const;

    [[nodiscard]] const std::string& method() const;
    [[nodiscard]] const std::string& path() const;
    [[nodiscard]] const std::string& version() const;
    [[nodiscard]] const std::unordered_map<std::string, std::string>& headers() const;
    [[nodiscard]] const std::string& body() const;

    [[nodiscard]] std::optional<std::string> getHeader(const std::string& key) const;

    [[nodiscard]] std::optional<std::string> getBoundary() const;

    [[nodiscard]] bool isHeaderParsed() const;

    void reset();

private:
    std::string method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    bool header_parsed_ = false;
    size_t header_end_pos_ = std::string::npos;
    size_t content_length_ = 0;

    static void trim(std::string& str);
};

#endif  // CORE_HTTP_REQUEST_H
