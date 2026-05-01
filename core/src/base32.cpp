#include "base32.h"
#include <cctype>
#include <algorithm>

static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static int base32_char_value(char c) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}

std::vector<uint8_t> base32_decode(const std::string& input) {
    std::vector<uint8_t> result;
    result.reserve(input.size() * 5 / 8);

    int buffer = 0;
    int bits_left = 0;

    for (char c : input) {
        // Skip padding and whitespace
        if (c == '=' || c == ' ' || c == '\n' || c == '\r') continue;

        int val = base32_char_value(c);
        if (val < 0) return {}; // Invalid character

        buffer = (buffer << 5) | val;
        bits_left += 5;

        if (bits_left >= 8) {
            bits_left -= 8;
            result.push_back(static_cast<uint8_t>((buffer >> bits_left) & 0xFF));
        }
    }

    return result;
}

std::string base32_encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len * 8 + 4) / 5);

    int buffer = 0;
    int bits_left = 0;

    for (size_t i = 0; i < len; i++) {
        buffer = (buffer << 8) | data[i];
        bits_left += 8;

        while (bits_left >= 5) {
            bits_left -= 5;
            result += BASE32_ALPHABET[(buffer >> bits_left) & 0x1F];
        }
    }

    if (bits_left > 0) {
        result += BASE32_ALPHABET[(buffer << (5 - bits_left)) & 0x1F];
    }

    // Add padding
    while (result.size() % 8 != 0) {
        result += '=';
    }

    return result;
}
