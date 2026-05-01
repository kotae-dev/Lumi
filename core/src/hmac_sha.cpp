#include "hmac_sha.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

// RAII helper for BCrypt handles
struct BcryptAlgHandle {
    BCRYPT_ALG_HANDLE h = nullptr;
    ~BcryptAlgHandle() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

struct BcryptHashHandle {
    BCRYPT_HASH_HANDLE h = nullptr;
    std::vector<uint8_t> obj;
    ~BcryptHashHandle() { if (h) BCryptDestroyHash(h); }
};

int hmac_compute(
    HmacAlgorithm algo,
    const uint8_t* key, int key_len,
    const uint8_t* data, int data_len,
    uint8_t* out)
{
    LPCWSTR alg_id = nullptr;
    int hash_len = 0;
    switch (algo) {
        case HmacAlgorithm::SHA1:   alg_id = BCRYPT_SHA1_ALGORITHM;   hash_len = 20; break;
        case HmacAlgorithm::SHA256: alg_id = BCRYPT_SHA256_ALGORITHM; hash_len = 32; break;
        case HmacAlgorithm::SHA512: alg_id = BCRYPT_SHA512_ALGORITHM; hash_len = 64; break;
        default: return 0;
    }

    BcryptAlgHandle alg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg.h, alg_id, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) return 0;

    // Query object length
    DWORD obj_len = 0, cb = 0;
    status = BCryptGetProperty(alg.h, BCRYPT_OBJECT_LENGTH, (PUCHAR)&obj_len, sizeof(obj_len), &cb, 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    BcryptHashHandle hash;
    hash.obj.resize(obj_len);

    status = BCryptCreateHash(alg.h, &hash.h, hash.obj.data(), obj_len,
                              const_cast<PUCHAR>(key), static_cast<ULONG>(key_len), 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    status = BCryptHashData(hash.h, const_cast<PUCHAR>(data), static_cast<ULONG>(data_len), 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    status = BCryptFinishHash(hash.h, out, static_cast<ULONG>(hash_len), 0);
    if (!BCRYPT_SUCCESS(status)) return 0;

    return hash_len;
}
