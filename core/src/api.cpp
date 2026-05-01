// ─── Lumi Core API Implementation ────────────────────────────────
// This file implements all exported functions from lumi_core.h.

#include "lumi_core.h"
#include "totp.h"
#include "base32.h"
#include "vault.h"
#include "uuid.h"

#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ─── Thread-local error string ───────────────────────────────────
static thread_local char g_last_error[512] = {};

static void set_error(const char* msg) {
    strncpy_s(g_last_error, sizeof(g_last_error), msg, _TRUNCATE);
}

// ─── Global vault instance ───────────────────────────────────────
static Vault g_vault;

// ─── External functions from other modules ───────────────────────
extern std::string decode_qr_from_file(const std::filesystem::path& path);
extern std::vector<LumiAccount> parse_migration_uri(const std::string& uri);
extern int parse_single_otpauth_uri(const std::string& uri, LumiAccount* out);
extern int64_t get_current_time_with_ntp();

// ─── QR / Import ────────────────────────────────────────────────

LUMI_API int lumi_decode_qr_file(
    const char* image_path,
    LumiAccount** out_accounts)
{
    try {
        std::string uri = decode_qr_from_file(image_path);

        std::vector<LumiAccount> accounts;

        if (uri.find("otpauth-migration://") == 0) {
            accounts = parse_migration_uri(uri);
        } else if (uri.find("otpauth://") == 0) {
            LumiAccount acc{};
            if (parse_single_otpauth_uri(uri, &acc) == 0) {
                accounts.push_back(acc);
            } else {
                set_error("Failed to parse otpauth URI");
                return -1;
            }
        } else {
            set_error("QR code does not contain a recognized OTP URI");
            return -1;
        }

        if (accounts.empty()) {
            *out_accounts = nullptr;
            return 0;
        }

        // Allocate output array
        *out_accounts = static_cast<LumiAccount*>(
            malloc(sizeof(LumiAccount) * accounts.size()));
        if (!*out_accounts) {
            set_error("Memory allocation failed");
            return -1;
        }

        memcpy(*out_accounts, accounts.data(),
               sizeof(LumiAccount) * accounts.size());

        return static_cast<int>(accounts.size());

    } catch (const std::exception& e) {
        set_error(e.what());
        return -1;
    }
}

LUMI_API void lumi_free_accounts(LumiAccount* accounts, int count) {
    if (accounts) {
        // Zero secrets before freeing
        for (int i = 0; i < count; i++) {
            SecureZeroMemory(accounts[i].secret, sizeof(accounts[i].secret));
        }
        free(accounts);
    }
}

LUMI_API int lumi_parse_otpauth_uri(
    const char* uri,
    LumiAccount* out_account)
{
    try {
        return parse_single_otpauth_uri(std::string(uri), out_account);
    } catch (const std::exception& e) {
        set_error(e.what());
        return -1;
    }
}

// ─── TOTP ────────────────────────────────────────────────────────

LUMI_API int lumi_get_totp(
    const LumiAccount* account,
    char* code_out,
    int* seconds_remaining_out)
{
    if (!account || !code_out) {
        set_error("Null parameter");
        return -1;
    }

    int64_t now = get_current_time_with_ntp();
    int period = account->period > 0 ? account->period : 30;

    uint32_t code;
    if (account->type == 1) {
        // HOTP
        code = compute_hotp(
            account->secret, account->secret_len,
            account->hotp_counter,
            account->digits, account->algorithm);
    } else {
        // TOTP
        code = compute_totp(
            account->secret, account->secret_len,
            now, period,
            account->digits, account->algorithm);
    }

    // Format code with leading zeros
    int digits = account->digits > 0 ? account->digits : 6;
    snprintf(code_out, 9, "%0*u", digits, code);

    if (seconds_remaining_out) {
        *seconds_remaining_out = period - static_cast<int>(now % period);
    }

    return 0;
}

LUMI_API int64_t lumi_get_time(void) {
    return get_current_time_with_ntp();
}

// ─── Vault ───────────────────────────────────────────────────────

LUMI_API int lumi_vault_init(const char* vault_path) {
    try {
        if (!g_vault.open(vault_path)) {
            set_error("Failed to open vault");
            return -1;
        }
        return 0;
    } catch (const std::exception& e) {
        set_error(e.what());
        return -1;
    }
}

LUMI_API void lumi_vault_close(void) {
    g_vault.close();
}

LUMI_API int lumi_vault_get_all(LumiAccount** out_accounts, int* out_count) {
    try {
        auto accounts = g_vault.get_all();
        *out_count = static_cast<int>(accounts.size());

        if (accounts.empty()) {
            *out_accounts = nullptr;
            return 0;
        }

        *out_accounts = static_cast<LumiAccount*>(
            malloc(sizeof(LumiAccount) * accounts.size()));
        if (!*out_accounts) {
            set_error("Memory allocation failed");
            return -1;
        }

        memcpy(*out_accounts, accounts.data(),
               sizeof(LumiAccount) * accounts.size());
        return 0;

    } catch (const std::exception& e) {
        set_error(e.what());
        return -1;
    }
}

LUMI_API int lumi_vault_save_account(const LumiAccount* account) {
    if (!account) { set_error("Null account"); return -1; }
    return g_vault.save_account(*account) ? 0 : -1;
}

LUMI_API int lumi_vault_delete_account(const char* id) {
    if (!id) { set_error("Null id"); return -1; }
    return g_vault.delete_account(id) ? 0 : -1;
}

LUMI_API int lumi_vault_reorder(const char** id_array, int count) {
    std::vector<std::string> order;
    order.reserve(count);
    for (int i = 0; i < count; i++) {
        order.push_back(id_array[i]);
    }
    return g_vault.reorder(order) ? 0 : -1;
}

LUMI_API int lumi_vault_rename_account(const char* id, const char* new_name) {
    if (!id || !new_name) { set_error("Null parameter"); return -1; }
    return g_vault.rename_account(id, new_name) ? 0 : -1;
}

// ─── Error handling ──────────────────────────────────────────────

LUMI_API const char* lumi_last_error(void) {
    return g_last_error;
}
