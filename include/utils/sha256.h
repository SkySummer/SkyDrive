#ifndef UTILS_SHA256_H
#define UTILS_SHA256_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

class SHA256 {
public:
    [[nodiscard]] static std::string hash(const std::string& input) {
        SHA256 sha;
        sha.update(input);
        sha.final();
        return sha.toString();
    }

private:
    // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers, readability-identifier-length)
    void update(const std::string& input) {
        for (unsigned char c : input) {
            data_buffer_.at(data_len_++) = c;
            if (data_len_ == 64) {
                transform(data_buffer_);
                total_len_ += 512;
                data_len_ = 0;
            }
        }
    }

    void final() {
        total_len_ += data_len_ * 8;

        data_buffer_.at(data_len_++) = 0x80;
        if (data_len_ > 56) {
            while (data_len_ < 64) {
                data_buffer_.at(data_len_++) = 0x00;
            }
            transform(data_buffer_);
            data_len_ = 0;
        }

        while (data_len_ < 56) {
            data_buffer_.at(data_len_++) = 0x00;
        }

        for (int i = 7; i >= 0; --i) {
            data_buffer_.at(data_len_++) = (total_len_ >> (i * 8)) & 0xff;
        }

        transform(data_buffer_);
    }

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        for (auto val : state_) {
            oss << std::hex << std::setfill('0') << std::setw(8) << val;
        }
        return oss.str();
    }

    void transform(const std::array<uint8_t, 64>& data) {
        static constexpr std::array<uint32_t, 64> k = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

        std::array<uint32_t, 64> w{};
        for (size_t i = 0; i < 16; ++i) {
            w.at(i) = (data.at(i * 4) << 24) | (data.at((i * 4) + 1) << 16) | (data.at((i * 4) + 2) << 8) |
                      data.at((i * 4) + 3);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w.at(i - 15), 7) ^ rotr(w.at(i - 15), 18) ^ (w.at(i - 15) >> 3);
            uint32_t s1 = rotr(w.at(i - 2), 17) ^ rotr(w.at(i - 2), 19) ^ (w.at(i - 2) >> 10);
            w.at(i) = w.at(i - 16) + s0 + w.at(i - 7) + s1;
        }

        uint32_t a = state_[0];
        uint32_t b = state_[1];
        uint32_t c = state_[2];
        uint32_t d = state_[3];
        uint32_t e = state_[4];
        uint32_t f = state_[5];
        uint32_t g = state_[6];
        uint32_t h = state_[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + s1 + ch + k.at(i) + w.at(i);
            uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

    std::array<uint8_t, 64> data_buffer_{};
    size_t data_len_ = 0;
    uint64_t total_len_ = 0;
    std::array<uint32_t, 8> state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers, readability-identifier-length)
};

#endif  // UTILS_SHA256_H
