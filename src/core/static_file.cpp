#include "core/static_file.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/http_request.h"
#include "core/http_response.h"
#include "user/session_manager.h"
#include "utils/cookie_parser.h"
#include "utils/logger.h"
#include "utils/mime_type.h"
#include "utils/url.h"

namespace {
    std::string formatSize(const std::uintmax_t bytes) {
        constexpr std::array<const char*, 5> units = {"B", "KB", "MB", "GB", "TB"};
        constexpr int base = 1024;
        auto size = static_cast<double>(bytes);
        size_t unit_index = 0;

        while (size >= base && unit_index < units.size() - 1) {
            size /= base;
            ++unit_index;
        }

        std::ostringstream oss;
        if (unit_index == 0) {
            oss << std::fixed << std::setprecision(0);
        } else {
            oss << std::fixed << std::setprecision(2);
        }
        oss << size << " " << units.at(unit_index);
        return oss.str();
    }

    std::string formatTime(const std::filesystem::file_time_type file_time) {
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        const auto raw_time = std::chrono::system_clock::to_time_t(sctp);
        const std::tm local_time = *std::localtime(&raw_time);

        std::ostringstream oss;
        oss << std::put_time(&local_time, "%Y-%m-%d %H:%M");
        return oss.str();
    }

    std::string htmlEscape(const std::string& str) {
        std::string escaped;
        for (const char chr : str) {
            switch (chr) {
                case '&':
                    escaped += "&amp;";
                    break;
                case '<':
                    escaped += "&lt;";
                    break;
                case '>':
                    escaped += "&gt;";
                    break;
                case '"':
                    escaped += "&quot;";
                    break;
                case '\'':
                    escaped += "&#39;";
                    break;
                default:
                    escaped += chr;
            }
        }
        return escaped;
    }

    std::string ensureTrailingSlash(const std::string& path) {
        return path.ends_with('/') ? path : path + '/';
    }
}  // namespace

StaticFile::StaticFile(const std::filesystem::path& root, const std::string& static_dir, std::string drive_dir,
                       Logger* logger, SessionManager* session_manager)
    : static_path_(weakly_canonical(root / static_dir)),
      templates_path_(weakly_canonical(root / "templates")),
      drive_url_(std::move(drive_dir)),
      drive_path_(weakly_canonical(root / "data/files")),
      logger_(logger),
      session_manager_(session_manager) {
    logger_->log(LogLevel::INFO, "StaticFile initialized");
    logger_->log(LogLevel::INFO, std::format("-- staticfile_path: {}", static_path_.string()));
    logger_->log(LogLevel::INFO, std::format("-- templates_path: {}", templates_path_.string()));
    logger_->log(LogLevel::INFO, std::format("-- drive_url: {}", drive_url_));
    logger_->log(LogLevel::INFO, std::format("-- drive_path: {}", drive_path_.string()));
}

std::string StaticFile::getDriveUrl() const {
    return drive_url_;
}

std::filesystem::path StaticFile::getDrivePath() const {
    return drive_path_;
}

HttpResponse StaticFile::serve(const HttpRequest& request, const Address& info) const {
    auto page_type = PageType::NORMAL;

    auto raw = serveRaw(request, info, page_type);

    if (raw.getContentType().starts_with("text/html")) {
        // 如果是 HTML 文件，则渲染模板
        return render(std::move(raw), request, page_type);
    }

    // 否则直接返回
    return raw;
}

