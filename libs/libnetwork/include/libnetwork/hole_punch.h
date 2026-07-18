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

SdpInfo BuildSdp(SOCKET sock, const sockaddr_in& stunAddr, uint16_t localPort);
bool HolePunch(UdpSocket& sock, const SdpInfo& remoteSdp, sockaddr_in& outPeerAddr);
bool KeyExchange(UdpSocket& sock, const sockaddr_in& peer, uint8_t outSharedKey[16]);
void DrainSocket(UdpSocket& sock);

} // namespace network
