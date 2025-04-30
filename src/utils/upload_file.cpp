#include "utils/upload_file.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>

#include "core/http_request.h"
#include "core/http_response.h"
#include "core/static_file.h"
#include "utils/logger.h"
#include "utils/multipart_parser.h"
#include "utils/url.h"

UploadFile::UploadFile(const HttpRequest& request, Logger* logger, StaticFile* static_file, const Address& info)
    : logger_(logger), static_file_(static_file), drive_path_(static_file->getDrivePath()) {
    response_ = handle(request, info);
}

HttpResponse UploadFile::process() const {
    return response_;
}

std::optional<std::string> UploadFile::checkFilename(const std::string& name, const Address& info) const {
    // 检查文件名是否包含非法字符
    static constexpr std::string_view illegal_chars = "<>:\"/\\|?*";
    if (name.find_first_of(illegal_chars) != std::string::npos) {
        // 文件名包含非法字符
        logger_->log(LogLevel::DEBUG, info, std::format("Filename contains illegal characters: {}", name));
        return "文件名包含非法字符";
    }

    if (name.empty()) {
        // 文件名为空
        logger_->log(LogLevel::DEBUG, info, "Filename is empty.");
        return "文件名为空";
    }

    return std::nullopt;
}

bool UploadFile::checkPath(const std::filesystem::path& path, const Address& info) const {
    if (!weakly_canonical(path).string().starts_with(drive_path_.string())) {
        // 路径不安全
        logger_->log(LogLevel::DEBUG, info, "Path is not safe.");
        return false;
    }

    if (!exists(path)) {
        // 上传路径不存在，创建目录
        logger_->log(LogLevel::DEBUG, info, std::format("Creating upload directory: {}", path.string()));
        create_directories(path);
    }

    if (!is_directory(path)) {
        // 上传路径不是目录
        logger_->log(LogLevel::DEBUG, info, "Upload path is not a directory.");
        return false;
    }

    return true;
}

std::optional<std::string> UploadFile::checkFilePath(const std::filesystem::path& path, const Address& info) const {
    if (!weakly_canonical(path).string().starts_with(drive_path_.string())) {
        // 路径不安全
        logger_->log(LogLevel::DEBUG, info, std::format("Unsafe file path: {}", path.string()));
        return "路径不安全";
    }

    if (exists(path)) {
        // 文件已存在
        logger_->log(LogLevel::DEBUG, info, std::format("File already exists: {}", path.string()));
        return "文件已存在";
    }

    return std::nullopt;
}

HttpResponse UploadFile::handle(const HttpRequest& request, const Address& info) {
    const std::string boundary = request.getBoundary().value_or("");
    if (boundary.empty()) {
        // 没有边界，返回 400 错误
        logger_->log(LogLevel::INFO, info, "Missing boundary in Content-Type header.");
        constexpr int error_code = 400;
        return HttpResponse::responseError(error_code, "No boundary found in request.");
    }

    const MultipartParser parser(request.body(), boundary);
    const auto& files = parser.files();

    if (files.empty()) {
        // 没有文件，返回 400 错误
        logger_->log(LogLevel::INFO, info, "No files found in upload request.");
        constexpr int error_code = 400;
        return HttpResponse::responseError(error_code, "No files found in upload request.");
    }

    // 解析上传路径
    const std::string& path = request.path();
    const auto upload_path = static_file_->getFileInfo(Url::decode(path)).first.parent_path();
    logger_->log(LogLevel::DEBUG, info, std::format("Upload path: {}", upload_path.string()));

    if (!checkPath(upload_path, info)) {
        // 上传路径不安全，返回 403 错误
        constexpr int error_code = 403;
        return HttpResponse::responseError(error_code);
    }

    for (const auto& file : files) {
        const std::filesystem::path file_path = upload_path / file.filename;
        logger_->log(LogLevel::DEBUG, info, std::format("Uploading file: {}", file.filename));
        logger_->log(LogLevel::DEBUG, info, std::format("File path: {}", file_path.string()));

        if (const auto message = checkFilePath(file_path, info)) {
            failure_files_.emplace_back(file.filename, *message);
            continue;
        }

        if (const auto message = checkFilename(file.filename, info)) {
            failure_files_.emplace_back(file.filename, *message);
            continue;
        }

        if (!write(file_path, file.data, info)) {
            failure_files_.emplace_back(file.filename, "写入文件失败");
            continue;
        }

        logger_->log(LogLevel::INFO, info, std::format("File upload successful: {}", upload_path.string()));
        ++success_count_;
    }

    std::ranges::sort(failure_files_);
    return HttpResponse::responseAlert(buildMessage(), getLocation(path));
}

bool UploadFile::write(const std::filesystem::path& path, const std::string& data, const Address& info) const {
    try {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) {
            // 无法打开文件，上传失败
            logger_->log(LogLevel::ERROR, info, std::format("Failed to open file for writing: {}", path.string()));
            return false;
        }

        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
        ofs.close();
    } catch (const std::exception& e) {
        // 写入文件失败，上传失败
        logger_->log(LogLevel::ERROR, info, std::format("File upload failed: {}", e.what()));
        return false;
    }
    return true;
}

std::string UploadFile::buildMessage() const {
    std::string message = std::format("上传完成：{} 成功，{} 失败", success_count_, failure_files_.size());

    if (!failure_files_.empty()) {
        message += "\\n失败文件：";

        for (const auto& [filename, error] : failure_files_) {
            if (filename.empty()) {
                message += "\\n<空文件名>";
            } else {
                message += std::format("\\n{}", filename);
            }
            if (!error.empty()) {
                message += std::format(" - {}", error);
            }
        }
    }

    return message;
}

std::string UploadFile::getLocation(const std::string& path) {
    const size_t pos = path.find_last_of('/');
    return pos != std::string::npos ? path.substr(0, pos + 1) : "/";
}