HttpResponse StaticFile::serveRaw(const HttpRequest& request, const Address& info, PageType& page_type) const {
    const std::string& path = request.path();
    const std::string decoded_path = Url::decode(path);
    const auto [full_path, type] = getFileInfo(decoded_path);
    page_type = type;

    logger_->log(LogLevel::DEBUG, info, std::format("Request for static file: {}", full_path.string()));

    if (!isPathSafe(full_path)) {
        // 路径不安全，返回 403
        logger_->log(LogLevel::DEBUG, info, "Path is not safe, return 403.");
        constexpr int error_code = 403;
        return HttpResponse::responseError(error_code);
    }

    if (isDriveUrl(decoded_path) && is_directory(full_path)) {
        // 如果请求的路径没有以斜杠结尾，则重定向到目录
        if (!path.ends_with('/')) {
            std::string location = path + '/';
            logger_->log(LogLevel::INFO, info,
                         std::format("Redirecting to directory with trailing slash: {} -> {}", path, location));

            constexpr int redirect_code = 302;
            return HttpResponse::responseRedirect(redirect_code, location);
        }

        // 如果请求的路径以斜杠结尾，则返回目录列表
        if (path.ends_with('/')) {
            // 获取相对于 files 的路径
            std::string virtual_path = path.substr(std::string(drive_url_).length() + 1);
            virtual_path = ensureTrailingSlash(virtual_path);

            // 生成网盘目录列表
            logger_->log(LogLevel::DEBUG, info, std::format("Serving directory listing for: {}", full_path.string()));
            return generateDirectoryListing(full_path, virtual_path);
        }
    }

    if (is_directory(full_path)) {
        // 如果请求的路径是目录，则返回 404
        logger_->log(LogLevel::DEBUG, info, "Requested path is a directory, return 404.");
        constexpr int error_code = 404;
        return HttpResponse::responseError(error_code);
    }

    if (const auto cached = readFromCache(full_path, info)) {
        // 从缓存中取文件
        logger_->log(LogLevel::DEBUG, info, "Static file served from cache.");
        return *cached;
    }

    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) {
        // 找不到文件，返回 404
        logger_->log(LogLevel::DEBUG, info, "Static file not found, return 404.");
        constexpr int error_code = 404;
        return HttpResponse::responseError(error_code);
    }

    std::ostringstream oss;
    oss << file.rdbuf();

    HttpResponse builder;
    builder.setStatus("200 OK").setContentType(MimeType::get(full_path)).setBody(oss.str());

    // 存入缓存
    updateCache(full_path, builder);
    logger_->log(LogLevel::DEBUG, info, "Static file loaded and cached.");

    return builder;
}

HttpResponse StaticFile::render(HttpResponse builder, const HttpRequest& request, const PageType page_type) const {
    builder.renderTemplate("footer", getTemplate("footer.html").value_or(""));

    if (page_type == PageType::AUTH) {
        // 认证页面
        return builder.renderTemplate("header-auth", getTemplate("header-auth.html").value_or(""));
    }

    const auto session_id = CookieParser::get(request, "session_id");
    if (!session_id) {
        // 未登录
        return builder.renderTemplate("header", getTemplate("header-guest.html").value_or(""));
    }

    const auto username = session_manager_->getUsername(*session_id);
    if (!username) {
        // 会话过期
        return builder.addHeader("Set-Cookie", "session_id=; Path=/; HttpOnly; Max-Age=0")
            .renderTemplate("header", getTemplate("header-guest.html").value_or(""));
    }

    // 已登录
    return builder.renderTemplate("header", getTemplate("header-user.html").value_or(""))
        .renderTemplate("username", *username);
}

std::optional<std::string> StaticFile::getTemplate(const std::string& name) const {
    std::ifstream file(templates_path_ / name);
    if (!file.is_open()) {
        // 模板文件不存在
        logger_->log(LogLevel::ERROR, std::format("Template file missing: {}", name));
        return std::nullopt;
    }

    logger_->log(LogLevel::DEBUG, std::format("Loading template: {}", name));
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

HttpResponse StaticFile::generateDirectoryListing(const std::filesystem::path& path,
                                                  const std::string& request_path) const {
    std::vector<std::filesystem::directory_entry> directories;
    std::vector<std::filesystem::directory_entry> files;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            directories.emplace_back(entry);
        } else {
            files.emplace_back(entry);
        }
    }

    auto filename_less = [](const auto& lhs_entry, const auto& rhs_entry) {
        return lhs_entry.path().filename().string() < rhs_entry.path().filename().string();
    };
    std::ranges::sort(directories, filename_less);
    std::ranges::sort(files, filename_less);

    std::ostringstream entries;

    // 返回上级
    if (request_path != "/") {
        entries << R"(
        <tr>
            <td><a href="../">⬅️ ../</a></td>
            <td>-</td>
            <td>-</td>
            <td>-</td>
        </tr>)";
    }

    std::string base_path = '/' + drive_url_ + request_path;
    base_path = ensureTrailingSlash(base_path);

    // 目录
    for (const auto& dir : directories) {
        const std::string name = htmlEscape(dir.path().filename().string());
        const std::string href = (std::filesystem::path(base_path) / Url::encode(name)).string() + '/';
        const std::string time = formatTime(last_write_time(dir));

        entries << std::format(R"(
        <tr>
            <td><a href="{}">📁 {}/</a></td>
            <td>-</td>
            <td>{}</td>
            <td>-</td>
        </tr>)",
                               href, name, time);
    }

    // 文件
    for (const auto& file : files) {
        const std::string name = htmlEscape(file.path().filename().string());
        const std::string href = (std::filesystem::path(base_path) / Url::encode(name)).string();
        const std::string size = formatSize(file_size(file));
        const std::string time = formatTime(last_write_time(file));

        entries << std::format(R"(
        <tr>
            <td><a href="{}">📄 {}</a></td>
            <td>{}</td>
            <td>{}</td>
            <td><a href="{}" download>下载</a></td>
        </tr>)",
                               href, name, size, time, href);
    }

    auto temp = getTemplate("directory-listing.html");
    if (!temp) {
        // 模板文件不存在，返回 500 错误
        constexpr int error_code = 500;
        return HttpResponse::responseError(error_code);
    }

    const std::string& html = *temp;

    return HttpResponse{}
        .setStatus("200 OK")
        .setContentType("text/html; charset=UTF-8")
        .setBody(html)
        .renderTemplate("path", Url::decode(request_path))
        .renderTemplate("entries", entries.str());
}

