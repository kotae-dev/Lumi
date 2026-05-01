# Lumi — Windows Authenticator App
## Complete Implementation Plan

> **Stack:** C++ Core (DLL) · C# WPF UI (.exe) · CMake · .NET 8 · Modern Fluent Design

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Google Authenticator QR Code Format — Deep Dive](#2-google-authenticator-qr-code-format--deep-dive)
3. [Solution Architecture](#3-solution-architecture)
4. [Repository Structure](#4-repository-structure)
5. [C++ Core (lumi_core.dll)](#5-c-core-lumi_coredll)
6. [C# WPF UI (Lumi.exe)](#6-c-wpf-ui-lumiexe)
7. [Build System](#7-build-system)
8. [Feature Specification](#8-feature-specification)
9. [UI/UX Design Specification](#9-uiux-design-specification)
10. [Data Storage & Security](#10-data-storage--security)
11. [Dependencies & Third-Party Libraries](#11-dependencies--third-party-libraries)
12. [Implementation Phases & Roadmap](#12-implementation-phases--roadmap)
13. [Testing Strategy](#13-testing-strategy)
14. [Distribution](#14-distribution)

---

## 1. Project Overview

**Lumi** is a modern Windows desktop TOTP (Time-based One-Time Password) authenticator app. It replaces Google Authenticator on desktop with a polished, native Windows experience. It is purposely minimal — the goal is clarity, speed, and security over feature bloat.

### Core Goals

| Goal | Description |
|------|-------------|
| **Import from Google Authenticator** | Decode the `otpauth-migration://` QR code export format |
| **TOTP Generation** | RFC 6238-compliant code generation in C++ |
| **Modern UI** | Fluent Design System, dark/light mode, smooth animations |
| **Editable names** | In-place label editing for each account |
| **Countdown ring** | Visual timer showing time until code refresh |
| **Encrypted storage** | AES-256-GCM vault using Windows DPAPI for key sealing |
| **No telemetry** | Fully offline; no network calls except NTP sync |

---

## 2. Google Authenticator QR Code Format — Deep Dive

This is the most critical technical piece. Understanding the export format correctly is essential.

### 2.1 What the Attached QR Code Contains

When you use **"Export accounts"** in Google Authenticator, it generates one or more QR codes. Each QR code encodes a URI of the form:

```
otpauth-migration://offline?data=<BASE64_ENCODED_PROTOBUF>
```

The `data` parameter is a **URL-encoded, Base64-encoded Protocol Buffer** payload. This is NOT a standard `otpauth://totp/...` URI — it is a proprietary batch-export format.

### 2.2 Protobuf Schema (Reverse Engineered)

Google has not published an official schema, but the community has fully reverse-engineered it. The `.proto` definition is:

```protobuf
syntax = "proto3";

message MigrationPayload {
  enum Algorithm {
    ALGORITHM_UNSPECIFIED = 0;
    SHA1   = 1;
    SHA256 = 2;
    SHA512 = 3;
    MD5    = 4;
  }

  enum DigitCount {
    DIGIT_COUNT_UNSPECIFIED = 0;
    SIX   = 1;
    EIGHT = 2;
  }

  enum OtpType {
    OTP_TYPE_UNSPECIFIED = 0;
    HOTP = 1;
    TOTP = 2;
  }

  message OtpParameters {
    bytes     secret        = 1;  // Raw binary secret (NOT base32)
    string    name          = 2;  // "issuer:account" or just account
    string    issuer        = 3;
    Algorithm algorithm     = 4;
    DigitCount digit_count  = 5;
    OtpType   type          = 6;
    int64     counter       = 7;  // HOTP only
  }

  repeated OtpParameters otp_parameters = 1;
  int32                  version        = 2;
  int32                  batch_size     = 3;
  int32                  batch_index    = 4;
  int32                  batch_id       = 5;
}
```

### 2.3 Decoding Pipeline

```
QR Image File
      │
      ▼
[ZXing-C++ QR Decoder]  →  raw URI string
      │
      ▼
Parse URI: extract `data` query parameter
      │
      ▼
URL-decode `data` → Base64 string
      │
      ▼
Base64-decode → raw bytes (protobuf payload)
      │
      ▼
[nanopb or protobuf-lite] parse MigrationPayload
      │
      ▼
For each OtpParameters:
  - secret (bytes) → store as-is (raw binary)
  - name, issuer   → store as UTF-8 strings
  - algorithm      → map to enum
  - digit_count    → 6 or 8
  - type           → TOTP or HOTP
      │
      ▼
Write to encrypted vault
```

### 2.4 Alternative: Single-Account otpauth:// URIs

Standard QR codes (from individual services, not Google Authenticator export) use:

```
otpauth://totp/ISSUER:ACCOUNT?secret=BASE32SECRET&issuer=ISSUER&algorithm=SHA1&digits=6&period=30
```

Lumi should support **both** formats.

### 2.5 Better Sync Options

While QR image import works well, there are additional sync strategies worth considering:

| Method | How It Works | Pros | Cons |
|--------|-------------|------|------|
| **QR Image Import** *(Implement First)* | User screenshots Google Auth export QR, loads image file | Simple, works now | Manual, one-time |
| **Webcam / Camera Scan** | Use DirectShow/MediaCapture to read QR from camera | Convenient | Needs camera permissions |
| **Drag & Drop QR** | User drags QR screenshot onto the app window | Frictionless UX | Still manual |
| **`otpauth://` URI** | User pastes a URI directly | Fast for power users | Less discoverable |
| **Manual Entry** | Form: Name, Secret (Base32), Algorithm, Digits | Universal fallback | Tedious |
| **Encrypted Backup Import** | Import from `.lumi` encrypted backup file | For Lumi→Lumi transfer | No cross-app compat |

> **Recommendation:** Implement QR image file drop + manual entry in v1. Add camera scan in v2.

---

## 3. Solution Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Lumi.exe  (C# WPF)                   │
│                                                         │
│  ┌──────────┐  ┌───────────┐  ┌────────────────────┐   │
│  │ MainView │  │ AddAccount│  │  SettingsView       │   │
│  │ (list)   │  │ Wizard    │  │  (theme, NTP, etc.) │   │
│  └────┬─────┘  └─────┬─────┘  └─────────────────────┘   │
│       │              │                                   │
│  ┌────▼──────────────▼──────────────────────────────┐   │
│  │           ViewModel Layer (MVVM)                 │   │
│  │  AccountListViewModel · AddAccountViewModel      │   │
│  └────────────────────┬─────────────────────────────┘   │
│                       │  P/Invoke  (DllImport)           │
└───────────────────────┼─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│               lumi_core.dll  (C++)                       │
│                                                         │
│  ┌────────────┐  ┌────────────┐  ┌──────────────────┐  │
│  │ QR Decoder │  │ TOTP Engine│  │  Vault / Storage │  │
│  │ (ZXing-C++)│  │ (RFC 6238) │  │  (AES-256-GCM)   │  │
│  └────────────┘  └────────────┘  └──────────────────┘  │
│  ┌────────────┐  ┌────────────┐                         │
│  │ Protobuf   │  │ NTP Client │                         │
│  │ Parser     │  │ (optional) │                         │
│  └────────────┘  └────────────┘                         │
└─────────────────────────────────────────────────────────┘
                        │
              Windows DPAPI  (CryptProtectData)
              AppData\Roaming\Lumi\vault.lumi
```

### Inter-Layer Communication

The C# UI communicates with the C++ DLL via **P/Invoke** with a clean flat C API surface. No COM, no C++/CLI — keeps the boundary simple and testable.

---

## 4. Repository Structure

```
Lumi/
├── CMakeLists.txt                  # Root CMake — orchestrates core + calls dotnet
├── .github/
│   └── workflows/
│       └── build.yml               # CI: cmake build + dotnet build
│
├── core/                           # C++ DLL project
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── lumi_core.h             # Public C API (extern "C")
│   ├── src/
│   │   ├── api.cpp                 # Exported function implementations
│   │   ├── totp.cpp / totp.h       # RFC 6238 TOTP engine
│   │   ├── hotp.cpp / hotp.h       # RFC 4226 HOTP engine
│   │   ├── qr_decoder.cpp          # ZXing-C++ wrapper
│   │   ├── migration_parser.cpp    # Google migration protobuf parser
│   │   ├── vault.cpp / vault.h     # AES-256-GCM encrypted storage
│   │   ├── base32.cpp / base32.h   # Base32 encode/decode
│   │   ├── ntp_client.cpp          # Optional NTP time sync
│   │   └── hmac_sha1.cpp           # HMAC-SHA1/256/512
│   ├── third_party/
│   │   ├── zxing-cpp/              # Git submodule
│   │   ├── nanopb/                 # Git submodule (protobuf)
│   │   └── openssl/                # Or mbedtls for crypto
│   └── tests/
│       ├── test_totp.cpp
│       ├── test_qr_decoder.cpp
│       └── test_vault.cpp
│
├── ui/                             # C# WPF project
│   ├── Lumi.csproj
│   ├── App.xaml / App.xaml.cs
│   ├── Assets/
│   │   ├── lumi-logo.svg
│   │   └── Fonts/
│   │       └── Inter-Variable.ttf
│   ├── Core/
│   │   ├── NativeInterop.cs        # P/Invoke declarations
│   │   ├── TotpAccount.cs          # Model
│   │   └── VaultManager.cs         # Wraps DLL vault calls
│   ├── ViewModels/
│   │   ├── BaseViewModel.cs        # INotifyPropertyChanged
│   │   ├── AccountListViewModel.cs
│   │   ├── AccountItemViewModel.cs
│   │   └── AddAccountViewModel.cs
│   ├── Views/
│   │   ├── MainWindow.xaml / .cs
│   │   ├── AccountCard.xaml / .cs  # UserControl for each code
│   │   ├── AddAccountDialog.xaml
│   │   └── SettingsWindow.xaml
│   ├── Converters/
│   │   ├── TimeToArcConverter.cs   # Countdown arc geometry
│   │   └── CodeFormatterConverter.cs # "123456" → "123 456"
│   ├── Styles/
│   │   ├── Colors.xaml
│   │   ├── Typography.xaml
│   │   └── Controls.xaml
│   └── Services/
│       ├── ThemeService.cs
│       ├── ClipboardService.cs
│       └── NtpSyncService.cs
│
├── proto/
│   └── migration_payload.proto     # .proto source
│
├── scripts/
│   ├── build.ps1                   # One-shot build script
│   └── generate_proto.ps1          # Runs nanopb generator
│
└── dist/
    └── installer/
        └── lumi-setup.iss          # Inno Setup installer script
```

---

## 5. C++ Core (lumi_core.dll)

### 5.1 Public C API — `lumi_core.h`

This is the boundary between C++ and C#. All functions use a flat C ABI to avoid name mangling and ABI compatibility issues.

```c
#pragma once
#ifdef LUMI_EXPORTS
  #define LUMI_API __declspec(dllexport)
#else
  #define LUMI_API __declspec(dllimport)
#endif

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
// Decode a QR image file. Returns number of accounts found.
// Caller owns `out_accounts` array; call lumi_free_accounts when done.
LUMI_API int  lumi_decode_qr_file(
    const char*   image_path,
    LumiAccount** out_accounts
);
LUMI_API void lumi_free_accounts(LumiAccount* accounts, int count);

// Parse a single otpauth:// URI
LUMI_API int  lumi_parse_otpauth_uri(
    const char*  uri,
    LumiAccount* out_account
);

// ─── TOTP ────────────────────────────────────────────────────────
// Returns 0 on success. code_out: null-terminated string "123456"
LUMI_API int  lumi_get_totp(
    const LumiAccount* account,
    char*              code_out,    // at least 9 bytes
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
```

### 5.2 TOTP Engine (`totp.cpp`)

Full RFC 6238 implementation:

```cpp
// T = floor(unix_time / period)
// HOTP(secret, T) using HMAC-SHA1/256/512
// Truncate to `digits` decimal digits

uint32_t compute_totp(
    const uint8_t* secret, int secret_len,
    int64_t unix_time, int period,
    int digits, HmacAlgorithm algo
) {
    int64_t T = unix_time / period;

    // Big-endian 8-byte counter
    uint8_t msg[8];
    for (int i = 7; i >= 0; i--) {
        msg[i] = T & 0xFF;
        T >>= 8;
    }

    uint8_t hmac[64];
    int hmac_len = hmac_compute(algo, secret, secret_len, msg, 8, hmac);

    // Dynamic truncation
    int offset = hmac[hmac_len - 1] & 0x0F;
    uint32_t bincode =
        ((hmac[offset]     & 0x7F) << 24) |
        ((hmac[offset + 1] & 0xFF) << 16) |
        ((hmac[offset + 2] & 0xFF) <<  8) |
        ((hmac[offset + 3] & 0xFF));

    static const uint32_t pow10[] = {1,10,100,1000,10000,100000,1000000,10000000,100000000};
    return bincode % pow10[digits];
}
```

### 5.3 QR Decoder (`qr_decoder.cpp`)

```cpp
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>

// Load image using stb_image (header-only, no extra deps)
// Pass pixel buffer to ZXing
// Return decoded URI string

std::string decode_qr_from_file(const std::filesystem::path& image_path) {
    int w, h, ch;
    uint8_t* pixels = stbi_load(image_path.string().c_str(), &w, &h, &ch, 1);
    if (!pixels) throw std::runtime_error("Cannot load image");

    ZXing::ImageView view(pixels, w, h, ZXing::ImageFormat::Lum);
    auto result = ZXing::ReadBarcode(view);
    stbi_image_free(pixels);

    if (!result.isValid())
        throw std::runtime_error("No QR code found in image");

    return result.text();
}
```

### 5.4 Migration Parser (`migration_parser.cpp`)

```cpp
// 1. Parse "otpauth-migration://offline?data=<ENCODED>"
// 2. URL-decode the data parameter
// 3. Base64-decode → raw bytes
// 4. Parse as MigrationPayload protobuf using nanopb
// 5. Return vector<LumiAccount>

std::vector<LumiAccount> parse_migration_uri(const std::string& uri) {
    auto data_b64 = url_decode(extract_query_param(uri, "data"));
    auto proto_bytes = base64_decode(data_b64);

    // nanopb decode
    MigrationPayload payload = MigrationPayload_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(proto_bytes.data(), proto_bytes.size());
    if (!pb_decode(&stream, MigrationPayload_fields, &payload))
        throw std::runtime_error("Protobuf decode failed");

    std::vector<LumiAccount> accounts;
    for (int i = 0; i < payload.otp_parameters_count; i++) {
        auto& p = payload.otp_parameters[i];
        LumiAccount acc{};
        generate_uuid_v4(acc.id);
        strncpy(acc.name,   p.name,   sizeof(acc.name) - 1);
        strncpy(acc.issuer, p.issuer, sizeof(acc.issuer) - 1);
        memcpy(acc.secret, p.secret.bytes, p.secret.size);
        acc.secret_len = p.secret.size;
        acc.algorithm  = map_algorithm(p.algorithm);
        acc.digits     = (p.digit_count == DigitCount_EIGHT) ? 8 : 6;
        acc.period     = 30;
        acc.type       = (p.type == OtpType_HOTP) ? 1 : 0;
        acc.hotp_counter = p.counter;
        accounts.push_back(acc);
    }
    return accounts;
}
```

### 5.5 Vault (`vault.cpp`)

The vault is a single encrypted binary file:

```
vault.lumi format:
┌─────────────────────────────────────┐
│  Magic: "LUMI" (4 bytes)            │
│  Version: uint32 (LE)               │
│  DPAPI-sealed AES key (var len)     │
│  IV: 12 bytes (GCM nonce)           │
│  Tag: 16 bytes (GCM auth tag)       │
│  Ciphertext: N bytes                │
│  (plaintext = JSON UTF-8)           │
└─────────────────────────────────────┘
```

The inner JSON structure:

```json
{
  "version": 1,
  "accounts": [
    {
      "id": "uuid",
      "name": "GitHub",
      "issuer": "GitHub",
      "secret_b64": "base64(raw secret bytes)",
      "algorithm": 0,
      "digits": 6,
      "period": 30,
      "type": 0,
      "sort_order": 0
    }
  ]
}
```

Key management: AES-256-GCM key is generated once, then sealed using **Windows DPAPI** (`CryptProtectData` with `CRYPTPROTECT_LOCAL_MACHINE = false`, binding to the current user account). The sealed blob is stored in the vault header, so the vault is only decryptable on the same machine by the same Windows user.

---

## 6. C# WPF UI (Lumi.exe)

### 6.1 P/Invoke Layer (`NativeInterop.cs`)

```csharp
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct LumiAccountNative {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 37)]  public string Id;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string Name;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string Issuer;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)] public byte[] Secret;
    public int SecretLen;
    public int Algorithm;
    public int Digits;
    public int Period;
    public int Type;
    public long HotpCounter;
}

public static class NativeInterop {
    private const string DLL = "lumi_core.dll";

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int lumi_decode_qr_file(
        string imagePath, out IntPtr outAccounts);

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern void lumi_free_accounts(IntPtr accounts, int count);

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int lumi_get_totp(
        ref LumiAccountNative account,
        [Out][MarshalAs(UnmanagedType.LPStr)] StringBuilder codeOut,
        out int secondsRemainingOut);

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int lumi_vault_init(string vaultPath);

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int lumi_vault_rename_account(string id, string newName);

    // ... all other API functions
}
```

### 6.2 AccountItemViewModel

```csharp
public class AccountItemViewModel : BaseViewModel {
    private DispatcherTimer _timer;
    private LumiAccountNative _native;

    public string Id           { get; }
    public string Issuer       { get; }
    public string Name         { get => _name; set { _name = value; OnPropertyChanged(); SaveName(); } }
    public string Code         { get => _code; private set { _code = value; OnPropertyChanged(); } }
    public string FormattedCode => Code.Length == 6
        ? $"{Code[..3]} {Code[3..]}"       // "123 456"
        : $"{Code[..4]} {Code[4..]}";      // "1234 5678"

    public int    SecondsLeft    { get => _secondsLeft;  private set { _secondsLeft  = value; OnPropertyChanged(); OnPropertyChanged(nameof(Progress)); } }
    public double Progress       => SecondsLeft / (double)_native.Period;  // 0.0 to 1.0
    public bool   IsExpiringSoon => SecondsLeft <= 5;

    public ICommand CopyCommand { get; }
    public ICommand EditNameCommand { get; }

    public AccountItemViewModel(LumiAccountNative native) {
        _native = native;
        Id = native.Id;
        Issuer = native.Issuer;
        CopyCommand = new RelayCommand(_ => CopyToClipboard());

        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
        _timer.Tick += (_, _) => Refresh();
        _timer.Start();
        Refresh();
    }

    private void Refresh() {
        var sb = new StringBuilder(9);
        NativeInterop.lumi_get_totp(ref _native, sb, out int secsLeft);
        Code = sb.ToString();
        SecondsLeft = secsLeft;
    }

    private void CopyToClipboard() {
        Clipboard.SetText(Code);
        // Trigger "Copied!" overlay animation
    }
}
```

### 6.3 Main Window Layout (XAML sketch)

```xml
<Window ...>
  <!-- Custom titlebar with drag region -->
  <Grid>
    <Grid.RowDefinitions>
      <RowDefinition Height="48"/>   <!-- TitleBar -->
      <RowDefinition Height="Auto"/> <!-- Search box -->
      <RowDefinition Height="*"/>    <!-- Account list -->
      <RowDefinition Height="64"/>   <!-- Bottom toolbar -->
    </Grid.RowDefinitions>

    <!-- TitleBar -->
    <Border Grid.Row="0" Background="{DynamicResource TitleBarBrush}">
      <Grid>
        <Image Source="Assets/lumi-logo.svg" Width="20" HorizontalAlignment="Left" Margin="16,0"/>
        <TextBlock Text="Lumi" VerticalAlignment="Center" HorizontalAlignment="Left" Margin="44,0,0,0"/>
        <!-- Window controls: minimize, close -->
      </Grid>
    </Border>

    <!-- Search -->
    <TextBox Grid.Row="1" Margin="16,8"
             Text="{Binding SearchQuery, UpdateSourceTrigger=PropertyChanged}"
             Style="{StaticResource SearchBoxStyle}"
             PlaceholderText="Search accounts…"/>

    <!-- Account List -->
    <ScrollViewer Grid.Row="2" VerticalScrollBarVisibility="Auto">
      <ItemsControl ItemsSource="{Binding FilteredAccounts}">
        <ItemsControl.ItemTemplate>
          <DataTemplate>
            <local:AccountCard DataContext="{Binding}" Margin="12,4"/>
          </DataTemplate>
        </ItemsControl.ItemTemplate>
      </ItemsControl>
    </ScrollViewer>

    <!-- Bottom toolbar -->
    <Border Grid.Row="3" Background="{DynamicResource ToolbarBrush}">
      <StackPanel Orientation="Horizontal" HorizontalAlignment="Right" Margin="16,0">
        <Button Command="{Binding AddQrCommand}"    Content="Scan QR"    Style="{StaticResource ToolbarButton}"/>
        <Button Command="{Binding AddManualCommand}" Content="Add Manual" Style="{StaticResource ToolbarButton}"/>
        <Button Command="{Binding SettingsCommand}" Content="⚙"          Style="{StaticResource IconButton}"/>
      </StackPanel>
    </Border>
  </Grid>
</Window>
```

### 6.4 AccountCard UserControl

The account card is the heart of the UI:

```xml
<UserControl x:Class="Lumi.Views.AccountCard">
  <Border Style="{StaticResource CardStyle}" CornerRadius="12">
    <Grid Margin="16,14">
      <Grid.ColumnDefinitions>
        <ColumnDefinition Width="*"/>      <!-- Text info -->
        <ColumnDefinition Width="Auto"/>   <!-- Timer ring + code -->
      </Grid.ColumnDefinitions>

      <!-- Left: Icon + Names -->
      <StackPanel Grid.Column="0" VerticalAlignment="Center">
        <Image Source="{Binding IssuerIcon}" Width="24" Height="24" HorizontalAlignment="Left"/>

        <!-- Editable name (double-click to edit) -->
        <TextBlock Text="{Binding Name}" Style="{StaticResource IssuerLabel}"
                   MouseLeftButtonDown="StartEdit" Visibility="{Binding IsEditing, Converter={...}, ConverterParameter=Collapsed}"/>
        <TextBox   Text="{Binding Name}" Style="{StaticResource InlineEditBox}"
                   Visibility="{Binding IsEditing, Converter={...}}"
                   LostFocus="CommitEdit" KeyDown="OnEditKeyDown"/>

        <TextBlock Text="{Binding Issuer}" Style="{StaticResource AccountLabel}"/>
      </StackPanel>

      <!-- Right: Code + Circular countdown -->
      <StackPanel Grid.Column="1" Orientation="Horizontal" VerticalAlignment="Center">

        <!-- Code display — click to copy -->
        <Button Command="{Binding CopyCommand}" Style="{StaticResource CodeButton}">
          <StackPanel>
            <TextBlock Text="{Binding FormattedCode}" Style="{StaticResource CodeLabel}"
                       Foreground="{Binding IsExpiringSoon, Converter={StaticResource ExpiryColorConverter}}"/>
            <TextBlock Text="Tap to copy" Style="{StaticResource HintLabel}"/>
          </StackPanel>
        </Button>

        <!-- Circular countdown ring -->
        <Canvas Width="44" Height="44" Margin="12,0,0,0">
          <!-- Background ring -->
          <Ellipse Width="44" Height="44" Stroke="{DynamicResource RingTrackBrush}" StrokeThickness="3"/>
          <!-- Progress arc (Path with ArcSegment, driven by converter) -->
          <Path Stroke="{DynamicResource AccentBrush}" StrokeThickness="3" StrokeLineCap="Round"
                Data="{Binding Progress, Converter={StaticResource TimeToArcConverter}}"/>
          <!-- Seconds label -->
          <TextBlock Text="{Binding SecondsLeft}" Style="{StaticResource TimerLabel}"
                     Canvas.Left="14" Canvas.Top="12"/>
        </Canvas>

      </StackPanel>
    </Grid>
  </Border>
</UserControl>
```

### 6.5 Add Account Wizard

Three pages in a slide-in `ContentControl`:

```
Page 1: Choose method
  ┌──────────────────────────────┐
  │  [📷 Scan QR Image File]    │  → FilePicker → Page 3
  │  [🔗 Paste URI]             │  → TextBox → Page 3
  │  [✏  Enter Manually]        │  → Page 2
  └──────────────────────────────┘

Page 2: Manual entry form
  ┌──────────────────────────────┐
  │  Account name: [___________]│
  │  Issuer:       [___________]│
  │  Secret key:   [___________]│
  │  Algorithm:    ○SHA1 ○SHA256│
  │  Digits:       ○6    ○8     │
  │  Period:       ○30s  ○60s   │
  │            [Back] [Add →]   │
  └──────────────────────────────┘

Page 3: Confirm imported accounts
  ┌──────────────────────────────┐
  │  Found 5 accounts:           │
  │  ☑ GitHub — user@email.com  │
  │  ☑ Google — user@gmail.com  │
  │  ☑ AWS — myaccount          │
  │  ☐ (uncheck to skip)        │
  │            [Import Selected] │
  └──────────────────────────────┘
```

---

## 7. Build System

### 7.1 Root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.25)
project(Lumi VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Build the C++ core DLL
add_subdirectory(core)

# Trigger dotnet build as a post-build step
find_program(DOTNET_EXEC dotnet REQUIRED)
add_custom_target(LumiUI ALL
    COMMAND ${DOTNET_EXEC} publish ${CMAKE_SOURCE_DIR}/ui/Lumi.csproj
            -c Release
            -r win-x64
            --self-contained false
            -o ${CMAKE_BINARY_DIR}/bin/
    DEPENDS lumi_core
    COMMENT "Building Lumi WPF UI..."
)
```

### 7.2 `core/CMakeLists.txt`

```cmake
find_package(OpenSSL REQUIRED)

# ZXing-C++ submodule
add_subdirectory(third_party/zxing-cpp EXCLUDE_FROM_ALL)

# nanopb
set(NANOPB_SRC third_party/nanopb)
add_library(nanopb STATIC ${NANOPB_SRC}/pb_encode.c ${NANOPB_SRC}/pb_decode.c ${NANOPB_SRC}/pb_common.c)

# Generate protobuf sources
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/migration_payload.pb.c
           ${CMAKE_CURRENT_BINARY_DIR}/migration_payload.pb.h
    COMMAND python ${NANOPB_SRC}/generator/nanopb_generator.py
            ${CMAKE_SOURCE_DIR}/proto/migration_payload.proto
            -D ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/proto/migration_payload.proto
)

add_library(lumi_core SHARED
    src/api.cpp
    src/totp.cpp
    src/hotp.cpp
    src/qr_decoder.cpp
    src/migration_parser.cpp
    src/vault.cpp
    src/base32.cpp
    src/hmac_sha1.cpp
    src/ntp_client.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/migration_payload.pb.c
)

target_include_directories(lumi_core PUBLIC include PRIVATE src ${CMAKE_CURRENT_BINARY_DIR} ${NANOPB_SRC})
target_link_libraries(lumi_core PRIVATE OpenSSL::Crypto ZXing nanopb Crypt32)
target_compile_definitions(lumi_core PRIVATE LUMI_EXPORTS)

# Install DLL next to the exe
install(TARGETS lumi_core RUNTIME DESTINATION .)
```

### 7.3 `ui/Lumi.csproj`

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <UseWPF>true</UseWPF>
    <ApplicationIcon>Assets\lumi-icon.ico</ApplicationIcon>
    <AssemblyName>Lumi</AssemblyName>
    <RootNamespace>Lumi</RootNamespace>
    <Nullable>enable</Nullable>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
  </PropertyGroup>

  <ItemGroup>
    <!-- Copy the native DLL to output -->
    <Content Include="..\build\bin\lumi_core.dll">
      <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </Content>
  </ItemGroup>

  <ItemGroup>
    <PackageReference Include="CommunityToolkit.Mvvm"    Version="8.3.2"/>
    <PackageReference Include="WPF-UI"                   Version="3.0.4"/>
    <PackageReference Include="Microsoft.Xaml.Behaviors.Wpf" Version="1.1.135"/>
  </ItemGroup>
</Project>
```

### 7.4 One-Shot Build Script (`scripts/build.ps1`)

```powershell
param(
    [string]$Config = "Release",
    [string]$BuildDir = "build"
)

# Configure CMake
cmake -B $BuildDir -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=$Config

# Build everything (core DLL + triggers dotnet publish)
cmake --build $BuildDir --config $Config --parallel

Write-Host "`n✅ Build complete. Output: $BuildDir\bin\" -ForegroundColor Green
```

---

## 8. Feature Specification

### 8.1 Core Features (v1.0)

| Feature | Description | Implementation |
|---------|-------------|----------------|
| **QR Image Import** | Load .png/.jpg file → decode → import accounts | ZXing-C++ + nanopb |
| **Live TOTP Display** | Codes update in real time | 200ms DispatcherTimer |
| **Countdown Ring** | Circular SVG arc shows time remaining | ArcSegment + converter |
| **Code Copy** | Click code → copied to clipboard → 2s feedback | Clipboard API |
| **Account Rename** | Double-click name → inline text edit → Enter saves | In-place TextBox |
| **Search** | Real-time filter by name or issuer | LINQ Where + ObservableCollection |
| **Drag to Reorder** | Drag account cards to rearrange | ListView drag behavior |
| **Dark / Light Mode** | Follows Windows system theme + manual override | SystemParameters + ThemeService |
| **Encrypted Vault** | AES-256-GCM + DPAPI key sealing | OpenSSL + Windows Crypt32 |
| **Manual Add** | Enter secret key manually | Wizard Page 2 |
| **otpauth:// URI** | Paste a URI directly | lumi_parse_otpauth_uri |

### 8.2 Extended Features (v1.1)

| Feature | Description |
|---------|-------------|
| **Delete Account** | Swipe-to-delete or right-click context menu |
| **Issuer Icons** | Auto-fetch service favicon / bundled icon pack |
| **NTP Sync** | Correct time drift using pool.ntp.org |
| **Backup Export** | Export encrypted `.lumi` backup file |
| **Backup Import** | Import from `.lumi` backup |
| **System Tray** | Minimize to tray; show current codes in tray popup |
| **Auto-lock** | Lock vault after N minutes of inactivity (PIN or Windows Hello) |
| **Windows Hello** | Use fingerprint/face unlock instead of DPAPI only |
| **HOTP Support** | Counter-based OTP with manual increment button |
| **8-digit codes** | Already supported in core; ensure UI handles display |
| **60-second period** | Some services use 60s; handled in core |

### 8.3 Future Possibilities (v2.0)

| Feature | Description |
|---------|-------------|
| **Camera QR Scan** | DirectShow / Windows.Media.Capture integration |
| **Multi-device sync** | Optional encrypted sync via user-provided cloud storage |
| **Browser Extension** | Auto-fill codes via a companion Edge/Chrome extension |
| **CLI companion** | `lumi-cli.exe` using the same core DLL |

---

## 9. UI/UX Design Specification

### 9.1 Visual Design Language

Lumi uses **Fluent Design** principles: depth, motion, material, light. The aesthetic is calm and focused — no gradients, no unnecessary chrome.

**Window:** 380 × 680 px default, resizable. Minimum 340 × 500 px. Custom titlebar with `WindowChrome` (no system chrome). Rounded corners via `WindowCornerPreference`.

### 9.2 Color System

```
Dark Theme                          Light Theme
─────────────────────────────────   ─────────────────────────────────
Background:      #0F0F10            Background:      #F3F3F3
Surface:         #1A1A1C            Surface:         #FFFFFF
Surface Alt:     #242428            Surface Alt:     #F0F0F0
Border:          #2E2E32            Border:          #E0E0E0
Text Primary:    #F0F0F5            Text Primary:    #1A1A1A
Text Secondary:  #8888A0            Text Secondary:  #606070
Accent:          #6E6BFF            Accent:          #5B58FF  (purple-blue)
Accent Dim:      #6E6BFF33          Accent Dim:      #5B58FF22
Danger:          #FF5C5C            Danger:          #E03030
Code font:       JetBrains Mono     Code font:       JetBrains Mono
UI font:         Inter Variable     UI font:         Inter Variable
```

### 9.3 AccountCard States

```
┌─────────────────────────────────────────────────────┐
│  [GH]  GitHub             │   456 789   [ ◯ 14 ]   │   Normal
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│  [GH]  GitHub             │   456 789   [ ◯ 03 ]   │   Expiring (code turns red)
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│  [GH]  [__GitHub_______]  │   456 789   [ ◯ 22 ]   │   Editing name (inline)
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│  [GH]  GitHub             │   ✓ Copied  [ ◯ 22 ]   │   After copy (2s flash)
└─────────────────────────────────────────────────────┘
```

### 9.4 Countdown Ring Converter

```csharp
public class TimeToArcConverter : IValueConverter {
    public object Convert(object value, ...) {
        double progress = (double)value;  // 0.0 to 1.0 (SecondsLeft / Period)
        double angle = (1.0 - progress) * 360.0;  // Depletes clockwise
        if (angle >= 360) angle = 359.99;

        double cx = 22, cy = 22, r = 20;
        double rad = (angle - 90) * Math.PI / 180.0;
        double x = cx + r * Math.Cos(rad);
        double y = cy + r * Math.Sin(rad);
        bool largeArc = angle > 180;

        return $"M {cx},{cy - r} A {r},{r} 0 {(largeArc ? 1 : 0)},1 {x:F2},{y:F2}";
    }
}
```

### 9.5 Animations

| Trigger | Animation |
|---------|-----------|
| App launch | Cards fade + slide in from bottom, staggered 50ms |
| Code refresh | Code digits fade-out / fade-in (300ms) |
| Copy feedback | "Copied!" overlay fades in 150ms, out 200ms after 1.5s |
| Card hover | Background brightens 4% (200ms ease) |
| Add account | New card slides in from top (400ms spring ease) |
| Delete | Card slides out right + collapses height (300ms) |
| Name edit mode | Border appears, background dims slightly |

---

## 10. Data Storage & Security

### 10.1 File Locations

```
%APPDATA%\Lumi\
  vault.lumi        ← Encrypted vault (accounts + secrets)
  settings.json     ← Non-sensitive settings (theme, sort order, etc.)
  logs\             ← Optional debug logs (never log secrets)
```

### 10.2 Security Properties

| Property | Implementation |
|----------|---------------|
| **Secrets at rest** | AES-256-GCM encrypted; key sealed via DPAPI |
| **User-bound** | DPAPI `CRYPTPROTECT_LOCAL_MACHINE = false` → only current Windows user |
| **No cloud** | Vault never transmitted; 100% local by default |
| **Memory safety** | Secrets zeroed from memory after use (`SecureZeroMemory`) |
| **No logging** | Secret bytes never written to logs or error messages |
| **Code display** | Codes shown only while app is visible; cleared on minimize (optional setting) |
| **Clipboard clear** | Clipboard auto-cleared after 30 seconds (configurable) |

### 10.3 Settings (`settings.json`)

```json
{
  "theme": "system",
  "clearClipboardAfterSeconds": 30,
  "autoLockAfterMinutes": 0,
  "showCodesOnMinimize": false,
  "ntpSyncEnabled": true,
  "ntpServer": "pool.ntp.org",
  "sortOrder": "manual",
  "accountOrder": ["uuid1", "uuid2", "uuid3"]
}
```

---

## 11. Dependencies & Third-Party Libraries

### C++ Core Dependencies

| Library | Purpose | License | Integration |
|---------|---------|---------|-------------|
| **ZXing-C++** | QR code decoding | Apache 2.0 | Git submodule |
| **nanopb** | Protobuf decode (tiny footprint) | zlib | Git submodule |
| **OpenSSL** (libcrypto) | HMAC-SHA1/256/512, AES-256-GCM | Apache 2.0 | vcpkg or prebuilt |
| **stb_image** | PNG/JPG loading for QR input | MIT | Single header |
| **Windows Crypt32** | DPAPI (`CryptProtectData`) | OS SDK | System |

### C# UI Dependencies (NuGet)

| Package | Purpose | Version |
|---------|---------|---------|
| **CommunityToolkit.Mvvm** | RelayCommand, ObservableObject | 8.x |
| **WPF-UI** (wpfui) | Fluent controls, titlebar, navigation | 3.x |
| **Microsoft.Xaml.Behaviors.Wpf** | XAML interaction triggers | 1.x |

> **Note on WPF-UI:** The `wpfui` NuGet package by Lepo.co provides ready-made Fluent Design controls for WPF including `TitleBar`, `NavigationView`, `Card`, `InfoBar`, and a complete dark/light theme system. It eliminates 80% of manual styling work.

---

## 12. Implementation Phases & Roadmap

### Phase 1 — Foundation (Weeks 1–2)
- [ ] Set up repo, CMake, .sln structure, Git submodules
- [ ] Write `lumi_core.h` public API header
- [ ] Implement Base32, HMAC-SHA1, TOTP engine
- [ ] Write unit tests for TOTP against RFC 6238 test vectors
- [ ] Implement basic vault (load/save JSON, no crypto yet)
- [ ] Minimal WPF window with P/Invoke calls verified

### Phase 2 — QR Import Pipeline (Weeks 3–4)
- [ ] Integrate ZXing-C++ and verify QR decode from test images
- [ ] Decode the attached migration QR image → raw protobuf bytes
- [ ] Integrate nanopb and decode protobuf → account structs
- [ ] Handle both `otpauth-migration://` and `otpauth://` URI formats
- [ ] Test with real Google Authenticator export QR codes
- [ ] Add WPF file picker → call lumi_decode_qr_file → show preview

### Phase 3 — Full UI (Weeks 5–6)
- [ ] Implement AccountCard with countdown ring
- [ ] Live code updates via DispatcherTimer
- [ ] Copy-to-clipboard with visual feedback
- [ ] Inline name editing (double-click to edit)
- [ ] Drag-to-reorder accounts
- [ ] Search/filter
- [ ] Dark/light theme with system detection
- [ ] Add Account wizard (3 pages)

### Phase 4 — Security & Storage (Week 7)
- [ ] Add AES-256-GCM encryption to vault
- [ ] Integrate DPAPI key sealing
- [ ] Memory zeroing for secret bytes
- [ ] Clipboard auto-clear timer
- [ ] Settings window + persistence

### Phase 5 — Polish & Distribution (Week 8)
- [ ] Issuer icon pack (SVG icons for common services)
- [ ] Animations (card entrance, code refresh, copy feedback)
- [ ] System tray support
- [ ] Custom window chrome (rounded corners, shadow)
- [ ] Inno Setup installer script
- [ ] App icon design
- [ ] Code signing (self-signed for dev, real cert for release)

---

## 13. Testing Strategy

### C++ Unit Tests (Catch2)

```cpp
// test_totp.cpp — RFC 6238 Appendix B test vectors
TEST_CASE("TOTP SHA1 reference vectors") {
    // Secret = ASCII "12345678901234567890"
    const uint8_t secret[] = "12345678901234567890";
    REQUIRE(compute_totp(secret, 20, 59,          30, 8, SHA1) == 94287082);
    REQUIRE(compute_totp(secret, 20, 1111111109,  30, 8, SHA1) == 7081804);
    REQUIRE(compute_totp(secret, 20, 1234567890,  30, 8, SHA1) == 89005924);
    REQUIRE(compute_totp(secret, 20, 2000000000,  30, 8, SHA1) == 69279037);
}

TEST_CASE("QR decode produces valid URI") {
    auto result = decode_qr_from_file("tests/fixtures/ga_export.png");
    REQUIRE(result.starts_with("otpauth-migration://offline?data="));
}

TEST_CASE("Migration parse extracts accounts") {
    auto accounts = parse_migration_uri(KNOWN_MIGRATION_URI);
    REQUIRE(accounts.size() == 3);
    REQUIRE(accounts[0].digits == 6);
    REQUIRE(accounts[0].type == 0); // TOTP
}
```

### C# Integration Tests (xUnit)

```csharp
[Fact]
public void GetTotp_ReturnsValidCode() {
    var acc = TestFixtures.GitHubAccount;
    var code = VaultManager.GetCode(acc);
    Assert.Matches(@"^\d{6}$", code);
}

[Fact]
public void ImportQr_LoadsAccounts() {
    var accounts = NativeInterop.DecodeQrFile("fixtures/ga_export.png");
    Assert.NotEmpty(accounts);
    Assert.All(accounts, a => Assert.Equal(6, a.Digits));
}
```

---

## 14. Distribution

### Installer (Inno Setup)

```iss
[Setup]
AppName=Lumi
AppVersion=1.0.0
DefaultDirName={autopf}\Lumi
DefaultGroupName=Lumi
OutputDir=dist
OutputBaseFilename=lumi-setup-1.0.0
Compression=lzma2
SolidCompression=yes

[Files]
Source: "build\bin\Lumi.exe";      DestDir: "{app}"
Source: "build\bin\lumi_core.dll"; DestDir: "{app}"
Source: "build\bin\*.dll";         DestDir: "{app}"  ; Flags: recursesubdirs

[Icons]
Name: "{group}\Lumi";              Filename: "{app}\Lumi.exe"
Name: "{commondesktop}\Lumi";      Filename: "{app}\Lumi.exe"

[Run]
Filename: "{app}\Lumi.exe"; Description: "Launch Lumi"; Flags: postinstall
```

### Minimum System Requirements

| Component | Requirement |
|-----------|-------------|
| OS | Windows 10 21H2 or later |
| Architecture | x64 |
| Runtime | .NET 8 Runtime (or self-contained publish) |
| VC++ Redist | Visual C++ 2022 Redistributable |
| RAM | 64 MB |
| Disk | 30 MB |

---

*End of Lumi Implementation Plan — v1.0*