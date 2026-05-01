#include "vault.h"
#include "uuid.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

static constexpr uint8_t VAULT_MAGIC[4] = { 'L', 'U', 'M', 'I' };
static constexpr uint32_t VAULT_VERSION = 1;

// ─── JSON helpers ─────────────────────────────────────────────────

static std::string escape_json(const std::string& s) {
    std::string r; r.reserve(s.size()+8);
    for (char c : s) {
        if (c=='"') r+="\\\""; else if (c=='\\') r+="\\\\";
        else if (c=='\n') r+="\\n"; else r+=c;
    }
    return r;
}

static std::string b64enc(const uint8_t* d, size_t len) {
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string r; r.reserve(((len+2)/3)*4);
    for (size_t i=0;i<len;i+=3) {
        uint32_t n=uint32_t(d[i])<<16;
        if(i+1<len) n|=uint32_t(d[i+1])<<8;
        if(i+2<len) n|=uint32_t(d[i+2]);
        r+=t[(n>>18)&0x3F]; r+=t[(n>>12)&0x3F];
        r+=(i+1<len)?t[(n>>6)&0x3F]:'=';
        r+=(i+2<len)?t[n&0x3F]:'=';
    }
    return r;
}

static std::vector<uint8_t> b64dec(const std::string& in) {
    static const int dt[128]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1};
    std::vector<uint8_t> r; int v=0,b=-8;
    for (unsigned char c:in) { if(c=='='||c>127) continue; int d=dt[c]; if(d<0) continue; v=(v<<6)|d; b+=6; if(b>=0){r.push_back(uint8_t((v>>b)&0xFF)); b-=8;} }
    return r;
}

static std::string jstr(const std::string& j, const std::string& k) {
    auto p=j.find("\""+k+"\""); if(p==std::string::npos) return "";
    p=j.find(':',p); if(p==std::string::npos) return "";
    p=j.find('"',p+1); if(p==std::string::npos) return ""; p++;
    std::string r; while(p<j.size()&&j[p]!='"'){if(j[p]=='\\'&&p+1<j.size()){p++;r+=j[p];}else r+=j[p]; p++;} return r;
}

static int jint(const std::string& j, const std::string& k, int def=0) {
    auto p=j.find("\""+k+"\""); if(p==std::string::npos) return def;
    p=j.find(':',p); if(p==std::string::npos) return def; p++;
    while(p<j.size()&&(j[p]==' '||j[p]=='\t')) p++; return atoi(j.c_str()+p);
}

static std::vector<std::string> extract_blocks(const std::string& j) {
    std::vector<std::string> bl; auto s=j.find("\"accounts\""); if(s==std::string::npos) return bl;
    s=j.find('[',s); if(s==std::string::npos) return bl;
    int d=0; size_t os=0;
    for(size_t i=s;i<j.size();i++){if(j[i]=='{'){if(d==0)os=i;d++;}else if(j[i]=='}'){d--;if(d==0)bl.push_back(j.substr(os,i-os+1));}else if(j[i]==']'&&d==0)break;}
    return bl;
}

// ─── Crypto (Windows CNG) ─────────────────────────────────────────

static bool cng_random(uint8_t* buf, size_t len) {
    return BCRYPT_SUCCESS(BCryptGenRandom(nullptr, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool Vault::generate_key() {
    m_aes_key.resize(32);
    return cng_random(m_aes_key.data(), 32);
}

bool Vault::seal_key(const std::vector<uint8_t>& key, std::vector<uint8_t>& out) {
    DATA_BLOB in={DWORD(key.size()),const_cast<BYTE*>(key.data())}; DATA_BLOB o;
    if(!CryptProtectData(&in,L"Lumi",nullptr,nullptr,nullptr,0,&o)) return false;
    out.assign(o.pbData,o.pbData+o.cbData); LocalFree(o.pbData); return true;
}

bool Vault::unseal_key(const std::vector<uint8_t>& sealed, std::vector<uint8_t>& out) {
    DATA_BLOB in={DWORD(sealed.size()),const_cast<BYTE*>(sealed.data())}; DATA_BLOB o;
    if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,0,&o)) return false;
    out.assign(o.pbData,o.pbData+o.cbData); SecureZeroMemory(o.pbData,o.cbData); LocalFree(o.pbData); return true;
}

bool Vault::encrypt_data(const std::vector<uint8_t>& pt, const std::vector<uint8_t>& key,
                         std::vector<uint8_t>& iv, std::vector<uint8_t>& tag, std::vector<uint8_t>& ct) {
    iv.resize(12);
    if (!cng_random(iv.data(), 12)) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;

    do {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            break;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0)))
            break;

        // Import the key
        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                const_cast<PUCHAR>(key.data()), (ULONG)key.size(), 0)))
            break;

        // Setup auth info
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = iv.data();
        authInfo.cbNonce = (ULONG)iv.size();

        tag.resize(16);
        authInfo.pbTag = tag.data();
        authInfo.cbTag = (ULONG)tag.size();

        // Get required ciphertext size
        ULONG ctLen = 0;
        if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey, const_cast<PUCHAR>(pt.data()), (ULONG)pt.size(),
                &authInfo, nullptr, 0, nullptr, 0, &ctLen, 0)))
            break;

        ct.resize(ctLen);
        if (!BCRYPT_SUCCESS(BCryptEncrypt(hKey, const_cast<PUCHAR>(pt.data()), (ULONG)pt.size(),
                &authInfo, nullptr, 0, ct.data(), (ULONG)ct.size(), &ctLen, 0)))
            break;

        ct.resize(ctLen);
        ok = true;
    } while (false);

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

