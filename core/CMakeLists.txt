cmake_minimum_required(VERSION 3.25)

# ─── ZXing-C++ submodule ──────────────────────────────────────────
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zxing-cpp/CMakeLists.txt)
    set(ZXING_READERS ON CACHE BOOL "" FORCE)
    set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
    set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/zxing-cpp EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "ZXing-C++ submodule not found — run: git submodule update --init --recursive")
endif()

# ─── nanopb (optional — migration_parser.cpp has a hand-rolled decoder) ─
set(NANOPB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanopb)
if(EXISTS ${NANOPB_SRC}/pb_decode.c)
    add_library(nanopb STATIC
        ${NANOPB_SRC}/pb_encode.c
        ${NANOPB_SRC}/pb_decode.c
        ${NANOPB_SRC}/pb_common.c
    )
    target_include_directories(nanopb PUBLIC ${NANOPB_SRC})
    set(HAVE_NANOPB TRUE)
else()
    message(STATUS "nanopb not found — hand-rolled protobuf decoder will be used")
    set(HAVE_NANOPB FALSE)
endif()

# ─── Core DLL ─────────────────────────────────────────────────────
add_library(lumi_core SHARED
    src/api.cpp
    src/totp.cpp
    src/hotp.cpp
    src/qr_decoder.cpp
    src/migration_parser.cpp
    src/vault.cpp
    src/base32.cpp
    src/hmac_sha.cpp
    src/ntp_client.cpp
    src/uuid.cpp
)

target_include_directories(lumi_core
    PUBLIC  include
    PRIVATE src
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Windows native crypto — no OpenSSL needed
target_link_libraries(lumi_core
    PRIVATE
        ZXing
        Crypt32       # DPAPI (CryptProtectData / CryptUnprotectData)
        Ws2_32        # NTP client sockets
        Bcrypt        # AES-GCM, HMAC, secure random
)

if(HAVE_NANOPB)
    target_link_libraries(lumi_core PRIVATE nanopb)
    target_include_directories(lumi_core PRIVATE ${NANOPB_SRC})
endif()

target_compile_definitions(lumi_core PRIVATE LUMI_EXPORTS)

# Install DLL next to the exe
install(TARGETS lumi_core RUNTIME DESTINATION .)
