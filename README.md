# Lumi 프로젝트

## 개요

> "가장 간편하고 좋은 인증기를 윈도우 환경에서 편하게 쓸 수는 없을까?"

위 질문에서 시작한 프로젝트입니다. 로컬 기반으로 안전하게 데이터를 보호하며, 금고 보안과 QR 코드를 통한 간편한 마이그레이션을 제공하는 **Windows 네이티브 TOTP 인증기**를 개발하고 있습니다.

이 프로젝트는 **LUMOS STUDIO의 버랜좌**가 개발하였습니다.

<p align="center">
  <img alt="Stars" src="https://img.shields.io/github/stars/kotae-dev/Lumi?style=for-the-badge&labelColor=0d1117&color=ffd700&logo=github&logoColor=white" />
  <img alt="License" src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" />
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%2010%2B-black?style=flat-square" />
  <img alt="C++" src="https://img.shields.io/badge/core-C++20-00599C?style=flat-square&logo=c%2B%2B" />
  <img alt=".NET" src="https://img.shields.io/badge/ui-.NET%2010%20WPF-512BD4?style=flat-square&logo=.net" />
</p>

## 합리화

대부분의 최신 인증기 앱은 모바일 기기에 종속되어 있거나, 구독이 필요한 클라우드 동기화 방식이거나, 너무 무거운 데스크톱 앱으로 제작되어 있습니다.

이에, 우리는 **Lumi를 Windows 네이티브 대안으로 제안합니다.** 클라우드 동기화를 전혀 사용하지 않으며, 암호화된 로컬 금고에 모든 데이터를 안전하게 저장합니다. 또한 순수 `BCrypt`를 사용하여 매우 빠른 속도로 TOTP/HOTP를 생성할 수 있도록 만들었습니다.

더불어, ZXing-C++를 통한 네이티브 QR 코드 디코딩 기능과 내장된 프로토콜 버퍼 파싱 기능을 제공하여, 서드파티 온라인 변환기 없이도 Google Authenticator 마이그레이션 페이로드를 직접 가져올 수 있게 했습니다.

## 목표와 현황

* 1차 목표 - Windows 환경에서 빠르고 가볍게 쓸 수 있는 완벽한 TOTP 인증기 앱 출시
* 주요 기능 제공 현황:
  * **코어(Core):** C++20 기반으로 강력한 성능을 내며, 암호화 작업 시 지연이 없습니다.
  * **암호화(Cryptography):** Windows 네이티브 Crypto API를 사용합니다.
  * **금고(Vault):** 로컬 DPAPI 암호화 금고를 사용하여 키가 외부로 유출되지 않습니다.
  * **가져오기(Import):** Google Authenticator QR 마이그레이션 파싱 및 ZXing-C++ QR 디코딩을 지원합니다.
  * **시간 동기화(Time Sync):** 내장 C++ NTP 클라이언트를 통해 TOTP 시간 오차를 방지합니다.
  * **사용자 인터페이스(UI):** `CommunityToolkit.Mvvm`과 `WPF-UI`를 활용한 모던한 프론트엔드를 구성했습니다.

## 어떻게 동작하는가?

Lumi는 C++20으로 빌드된 코어 라이브러리와 C# .NET 10 WPF 기반의 프론트엔드로 나뉘어 동작합니다. 프론트엔드는 P/Invoke를 통해 `lumi_core.dll`과 통신하며, 코어에서는 네이티브 Windows `Bcrypt`와 `Crypt32`를 통해 데이터를 암호화하고 복호화합니다.

```
┌────────────────────── Windows (WPF .NET 10) ─────────────────────┐
│  MVVM UI · Settings · QR Scanner · Vault View · Copy Feedback    │
└──────────────┬───────────────────────────────────┬───────────────┘
               │ P/Invoke (DllImport)              │
               ▼                                   ▼
   ┌─────────────────────────────────────────────────────────────┐
   │  lumi_core.dll (C++20)                                      │
   │  api.cpp             base32.cpp        qr_decoder.cpp       │
   │  totp.cpp / hotp.cpp hmac_sha.cpp      ntp_client.cpp       │
   │  vault.cpp           uuid.cpp          migration_parser.cpp │
   └─────────┬────────────────────────┬──────────────────────────┘
             ▼                        ▼
   ┌──────────────────┐     ┌──────────────────┐
   │ DPAPI (Crypt32)  │     │ BCrypt (AES/HMAC)│
   └──────────────────┘     └──────────────────┘
```

## 이용 방법

Lumi는 CMake와 .NET SDK를 사용하여 네이티브로 직접 빌드할 수 있습니다. 

```bash
git clone https://github.com/kotae-dev/Lumi.git
cd Lumi
git submodule update --init --recursive
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

CMake의 빌드 후 처리 단계에서 자동으로 `dotnet publish`를 실행하여 WPF UI를 빌드하고, `lumi_core.dll`과 함께 `build/bin/` 디렉터리에 패키징해 줍니다. 
(Visual Studio 2026 또는 호환 가능한 C++20 툴체인과 .NET 10 SDK가 필요합니다.)

## 한계

지금 방식은 오직 Windows 10 이상의 운영체제에서만 동작합니다. 또한 완벽한 로컬 기반이기에 기기를 분실하거나 로컬 데이터를 삭제하면 복구할 수 없는 단점이 있습니다. 이러한 명백한 기능적 한계점들을 안고 시작하지만, 저희가 목표하는 바를 달성하는 데에는 충분하리라 기대합니다.

감사합니다.