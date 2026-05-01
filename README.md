# Lumi

> **The Windows-native TOTP authenticator.** Local-first, vault-secured — **C++ core** meets a **WPF .NET 10** frontend. Designed for speed, security with DPAPI/Bcrypt, and seamless Google Authenticator QR migration.

<p align="center">
  <img alt="Stars" src="https://img.shields.io/github/stars/user/Lumi?style=for-the-badge&labelColor=0d1117&color=ffd700&logo=github&logoColor=white" />
  <img alt="License" src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%2010%2B-black?style=flat-square" />
  <img alt="C++" src="https://img.shields.io/badge/core-C++20-00599C?style=flat-square&logo=c%2B%2B" />
  <img alt=".NET" src="https://img.shields.io/badge/ui-.NET%2010%20WPF-512BD4?style=flat-square&logo=.net" />
</p>

---

## Why this exists

Most modern authenticator apps are locked to mobile devices, cloud-synced behind subscriptions, or built as heavy Electron desktop apps. 

**Lumi is the Windows-native alternative.** It uses zero cloud sync, stores everything securely in a local vault encrypted via Windows DPAPI, and uses raw `BCrypt` for lightning-fast TOTP/HOTP generation. 

Lumi also features native QR code decoding via ZXing-C++ and built-in protocol buffer parsing. It can seamlessly import Google Authenticator migration payloads (`otpauth-migration://` URIs) without needing third-party online converters.

## At a glance

| | What you get |
|---|---|
| **Core** | C++20 `lumi_core.dll` — raw performance, zero garbage collection pauses for crypto operations. |
| **Cryptography** | Windows Native Crypto API (`Crypt32`, `Bcrypt`). No heavy OpenSSL dependency. |
| **Vault** | Local DPAPI encrypted vault — keys never leave your machine in plaintext. |
| **Import** | Google Authenticator QR migration parsing (via `nanopb` or hand-rolled protobuf decoder) + ZXing C++ QR decoding. |
| **Time Sync** | Built-in C++ NTP Client (`Ws2_32`) to prevent TOTP drift. |
| **User Interface** | .NET 10 WPF frontend utilizing `CommunityToolkit.Mvvm` and `WPF-UI` for a modern, fluid experience. |

## Architecture

```
┌────────────────────── Windows (WPF .NET 10) ─────────────────────┐
│  MVVM UI · Settings · QR Scanner · Vault View · Copy Feedback    │
└──────────────┬───────────────────────────────────┬───────────────┘
               │ P/Invoke (DllImport)              │
               ▼                                   ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  lumi_core.dll (C++20)                                      │
   │                                                             │
   │  api.cpp             base32.cpp        qr_decoder.cpp       │
   │  totp.cpp / hotp.cpp hmac_sha.cpp      ntp_client.cpp       │
   │  vault.cpp           uuid.cpp          migration_parser.cpp │
   │                                                             │
   │  [ZXing-C++]         [nanopb]                               │
   └─────────┬────────────────────────┬──────────────────────────┘
             │                        │
             ▼                        ▼
   ┌──────────────────┐     ┌──────────────────┐
   │ DPAPI (Crypt32)  │     │ BCrypt (AES/HMAC)│
   └──────────────────┘     └──────────────────┘
```

| Layer | Stack |
|---|---|
| Frontend | C# · .NET 10 Windows Presentation Foundation (WPF) |
| UI Framework | `CommunityToolkit.Mvvm` 8.3.2 + `WPF-UI` 3.0.4 |
| Core Library | C++20, compiled as `lumi_core.dll` |
| Cryptography | Native Windows `Bcrypt` (AES-GCM, HMAC, secure random) |
| Vault Security | Native Windows `Crypt32` (DPAPI: `CryptProtectData` / `CryptUnprotectData`) |
| Network / Sync | `Ws2_32` for NTP client sockets |

## Quickstart

Lumi is built natively using CMake and the .NET SDK.

```bash
git clone https://github.com/yourusername/Lumi.git
cd Lumi
git submodule update --init --recursive
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

The post-build step in CMake automatically triggers `dotnet publish` to build the WPF UI and packages it alongside `lumi_core.dll` into `build/bin/`.

> **Note:** Requires Visual Studio 18 Community 2026 or a compatible C++20 toolchain, plus the .NET 10 SDK.

## Repository structure

```
Lumi/
├── README.md                      ← this file
├── CMakeLists.txt                 ← Main CMake build definition
│
├── core/                          ← C++20 Core Library (lumi_core.dll)
│   ├── CMakeLists.txt             
│   ├── include/                   ← Public C++ API headers
│   ├── src/                       ← C++ implementation (totp, vault, crypto)
│   │   ├── api.cpp                ← P/Invoke boundaries
│   │   ├── migration_parser.cpp   ← Google Authenticator protobuf parser
│   │   └── ...
│   └── third_party/               
│       ├── zxing-cpp/             ← ZXing QR code decoder submodule
│       └── nanopb/                ← Optional nanopb submodule
│
└── ui/                            ← .NET 10 WPF Frontend
    ├── Lumi.csproj                ← C# project definition
    ├── App.xaml                   ← Application entry
    ├── Assets/                    ← Icons, Fonts (JetBrainsMono)
    ├── Converters/                ← XAML Value Converters
    ├── Core/                      ← P/Invoke Wrappers for lumi_core.dll
    ├── Services/                  ← UI Services
    ├── Styles/                    ← XAML Resource Dictionaries
    ├── ViewModels/                ← MVVM ViewModels
    └── Views/                     ← MVVM Views (Pages/Windows)
```