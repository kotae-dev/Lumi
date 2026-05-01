using System;

namespace Lumi.Core
{
    /// <summary>
    /// Managed model for a TOTP/HOTP account.
    /// </summary>
    public class TotpAccount
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public string Issuer { get; set; } = string.Empty;
        public byte[] Secret { get; set; } = Array.Empty<byte>();
        public int Algorithm { get; set; } // 0=SHA1, 1=SHA256, 2=SHA512
        public int Digits { get; set; } = 6;
        public int Period { get; set; } = 30;
        public int Type { get; set; } // 0=TOTP, 1=HOTP
        public long HotpCounter { get; set; }

        /// <summary>
        /// Convert from native struct to managed model.
        /// </summary>
        public static TotpAccount FromNative(LumiAccountNative native)
        {
            var account = new TotpAccount
            {
                Id = native.Id ?? string.Empty,
                Name = native.Name ?? string.Empty,
                Issuer = native.Issuer ?? string.Empty,
                Algorithm = native.Algorithm,
                Digits = native.Digits > 0 ? native.Digits : 6,
                Period = native.Period > 0 ? native.Period : 30,
                Type = native.Type,
                HotpCounter = native.HotpCounter,
            };

            if (native.Secret != null && native.SecretLen > 0)
            {
                account.Secret = new byte[native.SecretLen];
                Array.Copy(native.Secret, account.Secret, native.SecretLen);
            }

            return account;
        }

        /// <summary>
        /// Convert from managed model to native struct.
        /// </summary>
        public LumiAccountNative ToNative()
        {
            var native = new LumiAccountNative
            {
                Id = Id,
                Name = Name,
                Issuer = Issuer,
                Algorithm = Algorithm,
                Digits = Digits,
                Period = Period,
                Type = Type,
                HotpCounter = HotpCounter,
                Secret = new byte[64],
                SecretLen = Secret?.Length ?? 0,
            };

            if (Secret != null && Secret.Length > 0)
            {
                Array.Copy(Secret, native.Secret, Math.Min(Secret.Length, 64));
            }

            return native;
        }

        /// <summary>
        /// Display name: Issuer or Name or "Unknown".
        /// </summary>
        public string DisplayName =>
            !string.IsNullOrWhiteSpace(Issuer) ? Issuer :
            !string.IsNullOrWhiteSpace(Name) ? Name :
            "Unknown";

        /// <summary>
        /// Text for the circular icon (e.g. "G" for Google).
        /// </summary>
        public string IconText
        {
            get
            {
                string text = DisplayName.Trim();
                if (string.IsNullOrEmpty(text)) return "?";
                if (text.Length >= 2 && char.IsLetterOrDigit(text[0]) && char.IsLetterOrDigit(text[1]))
                {
                    return text.Substring(0, 1).ToUpperInvariant();
                }
                return text.Substring(0, 1).ToUpperInvariant();
            }
        }
    }
}
