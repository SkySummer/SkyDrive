#ifndef CORE_HTTP_RESPONSE_H
#define CORE_HTTP_RESPONSE_H

#include <map>
#include <string>

class HttpResponse {
public:
    HttpResponse& setStatus(const std::string& status);

    HttpResponse& setContentType(const std::string& type);

    HttpResponse& setBody(const std::string& body);

    HttpResponse& addHeader(const std::string& key, const std::string& value);

    [[nodiscard]] std::string build();

    [[nodiscard]] static HttpResponse responseError(int code, const std::string& tips = "");

    [[nodiscard]] static HttpResponse responseAlert(const std::string& message);
    [[nodiscard]] static HttpResponse responseAlert(const std::string& message, const std::string& location);

    [[nodiscard]] static HttpResponse responseRedirect(int code, const std::string& location);

private:
    std::string status_ = "200 OK";
    std::string body_;
    std::map<std::string, std::string> headers_;
};

#endif  // CORE_HTTP_RESPONSE_H
