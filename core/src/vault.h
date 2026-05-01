#pragma once
#include "lumi_core.h"
#include <string>
#include <vector>

// ─── Vault internal API ──────────────────────────────────────────
class Vault {
public:
    bool open(const std::string& path);
    void close();

    std::vector<LumiAccount> get_all() const;
    bool save_account(const LumiAccount& account);
    bool delete_account(const std::string& id);
    bool rename_account(const std::string& id, const std::string& new_name);
    bool reorder(const std::vector<std::string>& id_order);

private:
    std::string m_path;
    std::vector<LumiAccount> m_accounts;
    std::vector<uint8_t> m_aes_key; // 32 bytes AES-256

    bool load();
    bool save();

    // Crypto helpers
    bool generate_key();
    bool seal_key(const std::vector<uint8_t>& key, std::vector<uint8_t>& sealed_out);
    bool unseal_key(const std::vector<uint8_t>& sealed, std::vector<uint8_t>& key_out);
    bool encrypt_data(const std::vector<uint8_t>& plaintext,
                      const std::vector<uint8_t>& key,
                      std::vector<uint8_t>& iv_out,
                      std::vector<uint8_t>& tag_out,
                      std::vector<uint8_t>& ciphertext_out);
    bool decrypt_data(const std::vector<uint8_t>& ciphertext,
                      const std::vector<uint8_t>& key,
                      const std::vector<uint8_t>& iv,
                      const std::vector<uint8_t>& tag,
                      std::vector<uint8_t>& plaintext_out);
};
