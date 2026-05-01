using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Lumi.Core;

namespace Lumi.Services
{
    /// <summary>
    /// Wraps native vault DLL calls for managed access.
    /// Singleton — use VaultManager.Instance.
    /// </summary>
    public class VaultManager
    {
        private static readonly Lazy<VaultManager> _lazy = new(() => new VaultManager());
        public static VaultManager Instance => _lazy.Value;

        private bool _initialized;

        private VaultManager() { }

        public bool Initialize(string vaultPath)
        {
            int result = NativeInterop.lumi_vault_init(vaultPath);
            _initialized = result == 0;
            return _initialized;
        }

        public void Close()
        {
            if (_initialized)
            {
                NativeInterop.lumi_vault_close();
                _initialized = false;
            }
        }

        public List<TotpAccount> GetAllAccounts()
        {
            var accounts = new List<TotpAccount>();
            if (!_initialized) return accounts;

            int result = NativeInterop.lumi_vault_get_all(out IntPtr ptr, out int count);
            if (result != 0 || count <= 0 || ptr == IntPtr.Zero) return accounts;

            var natives = NativeInterop.MarshalAccounts(ptr, count);
            NativeInterop.lumi_free_accounts(ptr, count);

            foreach (var native in natives)
            {
                accounts.Add(TotpAccount.FromNative(native));
            }

            return accounts;
        }

        public bool SaveAccount(TotpAccount account)
        {
            if (!_initialized) return false;
            var native = account.ToNative();
            return NativeInterop.lumi_vault_save_account(ref native) == 0;
        }

        public bool DeleteAccount(string id)
        {
            if (!_initialized) return false;
            return NativeInterop.lumi_vault_delete_account(id) == 0;
        }

        public bool RenameAccount(string id, string newName)
        {
            if (!_initialized) return false;
            return NativeInterop.lumi_vault_rename_account(id, newName) == 0;
        }

        public List<TotpAccount> ImportFromQrFile(string imagePath)
        {
            var accounts = new List<TotpAccount>();

            int count = NativeInterop.lumi_decode_qr_file(imagePath, out IntPtr ptr);
            if (count <= 0 || ptr == IntPtr.Zero) return accounts;

            var natives = NativeInterop.MarshalAccounts(ptr, count);
            NativeInterop.lumi_free_accounts(ptr, count);

            foreach (var native in natives)
            {
                accounts.Add(TotpAccount.FromNative(native));
            }

            return accounts;
        }

        public TotpAccount? ImportFromUri(string uri)
        {
            var native = new LumiAccountNative { Secret = new byte[64] };
            int result = NativeInterop.lumi_parse_otpauth_uri(uri, ref native);
            return result == 0 ? TotpAccount.FromNative(native) : null;
        }

        public (string Code, int SecondsLeft) GetCode(TotpAccount account)
        {
            var native = account.ToNative();
            var sb = new System.Text.StringBuilder(9);
            int result = NativeInterop.lumi_get_totp(ref native, sb, out int secsLeft);
            return result == 0 ? (sb.ToString(), secsLeft) : ("------", 0);
        }
    }
}
