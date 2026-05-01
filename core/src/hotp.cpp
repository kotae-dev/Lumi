#include "totp.h"

// HOTP is just the raw counter-based OTP — the implementation
// is shared with TOTP in totp.cpp via compute_otp_internal.
// This file exists for linker compatibility; all logic lives in totp.cpp.
