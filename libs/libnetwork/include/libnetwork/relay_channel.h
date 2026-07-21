#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <mutex>
#include "libproto.h"

namespace network {

class SignalClient;

// Encrypted relay channel over WebSocket signal connection.
// Used as fallback when UDP hole-punching fails (symmetric NAT etc.).
class RelayChannel {
public:
    using FrameCallback = std::function<void(proto::FrameType type, uint64_t seq,
                                              const uint8_t* payload, uint16_t len)>;

    RelayChannel() = default;
    ~RelayChannel() = default;

    bool Init(SignalClient* signal);
    void SetKey(const uint8_t key[16]);
    bool SendFrame(proto::FrameType type, const void* payload, uint16_t len);
    int  Poll(int timeoutMs, FrameCallback cb);
    uint64_t NextSeq() { return seq_++; }
    bool IsActive() const { return signal_ != nullptr; }

private:
    SignalClient* signal_ = nullptr;
    uint64_t    seq_     = 0;
    uint64_t    remoteSeq_ = 0;
    bool        hasRemoteSeq_ = false;
    uint8_t     aesKey_[16] = {};
    bool        hasKey_  = false;

    mutable std::mutex mutex_;
    std::vector<std::vector<uint8_t>> recvQueue_;

    void OnRelayData(const std::string& base64Data);
};

} // namespace network
