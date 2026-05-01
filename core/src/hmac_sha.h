#pragma once
#include <cstdint>
#include <cstddef>

// ─── HMAC Algorithm Enum ─────────────────────────────────────────
enum class HmacAlgorithm : int {
    SHA1   = 0,
    SHA256 = 1,
    SHA512 = 2
};

// Compute HMAC using OpenSSL.
// Returns the length of the HMAC output, or 0 on failure.
// `out` must be at least 64 bytes.
int hmac_compute(
    HmacAlgorithm algo,
    const uint8_t* key, int key_len,
    const uint8_t* data, int data_len,
    uint8_t* out
);
