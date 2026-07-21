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

// Generate ECDH keypair (sodium). Returns true on success.
// publicKey: crypto_box_PUBLICKEYBYTES (32) bytes
// secretKey: crypto_box_SECRETKEYBYTES (32) bytes
bool GenerateKeyPair(uint8_t publicKey[32], uint8_t secretKey[32]);

// Derive shared AES-128 key from peer's public key and our secret key.
// peerPublicKey: 32 bytes from peer
// ourSecretKey: 32 bytes our secret
// outSharedKey: 16 bytes output
bool DeriveSharedKey(const uint8_t peerPublicKey[32],
                     const uint8_t ourSecretKey[32],
                     uint8_t outSharedKey[16]);

} // namespace network
