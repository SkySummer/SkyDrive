#ifndef CORE_STATIC_FILE_H
#define CORE_STATIC_FILE_H

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

// 前向声明
class Address;
class Logger;

class StaticFile {
public:
    explicit StaticFile(const std::filesystem::path& root, const std::string& static_dir, std::string drive_dir,
                        Logger* logger);

    [[nodiscard]] std::string serve(const std::string& path, const Address& info) const;

private:
    const std::filesystem::path static_path_;     // 静态文件目录
    const std::filesystem::path templates_path_;  // 模板文件目录
    const std::string drive_url_;                 // 网盘文件目录 URL
    const std::filesystem::path drive_path_;      // 网盘文件目录
    Logger* logger_;                              // 日志

    mutable std::unordered_map<std::filesystem::path, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;

    [[nodiscard]] bool isPathSafe(const std::filesystem::path& path) const;

    [[nodiscard]] bool isDrivePath(const std::string& path) const;

    [[nodiscard]] std::filesystem::path getFilePath(const std::string& path) const;

    [[nodiscard]] std::optional<HttpResponse> readFromCache(const std::filesystem::path& path,
                                                            const Address& info) const;

    [[nodiscard]] std::string generateDirectoryListing(const std::filesystem::path& path,
                                                       const std::string& request_path) const;

    void updateCache(const std::filesystem::path& path, const HttpResponse& builder) const;
};

#endif  // CORE_STATIC_FILE_H
