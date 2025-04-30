#ifndef UTILS_UPLOAD_FILE_H
#define UTILS_UPLOAD_FILE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/http_response.h"

// 前向声明
class Logger;
class HttpRequest;
class StaticFile;
class Address;

class UploadFile {
public:
    UploadFile(const HttpRequest& request, Logger* logger, StaticFile* static_file, const Address& info);

    [[nodiscard]] HttpResponse process() const;

private:
    Logger* logger_;
    StaticFile* static_file_;
    std::filesystem::path drive_path_;  // 网盘文件目录

    HttpResponse response_;
    std::vector<std::pair<std::string, std::string>> failure_files_;  // 失败文件列表
    size_t success_count_ = 0;                                        // 成功文件数量

    [[nodiscard]] HttpResponse handle(const HttpRequest& request, const Address& info);

    [[nodiscard]] std::optional<std::string> checkFilename(const std::string& name, const Address& info) const;
    [[nodiscard]] bool checkPath(const std::filesystem::path& path, const Address& info) const;
    [[nodiscard]] std::optional<std::string> checkFilePath(const std::filesystem::path& path,
                                                           const Address& info) const;

    [[nodiscard]] bool write(const std::filesystem::path& path, const std::string& data, const Address& info) const;

    [[nodiscard]] std::string buildMessage() const;
    [[nodiscard]] static std::string getLocation(const std::string& path);
};

#endif  // UTILS_UPLOAD_FILE_H
