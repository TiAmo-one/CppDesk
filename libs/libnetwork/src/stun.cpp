#include "libnetwork/stun.h"
#include <cstring>
#include <ws2tcpip.h>

static constexpr uint32_t STUN_MAGIC = 0x2112A442;

namespace network::stun {

static bool SendBindingRequest(SOCKET sock, const char* server, uint16_t port) {
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server, &addr.sin_addr);

    uint8_t req[20] = {};
    req[0] = 0x00; req[1] = 0x01; // Binding Request
    for (int i = 8; i < 20; i++) req[i] = (uint8_t)(rand() & 0xFF);
    req[4] = 0x21; req[5] = 0x12; req[6] = 0xA4; req[7] = 0x42; // magic cookie

    return sendto(sock, (const char*)req, sizeof(req), 0,
                  (const sockaddr*)&addr, sizeof(addr)) == sizeof(req);
}

bool GetMappedAddress(SOCKET sock, const char* stunServer, uint16_t stunPort,
                       sockaddr_in& mappedAddr) {
    memset(&mappedAddr, 0, sizeof(mappedAddr));
    if (!SendBindingRequest(sock, stunServer, stunPort)) return false;

    uint8_t buf[256];
    sockaddr_in from = {};
    int fromLen = sizeof(from);

    fd_set fds;
    FD_ZERO(&fds); FD_SET(sock, &fds);
    timeval tv = { 2, 0 };
    if (select(0, &fds, nullptr, nullptr, &tv) <= 0) return false;

    int n = recvfrom(sock, (char*)buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
    if (n < 20) return false;

    if (buf[0] != 0x01 || buf[1] != 0x01) return false; // not Binding Success
    uint32_t* cookie = (uint32_t*)(buf + 4);
    if (ntohl(*cookie) != STUN_MAGIC) return false;

    uint16_t msgLen = (buf[2] << 8) | buf[3];
    int pos = 20;
    while (pos < 20 + msgLen && pos + 4 <= n) {
        uint16_t attrType = (buf[pos] << 8) | buf[pos + 1];
        uint16_t attrLen  = (buf[pos + 2] << 8) | buf[pos + 3];
        pos += 4;
        if (pos + attrLen > n) break;

        if (attrType == 0x0020 && attrLen >= 8) { // XOR-MAPPED-ADDRESS
            uint16_t family = buf[pos + 1];
            if (family == 0x01) {
                uint16_t xport = ((buf[pos + 2] << 8) | buf[pos + 3]);
                uint32_t xaddr = ((uint32_t)buf[pos + 4] << 24) |
                                 ((uint32_t)buf[pos + 5] << 16) |
                                 ((uint32_t)buf[pos + 6] << 8)  |
                                  (uint32_t)buf[pos + 7];
                mappedAddr.sin_family      = AF_INET;
                mappedAddr.sin_port        = (uint16_t)(xport ^ (STUN_MAGIC >> 16));
                uint32_t realAddr          = xaddr ^ STUN_MAGIC;
                mappedAddr.sin_addr.s_addr = htonl(realAddr);
                return true;
            }
        }
        if (attrType == 0x0001 && attrLen >= 8) { // MAPPED-ADDRESS fallback
            uint16_t family = buf[pos + 1];
            if (family == 0x01) {
                mappedAddr.sin_family = AF_INET;
                memcpy(&mappedAddr.sin_port, buf + pos + 2, 2);
                memcpy(&mappedAddr.sin_addr, buf + pos + 4, 4);
                return true;
            }
        }
        pos += (attrLen + 3) & ~3;
    }
    return false;
}

bool GetMappedAddress(SOCKET sock, sockaddr_in& mappedAddr) {
    return GetMappedAddress(sock, "stun.l.google.com", 19302, mappedAddr);
}

} // namespace network::stun
