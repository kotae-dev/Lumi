#pragma once
#include <cstdint>

// Compute TOTP code.
// Returns the numeric code (e.g. 123456 for a 6-digit code).
uint32_t compute_totp(
    const uint8_t* secret, int secret_len,
    int64_t unix_time, int period,
    int digits, int algorithm   // 0=SHA1, 1=SHA256, 2=SHA512
);

// Compute HOTP code (counter-based).
uint32_t compute_hotp(
    const uint8_t* secret, int secret_len,
    uint64_t counter,
    int digits, int algorithm
);