bool Vault::decrypt_data(const std::vector<uint8_t>& ct, const std::vector<uint8_t>& key,
                         const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag,
                         std::vector<uint8_t>& pt) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;

    do {
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            break;
        if (!BCRYPT_SUCCESS(BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0)))
            break;

        if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
                const_cast<PUCHAR>(key.data()), (ULONG)key.size(), 0)))
            break;

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce = const_cast<PUCHAR>(iv.data());
        authInfo.cbNonce = (ULONG)iv.size();
        authInfo.pbTag = const_cast<PUCHAR>(tag.data());
        authInfo.cbTag = (ULONG)tag.size();

        ULONG ptLen = 0;
        if (!BCRYPT_SUCCESS(BCryptDecrypt(hKey, const_cast<PUCHAR>(ct.data()), (ULONG)ct.size(),
                &authInfo, nullptr, 0, nullptr, 0, &ptLen, 0)))
            break;

        pt.resize(ptLen);
        if (!BCRYPT_SUCCESS(BCryptDecrypt(hKey, const_cast<PUCHAR>(ct.data()), (ULONG)ct.size(),
                &authInfo, nullptr, 0, pt.data(), (ULONG)pt.size(), &ptLen, 0)))
            break;

        pt.resize(ptLen);
        ok = true;
    } while (false);

    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ─── Vault file I/O ──────────────────────────────────────────────

bool Vault::open(const std::string& path) {
    m_path=path;
    auto par=std::filesystem::path(path).parent_path();
    if(!par.empty()) std::filesystem::create_directories(par);
    if(std::filesystem::exists(path)) return load();
    if(!generate_key()) return false;
    m_accounts.clear(); return save();
}

void Vault::close() {
    if(!m_aes_key.empty()){SecureZeroMemory(m_aes_key.data(),m_aes_key.size());m_aes_key.clear();}
    for(auto& a:m_accounts) SecureZeroMemory(a.secret,sizeof(a.secret));
    m_accounts.clear(); m_path.clear();
}

bool Vault::load() {
    std::ifstream f(m_path,std::ios::binary); if(!f) return false;
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>()); f.close();
    if(d.size()<12||memcmp(d.data(),VAULT_MAGIC,4)!=0) return false;
    size_t p=4; uint32_t ver; memcpy(&ver,d.data()+p,4); p+=4; if(ver!=VAULT_VERSION) return false;
    uint32_t skl; memcpy(&skl,d.data()+p,4); p+=4; if(p+skl>d.size()) return false;
    std::vector<uint8_t> sk(d.data()+p,d.data()+p+skl); p+=skl;
    if(!unseal_key(sk,m_aes_key)) return false;
    if(p+28>d.size()) return false;
    std::vector<uint8_t> iv_v(d.data()+p,d.data()+p+12); p+=12;
    std::vector<uint8_t> tag_v(d.data()+p,d.data()+p+16); p+=16;
    std::vector<uint8_t> ct(d.data()+p,d.data()+d.size());
    std::vector<uint8_t> pt; if(!decrypt_data(ct,m_aes_key,iv_v,tag_v,pt)) return false;
    std::string json(pt.begin(),pt.end()); SecureZeroMemory(pt.data(),pt.size());
    m_accounts.clear();
    for(auto& bl:extract_blocks(json)){
        LumiAccount a{}; strncpy_s(a.id,sizeof(a.id),jstr(bl,"id").c_str(),_TRUNCATE);
        strncpy_s(a.name,sizeof(a.name),jstr(bl,"name").c_str(),_TRUNCATE);
        strncpy_s(a.issuer,sizeof(a.issuer),jstr(bl,"issuer").c_str(),_TRUNCATE);
        auto sb=b64dec(jstr(bl,"secret_b64")); size_t cl=(std::min)(sb.size(),size_t(64));
        memcpy(a.secret,sb.data(),cl); a.secret_len=int(cl);
        a.algorithm=jint(bl,"algorithm"); a.digits=jint(bl,"digits",6); a.period=jint(bl,"period",30);
        a.type=jint(bl,"type"); a.hotp_counter=uint64_t(jint(bl,"hotp_counter"));
        m_accounts.push_back(a);
    }
    return true;
}

