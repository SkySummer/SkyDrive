#ifndef UTILS_BASE64_H
#define UTILS_BASE64_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

class Base64 {
public:
    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers, readability-identifier-length)
    [[nodiscard]] static std::string encode(const std::string& input) {
        std::string output;
        size_t i = 0;
        std::array<uint8_t, 3> array3{};
        std::array<uint8_t, 4> array4{};

        for (unsigned char c : input) {
            array3.at(i++) = c;
            if (i == 3) {
                array4[0] = (array3[0] & 0xfc) >> 2;
                array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
                array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
                array4[3] = array3[2] & 0x3f;

                for (i = 0; i < 4; i++) {
                    output += BASE64_CHARS.at(array4.at(i));
                }
                i = 0;
            }
        }

        if (i) {
            for (size_t j = i; j < 3; j++) {
                array3.at(j) = '\0';
            }

            array4[0] = (array3[0] & 0xfc) >> 2;
            array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
            array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
            array4[3] = array3[2] & 0x3f;

            for (size_t j = 0; j < i + 1; j++) {
                output += BASE64_CHARS.at(array4.at(j));
            }

            while (i++ < 3) {
                output += '=';
            }
        }

        return output;
    }

    [[nodiscard]] static std::string decode(const std::string& input) {
        size_t len = input.size();
        if (len % 4 != 0) {
            throw std::invalid_argument("Invalid base64 length");
        }

        std::string output;
        std::array<int, 4> array4{};
        std::array<int, 3> array3{};
        size_t i = 0;

        for (char c : input) {
            if (c == '=') {
                array4.at(i++) = 0;
            } else {
                int val = decodeChar(c);
                if (val == -1) {
                    continue;
                }
                array4.at(i++) = val;
            }

            if (i == 4) {
                array3[0] = (array4[0] << 2) + ((array4[1] & 0x30) >> 4);
                array3[1] = ((array4[1] & 0xf) << 4) + ((array4[2] & 0x3c) >> 2);
                array3[2] = ((array4[2] & 0x3) << 6) + array4[3];

                for (int j = 0; j < 3; j++) {
                    output += static_cast<char>(array3.at(j));
                }
                i = 0;
            }
        }

        if (!input.empty() && input[input.size() - 1] == '=') {
            if (input[input.size() - 2] == '=') {
                output.pop_back();
            }
            output.pop_back();
        }

        return output;
    }

private:
    static constexpr std::string_view BASE64_CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    [[nodiscard]] static int decodeChar(const unsigned char c) {
        if ('A' <= c && c <= 'Z') {
            return c - 'A';
        }
        if ('a' <= c && c <= 'z') {
            return c - 'a' + 26;
        }
        if ('0' <= c && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    };
    // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers, readability-identifier-length)
};

#endif  // UTILS_BASE64_H
