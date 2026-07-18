#include "libnetwork/peer_channel.h"
#include <cstring>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>
#include <iostream>

namespace network {

bool PeerChannel::Init(SOCKET sock, const sockaddr_in& peer) {
    sock_ = sock;
    peer_ = peer;
    int bufSize = 16 * 1024 * 1024;
    int ret = setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, (const char*)&bufSize, sizeof(bufSize));
    int actual = 0; int sz = sizeof(actual);
    getsockopt(sock_, SOL_SOCKET, SO_RCVBUF, (char*)&actual, &sz);
    char dbg[128];
    snprintf(dbg, sizeof(dbg), "[CHAN] SO_RCVBUF requested=%d set_ok=%d actual=%d\n", bufSize, ret == 0, actual);
    OutputDebugStringA(dbg);
    setsockopt(sock_, SOL_SOCKET, SO_SNDBUF, (const char*)&bufSize, sizeof(bufSize));
    u_long mode = 1;
    ioctlsocket(sock_, FIONBIO, &mode);
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
    if (sent != (int)wire.size()) {
        static int failCount = 0;
        if (++failCount <= 3) {
            int err = WSAGetLastError();
            std::cerr << "[CHAN] sendto FAIL: sent=" << sent << " expected=" << wire.size() << " err=" << err << std::endl;
        }
    }
    return sent == (int)wire.size();
}

int PeerChannel::Poll(int timeoutMs, FrameCallback cb) {
    int count = 0;
    uint8_t buf[65535];
    auto start = std::chrono::steady_clock::now();


    while (true) {
        sockaddr_in from = {};
        int fromLen = sizeof(from);
        int n = recvfrom(sock_, (char*)buf, sizeof(buf), 0,
                         (sockaddr*)&from, &fromLen);
        if (n <= 0) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) break;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) break;
            std::this_thread::yield();
            continue;
        }

        proto::FrameHeader hdr;
        const uint8_t* pPayload = nullptr;
        if (!proto::Decode(buf, n, hdr, pPayload)) continue;

        // Replay check: only reject if we have a valid previous seq
        if (hasRemoteSeq_ && hdr.sequence <= remoteSeq_) continue;
        remoteSeq_ = hdr.sequence;
        hasRemoteSeq_ = true;

        count++;
        if (cb) cb(hdr.type, hdr.sequence, pPayload, hdr.length);
        start = std::chrono::steady_clock::now();
    }
    return count;
}


} // namespace network