bool StaticFile::isPathSafe(const std::filesystem::path& path) const {
    return weakly_canonical(path).string().starts_with(static_path_.string()) ||
           weakly_canonical(path).string().starts_with(drive_path_.string());
}

bool StaticFile::isDriveUrl(const std::string& path) const {
    return path == '/' + drive_url_ || path.starts_with('/' + drive_url_ + '/');
}

std::pair<std::filesystem::path, PageType> StaticFile::getFileInfo(const std::string& path) const {
    if (isDriveUrl(path)) {
        if (path == '/' + drive_url_ || path == '/' + drive_url_ + '/') {
            // 网盘根目录
            return {weakly_canonical(drive_path_), PageType::DRIVE};
        }

        // 网盘路径
        const std::string drive_path = path.substr(drive_url_.length() + 2);
        return {weakly_canonical(drive_path_ / drive_path), PageType::DRIVE};
    }

    static const std::unordered_map<std::string, std::pair<std::string, PageType>> redirect_map = {
        {"/", {"index.html", PageType::INDEX}},
        {"/index", {"index.html", PageType::INDEX}},
        {"/index.htm", {"index.html", PageType::INDEX}},
        {"/index.html", {"index.html", PageType::INDEX}},
        {"/default.htm", {"index.html", PageType::INDEX}},
        {"/default.html", {"index.html", PageType::INDEX}},
        {"/login", {"login.html", PageType::AUTH}},
        {"/login.htm", {"login.html", PageType::AUTH}},
        {"/login.html", {"login.html", PageType::AUTH}},
        {"/register", {"register.html", PageType::AUTH}},
        {"/register.htm", {"register.html", PageType::AUTH}},
        {"/register.html", {"register.html", PageType::AUTH}},
        {"/reset-password", {"reset-password.html", PageType::AUTH}},
        {"/reset-password.htm", {"reset-password.html", PageType::AUTH}},
        {"/reset-password.html", {"reset-password.html", PageType::AUTH}},
    };

    // 重定向路径
    if (const auto redirect_iter = redirect_map.find(path); redirect_iter != redirect_map.end()) {
        const auto& [path, page_type] = redirect_iter->second;
        return {weakly_canonical(static_path_ / path), page_type};
    }

    // 静态文件路径
    const std::string clean_path = path.substr(1);
    return {weakly_canonical(static_path_ / clean_path), PageType::NORMAL};
}

std::optional<HttpResponse> StaticFile::readFromCache(const std::filesystem::path& path, const Address& info) const {
    std::lock_guard lock(cache_mutex_);
    const auto cache_iter = cache_.find(path);

    if (cache_iter == cache_.end()) {
        logger_->log(LogLevel::DEBUG, info, std::format("Cache miss: {}", path.string()));
        return std::nullopt;
    }

    if (!exists(path)) {
        logger_->log(LogLevel::DEBUG, info, std::format("Cache erase (file missing): {}", path.string()));
        cache_.erase(cache_iter);
        return std::nullopt;
    }

    if (cache_iter->second.last_modified != last_write_time(path)) {
        logger_->log(LogLevel::DEBUG, info, std::format("Cache stale: {}", path.string()));
        return std::nullopt;
    }

    logger_->log(LogLevel::DEBUG, info, std::format("Cache hit: {}", path.string()));
    return cache_iter->second.builder;
}

void StaticFile::updateCache(const std::filesystem::path& path, const HttpResponse& builder) const {
    try {
        CacheEntry entry = {.builder = builder, .last_modified = last_write_time(path)};

        std::lock_guard lock(cache_mutex_);
        cache_[path] = std::move(entry);
    } catch (const std::filesystem::filesystem_error& e) {
        // 极端文件丢失情况
        logger_->log(LogLevel::ERROR, std::format("updateCache failed: {} ({})", e.what(), path.string()));
    }
}