bool Vault::save() {
    std::ostringstream j; j<<"{\n  \"version\": 1,\n  \"accounts\": [\n";
    for(size_t i=0;i<m_accounts.size();i++){
        auto& a=m_accounts[i];
        j<<"    {\"id\":\""<<escape_json(a.id)<<"\",\"name\":\""<<escape_json(a.name)
         <<"\",\"issuer\":\""<<escape_json(a.issuer)<<"\",\"secret_b64\":\""<<b64enc(a.secret,a.secret_len)
         <<"\",\"algorithm\":"<<a.algorithm<<",\"digits\":"<<a.digits<<",\"period\":"<<a.period
         <<",\"type\":"<<a.type<<",\"hotp_counter\":"<<a.hotp_counter<<",\"sort_order\":"<<i<<"}";
        if(i+1<m_accounts.size()) j<<","; j<<"\n";
    }
    j<<"  ]\n}"; std::string js=j.str();
    std::vector<uint8_t> pt(js.begin(),js.end()),iv_v,tag_v,ct;
    if(!encrypt_data(pt,m_aes_key,iv_v,tag_v,ct)){SecureZeroMemory(pt.data(),pt.size());return false;}
    SecureZeroMemory(pt.data(),pt.size());
    std::vector<uint8_t> sk; if(!seal_key(m_aes_key,sk)) return false;
    std::ofstream f(m_path,std::ios::binary|std::ios::trunc); if(!f) return false;
    f.write((const char*)VAULT_MAGIC,4); f.write((const char*)&VAULT_VERSION,4);
    uint32_t sl=uint32_t(sk.size()); f.write((const char*)&sl,4); f.write((const char*)sk.data(),sk.size());
    f.write((const char*)iv_v.data(),iv_v.size()); f.write((const char*)tag_v.data(),tag_v.size());
    f.write((const char*)ct.data(),ct.size()); f.close(); return f.good();
}

// ─── CRUD ────────────────────────────────────────────────────────

std::vector<LumiAccount> Vault::get_all() const { return m_accounts; }

bool Vault::save_account(const LumiAccount& acc) {
    for(auto& a:m_accounts) if(strcmp(a.id,acc.id)==0){a=acc;return save();}
    m_accounts.push_back(acc); return save();
}

bool Vault::delete_account(const std::string& id) {
    auto it=std::remove_if(m_accounts.begin(),m_accounts.end(),[&](const LumiAccount& a){return id==a.id;});
    if(it==m_accounts.end()) return false;
    for(auto j=it;j!=m_accounts.end();++j) SecureZeroMemory(j->secret,sizeof(j->secret));
    m_accounts.erase(it,m_accounts.end()); return save();
}

bool Vault::rename_account(const std::string& id, const std::string& nm) {
    for(auto& a:m_accounts) if(id==a.id){strncpy_s(a.name,sizeof(a.name),nm.c_str(),_TRUNCATE);return save();}
    return false;
}

bool Vault::reorder(const std::vector<std::string>& order) {
    std::vector<LumiAccount> r; r.reserve(m_accounts.size());
    for(auto& id:order) for(auto& a:m_accounts) if(id==a.id){r.push_back(a);break;}
    for(auto& a:m_accounts){bool f=false;for(auto& x:r)if(strcmp(a.id,x.id)==0){f=true;break;}if(!f)r.push_back(a);}
    m_accounts=std::move(r); return save();
}
