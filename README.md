# Lumi

> **Windows 네이티브 TOTP 인증기.** 로컬 우선, 금고(Vault) 보안 — **C++ 코어**와 **WPF .NET 10** 프론트엔드의 만남. DPAPI/Bcrypt를 활용한 속도와 보안, 그리고 매끄러운 Google Authenticator QR 마이그레이션을 위해 설계되었습니다.
>
> 💡 **LUMOS STUDIO의 버랜좌가 개발하였습니다.**

<p align="center">
  <img alt="Stars" src="https://img.shields.io/github/stars/kotae-dev/Lumi?style=for-the-badge&labelColor=0d1117&color=ffd700&logo=github&logoColor=white" />
  <img alt="License" src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%2010%2B-black?style=flat-square" />
  <img alt="C++" src="https://img.shields.io/badge/core-C++20-00599C?style=flat-square&logo=c%2B%2B" />
  <img alt=".NET" src="https://img.shields.io/badge/ui-.NET%2010%20WPF-512BD4?style=flat-square&logo=.net" />
</p>

---

## 제작 배경

대부분의 최신 인증기 앱은 모바일 기기에 종속되어 있거나, 구독이 필요한 클라우드 동기화 방식이거나, 무거운 Electron 기반 데스크톱 앱으로 제작되어 있습니다.

**Lumi는 Windows 네이티브 대안입니다.** 클라우드 동기화를 전혀 사용하지 않으며, Windows DPAPI를 통해 암호화된 로컬 금고(Vault)에 모든 데이터를 안전하게 저장하고, 순수 `BCrypt`를 사용하여 매우 빠른 속도로 TOTP/HOTP를 생성합니다.

또한 Lumi는 ZXing-C++를 통한 네이티브 QR 코드 디코딩 기능과 내장된 프로토콜 버퍼(protobuf) 파싱 기능을 제공합니다. 서드파티 온라인 변환기 없이도 Google Authenticator 마이그레이션 페이로드(`otpauth-migration://` URI)를 매끄럽게 가져올 수 있습니다.

## 주요 기능

| | 설명 |
|---|---|
| **코어(Core)** | C++20 `lumi_core.dll` — 강력한 성능, 암호화 작업 시 가비지 컬렉션으로 인한 지연(pause) 없음. |
| **암호화(Cryptography)** | Windows 네이티브 Crypto API (`Crypt32`, `Bcrypt`). 무거운 OpenSSL 종속성 없음. |
| **금고(Vault)** | 로컬 DPAPI 암호화 금고 — 암호화 키가 평문 형태로 기기 외부로 유출되지 않음. |
| **가져오기(Import)** | Google Authenticator QR 마이그레이션 파싱 (`nanopb` 또는 자체 제작 protobuf 디코더 활용) + ZXing C++ QR 디코딩. |
| **시간 동기화(Time Sync)** | 내장 C++ NTP 클라이언트 (`Ws2_32`)를 통한 TOTP 시간 오차 방지. |
| **사용자 인터페이스(UI)** | `CommunityToolkit.Mvvm`과 `WPF-UI`를 활용하여 모던하고 부드러운 경험을 제공하는 .NET 10 WPF 프론트엔드. |

## 아키텍처

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

| 계층 (Layer) | 스택 (Stack) |
|---|---|
| 프론트엔드 (Frontend) | C# · .NET 10 Windows Presentation Foundation (WPF) |
| UI 프레임워크 | `CommunityToolkit.Mvvm` 8.3.2 + `WPF-UI` 3.0.4 |
| 코어 라이브러리 | C++20, `lumi_core.dll`로 빌드됨 |
| 암호화 | 네이티브 Windows `Bcrypt` (AES-GCM, HMAC, secure random) |
| 금고 보안 | 네이티브 Windows `Crypt32` (DPAPI: `CryptProtectData` / `CryptUnprotectData`) |
| 네트워크 / 동기화 | NTP 클라이언트 소켓용 `Ws2_32` |

## 빠른 시작

Lumi는 CMake와 .NET SDK를 사용하여 네이티브로 빌드됩니다.

```bash
git clone https://github.com/kotae-dev/Lumi.git
cd Lumi
git submodule update --init --recursive
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

CMake의 빌드 후 처리(post-build) 단계에서 자동으로 `dotnet publish`를 실행하여 WPF UI를 빌드하고, `lumi_core.dll`과 함께 `build/bin/` 디렉터리에 패키징합니다.

> **참고:** Visual Studio 18 Community 2026 또는 호환 가능한 C++20 툴체인, 그리고 .NET 10 SDK가 필요합니다.

## 리포지토리 구조

```
Lumi/
├── README.md                      ← 현재 파일
├── CMakeLists.txt                 ← 메인 CMake 빌드 정의 파일
│
├── core/                          ← C++20 코어 라이브러리 (lumi_core.dll)
│   ├── CMakeLists.txt             
│   ├── include/                   ← 공개 C++ API 헤더
│   ├── src/                       ← C++ 구현부 (totp, vault, crypto)
│   │   ├── api.cpp                ← P/Invoke 경계
│   │   ├── migration_parser.cpp   ← Google Authenticator protobuf 파서
│   │   └── ...
│   └── third_party/               
│       ├── zxing-cpp/             ← ZXing QR 코드 디코더 서브모듈
│       └── nanopb/                ← 선택적 nanopb 서브모듈
│
└── ui/                            ← .NET 10 WPF 프론트엔드
    ├── Lumi.csproj                ← C# 프로젝트 정의 파일
    ├── App.xaml                   ← 애플리케이션 진입점
    ├── Assets/                    ← 아이콘, 폰트 (JetBrainsMono)
    ├── Converters/                ← XAML 값 변환기 (Value Converters)
    ├── Core/                      ← lumi_core.dll을 위한 P/Invoke 래퍼
    ├── Services/                  ← UI 서비스
    ├── Styles/                    ← XAML 리소스 딕셔너리
    ├── ViewModels/                ← MVVM 뷰모델
    └── Views/                     ← MVVM 뷰 (페이지/창)
```