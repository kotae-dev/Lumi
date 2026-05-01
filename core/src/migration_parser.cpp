#include "lumi_core.h"
#include "base32.h"
#include "uuid.h"
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <stdexcept>

// ─── URL decode ──────────────────────────────────────────────────
static std::string url_decode(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '%' && i + 2 < input.size()) {
            char hex[3] = { input[i + 1], input[i + 2], 0 };
            result += static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        } else if (input[i] == '+') {
            result += ' ';
        } else {
            result += input[i];
        }
    }
    return result;
}

// ─── Base64 decode ───────────────────────────────────────────────
static const int B64_DECODE_TABLE[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static std::vector<uint8_t> base64_decode(const std::string& input) {
    std::vector<uint8_t> result;
    result.reserve(input.size() * 3 / 4);

    int val = 0;
    int bits = -8;

    for (unsigned char c : input) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
        int d = B64_DECODE_TABLE[c];
        if (d < 0) continue;

        val = (val << 6) | d;
        bits += 6;

        if (bits >= 0) {
            result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    return result;
}

// ─── Extract query parameter ─────────────────────────────────────
static std::string extract_query_param(const std::string& uri, const std::string& key) {
    std::string search = key + "=";
    auto pos = uri.find(search);
    if (pos == std::string::npos) return "";

    pos += search.size();
    auto end = uri.find('&', pos);
    if (end == std::string::npos) end = uri.size();

    return uri.substr(pos, end - pos);
}

// ─── Simple protobuf wire-format decoder (no nanopb dependency) ──
// This is a lightweight protobuf decoder for the MigrationPayload
// message, avoiding the nanopb code-generation step.

struct OtpParam {
    std::vector<uint8_t> secret;
    std::string name;
    std::string issuer;
    int algorithm = 0;
    int digit_count = 0;
    int otp_type = 0;
    int64_t counter = 0;
};

static bool read_varint(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
    value = 0;
    int shift = 0;
    while (p < end) {
        uint8_t b = *p++;
        value |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
        shift += 7;
        if (shift >= 64) return false;
    }
    return false;
}

static OtpParam parse_otp_parameters(const uint8_t* data, size_t len) {
    OtpParam param;
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    while (p < end) {
        uint64_t tag;
        if (!read_varint(p, end, tag)) break;

        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint32_t wire = static_cast<uint32_t>(tag & 0x07);

        if (wire == 0) { // Varint
            uint64_t val;
            if (!read_varint(p, end, val)) break;
            switch (field) {
                case 4: param.algorithm = static_cast<int>(val); break;
                case 5: param.digit_count = static_cast<int>(val); break;
                case 6: param.otp_type = static_cast<int>(val); break;
                case 7: param.counter = static_cast<int64_t>(val); break;
                default: break;
            }
        } else if (wire == 2) { // Length-delimited
            uint64_t length;
            if (!read_varint(p, end, length)) break;
            if (p + length > end) break;

            switch (field) {
                case 1: // secret (bytes)
                    param.secret.assign(p, p + length);
                    break;
                case 2: // name (string)
                    param.name.assign(reinterpret_cast<const char*>(p), length);
                    break;
                case 3: // issuer (string)
                    param.issuer.assign(reinterpret_cast<const char*>(p), length);
                    break;
                default: break;
            }
            p += length;
        } else {
            break; // Unsupported wire type
        }
    }

    return param;
}

static std::vector<OtpParam> parse_migration_payload(const uint8_t* data, size_t len) {
    std::vector<OtpParam> params;
    const uint8_t* p = data;
    const uint8_t* end = data + len;

    while (p < end) {
        uint64_t tag;
        if (!read_varint(p, end, tag)) break;

        uint32_t field = static_cast<uint32_t>(tag >> 3);
        uint32_t wire = static_cast<uint32_t>(tag & 0x07);

        if (wire == 0) { // Varint (version, batch_size, etc.)
            uint64_t val;
            if (!read_varint(p, end, val)) break;
            // Fields 2-5 are version/batch metadata — we don't need them
        } else if (wire == 2) { // Length-delimited
            uint64_t length;
            if (!read_varint(p, end, length)) break;
            if (p + length > end) break;

            if (field == 1) { // otp_parameters (repeated)
                params.push_back(parse_otp_parameters(p, static_cast<size_t>(length)));
            }
            p += length;
        } else {
            break;
        }
    }

    return params;
}

// ─── Map protobuf enums to LumiAccount fields ───────────────────
static int map_algorithm(int proto_algo) {
    switch (proto_algo) {
        case 1: return 0;  // SHA1
        case 2: return 1;  // SHA256
        case 3: return 2;  // SHA512
        default: return 0; // Default to SHA1
    }
}

static int map_digits(int proto_digits) {
    switch (proto_digits) {
        case 2: return 8;  // EIGHT
        default: return 6; // SIX or unspecified
    }
}

// ─── Public API: parse migration URI ─────────────────────────────
std::vector<LumiAccount> parse_migration_uri(const std::string& uri) {
    auto data_encoded = extract_query_param(uri, "data");
    auto data_b64 = url_decode(data_encoded);
    auto proto_bytes = base64_decode(data_b64);

    if (proto_bytes.empty()) {
        throw std::runtime_error("Failed to decode migration data");
    }

    auto params = parse_migration_payload(proto_bytes.data(), proto_bytes.size());

    std::vector<LumiAccount> accounts;
    accounts.reserve(params.size());

    for (auto& p : params) {
        LumiAccount acc{};
        generate_uuid_v4(acc.id);

        // Copy name and issuer
        strncpy_s(acc.name, sizeof(acc.name), p.name.c_str(), _TRUNCATE);
        strncpy_s(acc.issuer, sizeof(acc.issuer), p.issuer.c_str(), _TRUNCATE);

        // Copy secret
        size_t copy_len = (std::min)(p.secret.size(), static_cast<size_t>(64));
        memcpy(acc.secret, p.secret.data(), copy_len);
        acc.secret_len = static_cast<int>(copy_len);

        acc.algorithm = map_algorithm(p.algorithm);
        acc.digits = map_digits(p.digit_count);
        acc.period = 30;
        acc.type = (p.otp_type == 1) ? 1 : 0; // 1=HOTP, default TOTP
        acc.hotp_counter = static_cast<uint64_t>(p.counter);

        accounts.push_back(acc);
    }

    return accounts;
}

// ─── Public API: parse single otpauth:// URI ─────────────────────
int parse_single_otpauth_uri(const std::string& uri, LumiAccount* out) {
    if (uri.find("otpauth://") != 0) return -1;

    // Determine type
    bool is_hotp = (uri.find("otpauth://hotp/") == 0);
    bool is_totp = (uri.find("otpauth://totp/") == 0);
    if (!is_hotp && !is_totp) return -1;

    memset(out, 0, sizeof(LumiAccount));
    generate_uuid_v4(out->id);
    out->type = is_hotp ? 1 : 0;
    out->digits = 6;
    out->period = 30;
    out->algorithm = 0; // SHA1 default

    // Extract label (between type/ and ?)
    size_t label_start = uri.find('/', 15) + 1;
    if (label_start == std::string::npos + 1) label_start = 15;
    size_t label_end = uri.find('?', label_start);
    if (label_end == std::string::npos) label_end = uri.size();

    std::string label = url_decode(uri.substr(label_start, label_end - label_start));

    // Split label by ':' for issuer:account
    auto colon = label.find(':');
    if (colon != std::string::npos) {
        std::string issuer_part = label.substr(0, colon);
        std::string account_part = label.substr(colon + 1);
        // Trim leading space from account
        if (!account_part.empty() && account_part[0] == ' ')
            account_part = account_part.substr(1);
        strncpy_s(out->issuer, sizeof(out->issuer), issuer_part.c_str(), _TRUNCATE);
        strncpy_s(out->name, sizeof(out->name), account_part.c_str(), _TRUNCATE);
    } else {
        strncpy_s(out->name, sizeof(out->name), label.c_str(), _TRUNCATE);
    }

    // Parse query parameters
    std::string query = (label_end < uri.size()) ? uri.substr(label_end + 1) : "";

    // Extract secret (Base32)
    std::string secret_b32 = extract_query_param(query, "secret");
    if (!secret_b32.empty()) {
        auto secret_bytes = base32_decode(secret_b32);
        size_t copy_len = (std::min)(secret_bytes.size(), static_cast<size_t>(64));
        memcpy(out->secret, secret_bytes.data(), copy_len);
        out->secret_len = static_cast<int>(copy_len);
    }

    // Extract issuer (overrides label issuer if present)
    std::string issuer_param = extract_query_param(query, "issuer");
    if (!issuer_param.empty()) {
        std::string decoded = url_decode(issuer_param);
        strncpy_s(out->issuer, sizeof(out->issuer), decoded.c_str(), _TRUNCATE);
    }

    // Extract algorithm
    std::string algo = extract_query_param(query, "algorithm");
    if (algo == "SHA256") out->algorithm = 1;
    else if (algo == "SHA512") out->algorithm = 2;

    // Extract digits
    std::string digits_str = extract_query_param(query, "digits");
    if (digits_str == "8") out->digits = 8;

    // Extract period
    std::string period_str = extract_query_param(query, "period");
    if (!period_str.empty()) {
        int p = atoi(period_str.c_str());
        if (p > 0) out->period = p;
    }

    // Extract counter (HOTP)
    std::string counter_str = extract_query_param(query, "counter");
    if (!counter_str.empty()) {
        out->hotp_counter = static_cast<uint64_t>(atoll(counter_str.c_str()));
    }

    return 0;
}
