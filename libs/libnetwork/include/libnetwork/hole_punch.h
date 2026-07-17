#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <winsock2.h>
#include "libnetwork/udp_socket.h"

namespace network {

struct SdpInfo {
    std::vector<std::string> candidates; // "ip:port"
};

// Build SDP from local STUN result + local addresses
SdpInfo BuildSdp(SOCKET sock, const sockaddr_in& stunAddr);

// Attempt UDP hole punch to remote peer
bool HolePunch(UdpSocket& sock, const SdpInfo& remoteSdp,
               sockaddr_in& outPeerAddr);

// ECDH key exchange over UDP (Curve25519 via libsodium)
bool KeyExchange(UdpSocket& sock, const sockaddr_in& peer,
                  uint8_t outSharedKey[16]);

} // namespace network
