#pragma once

#ifdef LUMI_EXPORTS
  #define LUMI_API __declspec(dllexport)
#else
  #define LUMI_API __declspec(dllimport)
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Account structure ──────────────────────────────────────────
typedef struct {
    char     id[37];          // UUID v4, null-terminated
    char     name[256];       // User-visible label
    char     issuer[256];     // e.g. "Google", "GitHub"
    uint8_t  secret[64];      // Raw binary secret
    int      secret_len;
    int      algorithm;       // 0=SHA1, 1=SHA256, 2=SHA512
    int      digits;          // 6 or 8
    int      period;          // 30 or 60 seconds
    int      type;            // 0=TOTP, 1=HOTP
    uint64_t hotp_counter;
} LumiAccount;

// ─── QR / Import ────────────────────────────────────────────────
// Decode a QR image file. Returns number of accounts found, or -1 on error.
// Caller owns `out_accounts` array; call lumi_free_accounts when done.
LUMI_API int  lumi_decode_qr_file(
    const char*   image_path,
    LumiAccount** out_accounts
);
LUMI_API void lumi_free_accounts(LumiAccount* accounts, int count);

// Parse a single otpauth:// URI
// Returns 0 on success, -1 on error.
LUMI_API int  lumi_parse_otpauth_uri(
    const char*  uri,
    LumiAccount* out_account
);

// ─── TOTP ────────────────────────────────────────────────────────
// Returns 0 on success. code_out: null-terminated string "123456"
LUMI_API int  lumi_get_totp(
    const LumiAccount* account,
    char*              code_out,         // at least 9 bytes
    int*               seconds_remaining_out
);

// Get current Unix time (with optional NTP correction)
LUMI_API int64_t lumi_get_time(void);

// ─── Vault ───────────────────────────────────────────────────────
// Initialize vault (creates or opens existing vault file)
LUMI_API int  lumi_vault_init(const char* vault_path);
LUMI_API void lumi_vault_close(void);

LUMI_API int  lumi_vault_get_all(LumiAccount** out_accounts, int* out_count);
LUMI_API int  lumi_vault_save_account(const LumiAccount* account);   // insert or update by id
LUMI_API int  lumi_vault_delete_account(const char* id);
LUMI_API int  lumi_vault_reorder(const char** id_array, int count);
LUMI_API int  lumi_vault_rename_account(const char* id, const char* new_name);

// ─── Error handling ──────────────────────────────────────────────
LUMI_API const char* lumi_last_error(void);   // Thread-local error string

#ifdef __cplusplus
}
#endif
