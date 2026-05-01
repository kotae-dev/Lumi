#include <cstdint>
#include <ctime>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// NTP timestamp offset (Jan 1 1900 -> Jan 1 1970)
static constexpr uint32_t NTP_EPOCH_OFFSET = 2208988800UL;

static int64_t g_ntp_offset = 0;       // Offset in seconds
static bool    g_ntp_synced = false;

// Simple NTP client — queries a single NTP server for time correction.
// Returns offset in seconds, or 0 on failure.
static int64_t query_ntp(const char* server, int timeout_ms = 3000) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;

    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(server, "123", &hints, &result) != 0) {
        WSACleanup();
        return 0;
    }

    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        WSACleanup();
        return 0;
    }

    // Set timeout
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    // NTP packet (48 bytes, LI=0, VN=4, Mode=3 client)
    uint8_t packet[48] = {};
    packet[0] = 0x23; // LI=0, VN=4, Mode=3

    // Record local time before send
    int64_t t1 = static_cast<int64_t>(time(nullptr));

    if (sendto(sock, reinterpret_cast<const char*>(packet), 48, 0,
               result->ai_addr, static_cast<int>(result->ai_addrlen)) != 48) {
        closesocket(sock);
        freeaddrinfo(result);
        WSACleanup();
        return 0;
    }

    // Receive response
    int recv_len = recv(sock, reinterpret_cast<char*>(packet), 48, 0);
    closesocket(sock);
    freeaddrinfo(result);
    WSACleanup();

    if (recv_len < 48) return 0;

    // Extract transmit timestamp (bytes 40-43 = seconds)
    uint32_t ntp_secs =
        (static_cast<uint32_t>(packet[40]) << 24) |
        (static_cast<uint32_t>(packet[41]) << 16) |
        (static_cast<uint32_t>(packet[42]) << 8)  |
        (static_cast<uint32_t>(packet[43]));

    int64_t server_time = static_cast<int64_t>(ntp_secs) - NTP_EPOCH_OFFSET;
    return server_time - t1;
}

// Exported: sync with NTP server
extern "C" {

__declspec(dllexport)
int lumi_ntp_sync(const char* server) {
    if (!server || !server[0]) server = "pool.ntp.org";

    int64_t offset = query_ntp(server);
    // Only accept if the response seems reasonable (< 24h drift)
    if (offset > -86400 && offset < 86400) {
        g_ntp_offset = offset;
        g_ntp_synced = true;
        return 0;
    }
    return -1;
}

__declspec(dllexport)
int64_t lumi_get_corrected_time(void) {
    return static_cast<int64_t>(time(nullptr)) + g_ntp_offset - 1;
}

} // extern "C"

int64_t get_current_time_with_ntp() {
    return static_cast<int64_t>(time(nullptr)) + g_ntp_offset - 1;
}
