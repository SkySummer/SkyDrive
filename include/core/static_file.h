#ifndef CORE_STATIC_FILE_H
#define CORE_STATIC_FILE_H

#include <cctype>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/http_response.h"

struct CacheEntry {
    HttpResponse builder{};                         // 文件内容
    std::filesystem::file_time_type last_modified;  // 最后修改时间
};

enum class PageType : std::uint8_t {
    INDEX,   // 首页
    AUTH,    // 认证页面
    DRIVE,   // 网盘页面
    NORMAL,  // 普通页面
};

// 前向声明
class Address;
class Logger;
class HttpRequest;
class SessionManager;

class StaticFile {
public:
    explicit StaticFile(const std::filesystem::path& root, const std::string& static_dir, std::string drive_dir,
                        Logger* logger, SessionManager* session_manager);

    [[nodiscard]] HttpResponse serve(const HttpRequest& request, const Address& info) const;

    [[nodiscard]] std::string getDriveUrl() const;
    [[nodiscard]] std::filesystem::path getDrivePath() const;

    [[nodiscard]] bool isDriveUrl(const std::string& path) const;

    [[nodiscard]] std::pair<std::filesystem::path, PageType> getFileInfo(const std::string& path) const;

private:
    const std::filesystem::path static_path_;     // 静态文件目录
    const std::filesystem::path templates_path_;  // 模板文件目录
    const std::string drive_url_;                 // 网盘文件目录 URL
    const std::filesystem::path drive_path_;      // 网盘文件目录
    Logger* logger_;                              // 日志
    SessionManager* session_manager_;             // 会话管理器

    mutable std::unordered_map<std::filesystem::path, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;

    [[nodiscard]] HttpResponse serveRaw(const HttpRequest& request, const Address& info, PageType& page_type) const;

    [[nodiscard]] bool isPathSafe(const std::filesystem::path& path) const;
    [[nodiscard]] static bool isNameSafe(const std::string& name);

    [[nodiscard]] std::optional<HttpResponse> readFromCache(const std::filesystem::path& path,
                                                            const Address& info) const;

    [[nodiscard]] HttpResponse generateDirectoryListing(const std::filesystem::path& path,
                                                        const std::string& request_path) const;

    void updateCache(const std::filesystem::path& path, const HttpResponse& builder) const;

    [[nodiscard]] HttpResponse render(HttpResponse builder, const HttpRequest& request, PageType page_type) const;

    [[nodiscard]] std::optional<std::string> getTemplate(const std::string& name) const;
};

#endif  // CORE_STATIC_FILE_H
