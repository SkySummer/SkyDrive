#ifndef UTILS_MULTIPART_PARSER_H
#define UTILS_MULTIPART_PARSER_H

#include <string>
#include <vector>

struct FormField {
    std::string name;
    std::string value;
};

struct UploadedFile {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::string data;
};

class MultipartParser {
public:
    MultipartParser(std::string body, std::string boundary);

    void parse(const std::string& data);
    [[nodiscard]] const std::vector<FormField>& fields() const;
    [[nodiscard]] const std::vector<UploadedFile>& files() const;

private:
    void parse() const;

    std::string body_;
    std::string boundary_;

    mutable std::vector<FormField> fields_;
    mutable std::vector<UploadedFile> files_;

    static std::string getHeaderValue(const std::string& header, const std::string& key);

    static void trim(std::string& str);
};

#endif  // UTILS_MULTIPART_PARSER_H
