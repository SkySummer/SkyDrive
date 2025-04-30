#include "utils/multipart_parser.h"

#include <cstddef>
#include <sstream>
#include <utility>

MultipartParser::MultipartParser(std::string body, std::string boundary)
    : body_(std::move(body)), boundary_(std::move(boundary)) {
    parse();
}

void MultipartParser::parse() const {
    const std::string delimiter = "--" + boundary_;
    size_t pos = 0;

    while (true) {
        size_t start = body_.find(delimiter, pos);
        if (start == std::string::npos) {
            break;
        }
        start += delimiter.length();

        if (body_.substr(start, 2) == "\r\n") {
            start += 2;
        }

        const size_t next = body_.find(delimiter, start);
        if (next == std::string::npos) {
            break;
        }

        const std::string part = body_.substr(start, next - start);
        const size_t header_end = part.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            pos = next;
            continue;
        }

        const std::string headers = part.substr(0, header_end);
        const std::string content = part.substr(header_end + 4);

        // 处理 headers
        std::istringstream header_stream(headers);
        std::string disposition_line;
        std::getline(header_stream, disposition_line);

        if (disposition_line.starts_with("Content-Disposition")) {
            const std::string name = getHeaderValue(disposition_line, "name");

            if (const std::string filename = getHeaderValue(disposition_line, "filename"); !filename.empty()) {
                // 处理文件上传
                std::string content_type = "application/octet-stream";
                std::string line;
                while (std::getline(header_stream, line)) {
                    if (line.starts_with("Content-Type:")) {
                        content_type = line.substr(std::string("Content-Type:").length());
                        trim(content_type);
                        break;
                    }
                }

                files_.emplace_back(name, filename, content_type, content);
            } else {
                // 处理表单字段
                fields_.emplace_back(name, content);
            }
        }

        pos = next;
    }
}

std::string MultipartParser::getHeaderValue(const std::string& header, const std::string& key) {
    const std::string key_token = key + "=\"";
    size_t pos = header.find(key_token);

    if (pos == std::string::npos) {
        return "";
    }

    pos += key_token.length();
    const size_t end_pos = header.find('"', pos);
    if (end_pos == std::string::npos) {
        return "";
    }

    return header.substr(pos, end_pos - pos);
}

const std::vector<FormField>& MultipartParser::fields() const {
    return fields_;
}

const std::vector<UploadedFile>& MultipartParser::files() const {
    return files_;
}

void MultipartParser::trim(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t"));
    str.erase(str.find_last_not_of(" \t") + 1);
}
