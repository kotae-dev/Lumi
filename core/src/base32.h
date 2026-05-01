#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Base32 decode (RFC 4648) — used for otpauth:// URI secret parameter.
// Returns decoded bytes, or empty vector on error.
std::vector<uint8_t> base32_decode(const std::string& input);

// Base32 encode (RFC 4648) — for display/export.
std::string base32_encode(const uint8_t* data, size_t len);
