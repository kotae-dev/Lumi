using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Lumi.Core
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct LumiAccountNative
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 37)]
        public string Id;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string Name;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string Issuer;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
        public byte[] Secret;

        public int SecretLen;
        public int Algorithm;
        public int Digits;
        public int Period;
        public int Type;
        public long HotpCounter;
    }

    public static class NativeInterop
    {
        private const string DLL = "lumi_core.dll";

        // ─── QR / Import ────────────────────────────────────
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_decode_qr_file(
            string imagePath,
            out IntPtr outAccounts);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lumi_free_accounts(IntPtr accounts, int count);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_parse_otpauth_uri(
            string uri,
            ref LumiAccountNative outAccount);

        // ─── TOTP ───────────────────────────────────────────
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lumi_get_totp(
            ref LumiAccountNative account,
            StringBuilder codeOut,
            out int secondsRemainingOut);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern long lumi_get_time();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_ntp_sync(string? server);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern long lumi_get_corrected_time();

        // ─── Vault ──────────────────────────────────────────
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_vault_init(string vaultPath);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void lumi_vault_close();

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lumi_vault_get_all(
            out IntPtr outAccounts,
            out int outCount);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int lumi_vault_save_account(
            ref LumiAccountNative account);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_vault_delete_account(string id);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int lumi_vault_rename_account(string id, string newName);

        // ─── Error ──────────────────────────────────────────
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr lumi_last_error();

        // ─── Helper: get error string ───────────────────────
        public static string GetLastError()
        {
            IntPtr ptr = lumi_last_error();
            return ptr != IntPtr.Zero ? Marshal.PtrToStringAnsi(ptr) ?? "" : "";
        }

        // ─── Helper: marshal account array ──────────────────
        public static LumiAccountNative[] MarshalAccounts(IntPtr ptr, int count)
        {
            var accounts = new LumiAccountNative[count];
            int structSize = Marshal.SizeOf<LumiAccountNative>();

            for (int i = 0; i < count; i++)
            {
                IntPtr offset = IntPtr.Add(ptr, i * structSize);
                accounts[i] = Marshal.PtrToStructure<LumiAccountNative>(offset);
            }

            return accounts;
        }
    }
}
