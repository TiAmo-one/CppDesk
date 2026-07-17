#include "libnetwork/hole_punch.h"
#include <ws2tcpip.h>
#include <sodium.h>
#include <chrono>
#include <thread>

namespace network {

SdpInfo BuildSdp(SOCKET sock, const sockaddr_in& stunAddr) {
    SdpInfo sdp;
    char ip[64];
    inet_ntop(AF_INET, &stunAddr.sin_addr, ip, sizeof(ip));
    sdp.candidates.push_back(
        std::string(ip) + ":" + std::to_string(ntohs(stunAddr.sin_port)));

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostent* he = gethostbyname(hostname);
        if (he) {
            for (int i = 0; he->h_addr_list[i]; i++) {
                in_addr addr;
                memcpy(&addr, he->h_addr_list[i], sizeof(addr));
                inet_ntop(AF_INET, &addr, ip, sizeof(ip));
                sdp.candidates.push_back(std::string(ip) + ":" +
                    std::to_string(UdpSocket{}.GetPort()));
            }
        }
    }
    return sdp;
}

bool HolePunch(UdpSocket& sock, const SdpInfo& remoteSdp,
               sockaddr_in& outPeerAddr) {
    std::vector<sockaddr_in> candidates;
    for (auto& c : remoteSdp.candidates) {
        auto colon = c.find(':');
        if (colon == std::string::npos) continue;
        std::string ip = c.substr(0, colon);
        uint16_t port = (uint16_t)std::stoi(c.substr(colon + 1));

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
        candidates.push_back(addr);
    }
    if (candidates.empty()) return false;

    uint8_t punchMsg[] = {0x50, 0x55, 0x4E, 0x43, 0x48}; // "PUNCH"
    for (int burst = 0; burst < 10; burst++) {
        for (auto& addr : candidates) {
            sock.SendTo(addr, punchMsg, sizeof(punchMsg));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        sockaddr_in from = {};
        uint8_t buf[256];
        int n = sock.RecvFrom(from, buf, sizeof(buf), 10);
        if (n > 0 && memcmp(buf, punchMsg, sizeof(punchMsg)) == 0) {
            outPeerAddr = from;
            return true;
        }
    }
    return false;
}

bool KeyExchange(UdpSocket& sock, const sockaddr_in& peer,
                  uint8_t outSharedKey[16]) {
    if (sodium_init() < 0) return false;

    uint8_t pk[crypto_box_PUBLICKEYBYTES];
    uint8_t sk[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(pk, sk);

    if (!sock.SendTo(peer, pk, sizeof(pk))) return false;

    uint8_t peerPk[crypto_box_PUBLICKEYBYTES];
    sockaddr_in from = {};
    int n = sock.RecvFrom(from, peerPk, sizeof(peerPk), 5000);
    if (n != sizeof(peerPk)) return false;

    uint8_t shared[crypto_box_BEFORENMBYTES];
    if (crypto_box_beforenm(shared, peerPk, sk) != 0) return false;

    memcpy(outSharedKey, shared, 16);
    return true;
}

} // namespace network
