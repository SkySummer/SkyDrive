#ifndef UTILS_HASH_H
#define UTILS_HASH_H

#include <cstddef>
#include <random>
#include <sstream>
#include <string>

#include "utils/sha256.h"

class Hash {
public:
    [[nodiscard]] static std::string sha256(const std::string& input) { return SHA256::hash(input); }

    [[nodiscard]] static std::string randomSalt(size_t length = DEFAULT_SALT_LENGTH) {
        static constexpr uint8_t max = 255;
        static std::random_device random_device;
        static std::mt19937 gen(random_device());
        static std::uniform_int_distribution<> dis(0, max);

        std::ostringstream oss;
        for (size_t i = 0; i < length; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
        }
        return oss.str();
    }

    [[nodiscard]] static std::string saltedHash(const std::string& salt, const std::string& password) {
        return sha256(salt + password);
    }

private:
    static constexpr size_t DEFAULT_SALT_LENGTH = 16;
};

#endif  // UTILS_HASH_H
