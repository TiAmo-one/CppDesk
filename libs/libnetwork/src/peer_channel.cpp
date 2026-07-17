#include "libnetwork/peer_channel.h"
#include <cstring>

namespace network {

bool PeerChannel::Init(SOCKET sock, const sockaddr_in& peer) {
    sock_ = sock;
    peer_ = peer;
    return true;
}

void PeerChannel::SetKey(const uint8_t key[16]) {
    memcpy(aesKey_, key, 16);
    hasKey_ = true;
}

bool PeerChannel::SendFrame(proto::FrameType type, const void* payload, uint16_t len) {
    auto wire = proto::Encode(type, seq_++, payload, len);
    // AES-GCM encrypt would go here if hasKey_
    int sent = sendto(sock_, (const char*)wire.data(), (int)wire.size(), 0,
                      (const sockaddr*)&peer_, sizeof(peer_));
    return sent == (int)wire.size();
}

int PeerChannel::Poll(int timeoutMs, FrameCallback cb) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock_, &fds);
    timeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int sel = select(0, &fds, nullptr, nullptr, &tv);
    if (sel <= 0) return 0;

    uint8_t buf[65535];
    sockaddr_in from = {};
    int fromLen = sizeof(from);
    int n = recvfrom(sock_, (char*)buf, sizeof(buf), 0,
                     (sockaddr*)&from, &fromLen);
    if (n <= 0) return 0;

    proto::FrameHeader hdr;
    const uint8_t* pPayload = nullptr;
    if (!proto::Decode(buf, n, hdr, pPayload)) return 0;

    // Replay check
    if (hdr.sequence <= remoteSeq_) return 0;
    remoteSeq_ = hdr.sequence;

    if (cb) cb(hdr.type, hdr.sequence, pPayload, hdr.length);
    return 1;
}

} // namespace network
