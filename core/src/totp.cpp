#include "totp.h"
#include "hmac_sha.h"
#include <cstring>

// ─── Internal: compute HOTP (both TOTP and HOTP use this) ────────
static uint32_t compute_otp_internal(
    const uint8_t* secret, int secret_len,
    uint64_t counter,
    int digits, int algorithm)
{
    // Big-endian 8-byte counter
    uint8_t msg[8];
    for (int i = 7; i >= 0; i--) {
        msg[i] = static_cast<uint8_t>(counter & 0xFF);
        counter >>= 8;
    }

    uint8_t hmac[64];
    int hmac_len = hmac_compute(
        static_cast<HmacAlgorithm>(algorithm),
        secret, secret_len,
        msg, 8,
        hmac
    );

    if (hmac_len == 0) return 0;

    // Dynamic truncation (RFC 4226 Section 5.4)
    int offset = hmac[hmac_len - 1] & 0x0F;
    uint32_t bincode =
        (static_cast<uint32_t>(hmac[offset]     & 0x7F) << 24) |
        (static_cast<uint32_t>(hmac[offset + 1] & 0xFF) << 16) |
        (static_cast<uint32_t>(hmac[offset + 2] & 0xFF) <<  8) |
        (static_cast<uint32_t>(hmac[offset + 3] & 0xFF));

    static const uint32_t pow10[] = {
        1, 10, 100, 1000, 10000, 100000,
        1000000, 10000000, 100000000
    };

    return bincode % pow10[digits];
}

uint32_t compute_totp(
    const uint8_t* secret, int secret_len,
    int64_t unix_time, int period,
    int digits, int algorithm)
{
    if (period <= 0) period = 30;
    if (digits < 1 || digits > 8) digits = 6;

    uint64_t T = static_cast<uint64_t>(unix_time / period);
    return compute_otp_internal(secret, secret_len, T, digits, algorithm);
}

uint32_t compute_hotp(
    const uint8_t* secret, int secret_len,
    uint64_t counter,
    int digits, int algorithm)
{
    if (digits < 1 || digits > 8) digits = 6;
    return compute_otp_internal(secret, secret_len, counter, digits, algorithm);
}
