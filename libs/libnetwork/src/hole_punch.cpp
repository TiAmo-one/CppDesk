#include "libnetwork/hole_punch.h"
#include <ws2tcpip.h>
#include <sodium.h>
#include <chrono>
#include <thread>
#include <iostream>

namespace network {

SdpInfo BuildSdp(SOCKET sock, const sockaddr_in& stunAddr, uint16_t localPort) {
    SdpInfo sdp;
    sdp.candidates.push_back("127.0.0.1:" + std::to_string(localPort));
    if (stunAddr.sin_addr.s_addr != 0) {
        char ip[64];
        inet_ntop(AF_INET, &stunAddr.sin_addr, ip, sizeof(ip));
        sdp.candidates.push_back(
            std::string(ip) + ":" + std::to_string(ntohs(stunAddr.sin_port)));
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

    const uint8_t punchMsg[] = {0x50, 0x55, 0x4E, 0x43, 0x48};
    std::cout << "Hole punching to " << candidates.size() << " candidate(s)..." << std::endl;
    for (int burst = 0; burst < 60; burst++) {
        for (auto& addr : candidates)
            sock.SendTo(addr, punchMsg, sizeof(punchMsg));
        for (int retry = 0; retry < 3; retry++) {
            sockaddr_in from = {};
            uint8_t buf[256];
            int n = sock.RecvFrom(from, buf, sizeof(buf), 5);
            if (n == sizeof(punchMsg) && memcmp(buf, punchMsg, sizeof(punchMsg)) == 0) {
                outPeerAddr = from;
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}

void DrainSocket(UdpSocket& sock) {
    sockaddr_in dummy = {};
    uint8_t buf[2048];
    int drained = 0;
    while (true) {
        int n = sock.RecvFrom(dummy, buf, sizeof(buf), 0);
        if (n <= 0) break;
        drained++;
    }
    if (drained > 0)
        std::cerr << "[DRAIN] Cleared " << drained << " stale UDP packets" << std::endl;
}

bool KeyExchange(UdpSocket& sock, const sockaddr_in& peer,
                  uint8_t outSharedKey[16]) {
    if (sodium_init() < 0) return false;

    // Drain stale packets from previous session
    DrainSocket(sock);

    uint8_t pk[crypto_box_PUBLICKEYBYTES];
    uint8_t sk[crypto_box_SECRETKEYBYTES];
    crypto_box_keypair(pk, sk);

    if (!sock.SendTo(peer, pk, sizeof(pk))) {
        std::cerr << "[KEYEX] Failed to send public key" << std::endl;
        return false;
    }

    // Receive peer's public key — skip any non-key packets (stale input events etc.)
    uint8_t peerPk[crypto_box_PUBLICKEYBYTES];
    sockaddr_in from = {};
    int attempts = 0;
    while (attempts < 100) {
        int n = sock.RecvFrom(from, peerPk, sizeof(peerPk), 100);
        if (n == sizeof(peerPk)) break; // got the key
        if (n > 0) {
            std::cerr << "[KEYEX] Skipped stale packet size=" << n << " attempt=" << attempts << std::endl;
        }
        attempts++;
        // Re-send our key in case it was lost
        if (attempts % 5 == 0) {
            sock.SendTo(peer, pk, sizeof(pk));
        }
    }

    if (attempts >= 100) {
        std::cerr << "[KEYEX] Timed out waiting for peer key" << std::endl;
        return false;
    }

    uint8_t shared[crypto_box_BEFORENMBYTES];
    if (crypto_box_beforenm(shared, peerPk, sk) != 0) return false;

    memcpy(outSharedKey, shared, 16);
    return true;
}

} // namespace network

