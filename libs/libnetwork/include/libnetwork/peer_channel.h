#pragma once
#include <cstdint>
#include <functional>
#include <winsock2.h>
#include "libproto.h"

namespace network {

// Encrypted P2P channel. After hole punch + ECDH, this wraps frame send/recv.
class PeerChannel {
public:
    using FrameCallback = std::function<void(proto::FrameType type, uint64_t seq,
                                              const uint8_t* payload, uint16_t len)>;

    PeerChannel() = default;
    ~PeerChannel() = default;

    bool Init(SOCKET sock, const sockaddr_in& peer);

    void SetKey(const uint8_t key[16]);
    bool SendFrame(proto::FrameType type, const void* payload, uint16_t len);
    int  Poll(int timeoutMs, FrameCallback cb);
    uint64_t NextSeq() { return seq_++; }

    SOCKET GetSocket() const { return sock_; }
    const sockaddr_in& GetPeerAddr() const { return peer_; }

private:
    SOCKET      sock_    = INVALID_SOCKET;
    sockaddr_in peer_    = {};
    uint64_t    seq_     = 0;
    uint64_t    remoteSeq_ = 0;
    uint8_t     aesKey_[16] = {};
    bool        hasKey_  = false;
};

} // namespace network
