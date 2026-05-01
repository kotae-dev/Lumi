#include "uuid.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

void generate_uuid_v4(char* out) {
    uint8_t bytes[16];

    // Use Windows cryptographic RNG
    NTSTATUS status = BCryptGenRandom(
        nullptr, bytes, sizeof(bytes),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (status != 0) {
        // Fallback: zero UUID (should never happen)
        memset(out, '0', 36);
        out[8] = out[13] = out[18] = out[23] = '-';
        out[36] = '\0';
        return;
    }

    // Set version 4 (bits 12-15 of time_hi_and_version)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    // Set variant bits (10xx for RFC 4122)
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
    );
}

std::string generate_uuid_v4_string() {
    char buf[37];
    generate_uuid_v4(buf);
    return std::string(buf);
}
