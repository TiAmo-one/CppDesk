#include "libnetwork/relay_channel.h"
#include "libnetwork/signal_client.h"
#include <cstring>
#include <chrono>
#include <iostream>

namespace network {

// Simple Base64 decode
static bool Base64Decode(const std::string& input, std::vector<uint8_t>& output) {
    static const unsigned char decodeTable[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,62, 255,255,255,63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255,255,255,0,  255,255,
        255,0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255,255,255,255,255,
        255,26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255,255,255,255,255,
    };
    output.clear();
    if (input.size() % 4 != 0) return false;
    size_t outLen = (input.size() / 4) * 3;
    if (input.size() >= 2 && input[input.size() - 1] == '=') outLen--;
    if (input.size() >= 3 && input[input.size() - 2] == '=') outLen--;
    output.resize(outLen);

    size_t outIdx = 0;
    uint32_t accum = 0;
    int bits = 0;
    for (size_t i = 0; i < input.size(); i++) {
        unsigned char val = decodeTable[(unsigned char)input[i]];
        if (val == 255) continue;
        accum = (accum << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (outIdx < outLen)
                output[outIdx++] = (uint8_t)(accum >> bits);
        }
    }
    return true;
}

// Simple Base64 encode
static std::string Base64Encode(const uint8_t* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out += tbl[(n >> 18) & 0x3F];
        out += tbl[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? tbl[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? tbl[n & 0x3F] : '=';
    }
    return out;
}

bool RelayChannel::Init(SignalClient* signal) {
    signal_ = signal;
    if (!signal_) return false;

    // Register callback for relay_data messages
    signal_->SetCallbacks({
        nullptr, nullptr, nullptr,
        [this](const std::string& data) { OnRelayData(data); }
    });
    return true;
}

void RelayChannel::SetKey(const uint8_t key[16]) {
    memcpy(aesKey_, key, 16);
    hasKey_ = true;
}

void RelayChannel::OnRelayData(const std::string& base64Data) {
    std::vector<uint8_t> decoded;
    if (!Base64Decode(base64Data, decoded) || decoded.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);
    recvQueue_.push_back(std::move(decoded));
}

bool RelayChannel::SendFrame(proto::FrameType type, const void* payload, uint16_t len) {
    if (!signal_ || !hasKey_) return false;

    auto wire = proto::Encode(type, seq_++, payload, len);
    std::string encoded = Base64Encode(wire.data(), wire.size());
    return signal_->SendRelayData(encoded);
}

int RelayChannel::Poll(int timeoutMs, FrameCallback cb) {
    int count = 0;
    auto start = std::chrono::steady_clock::now();

    while (true) {
        std::vector<uint8_t> data;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!recvQueue_.empty()) {
                data = std::move(recvQueue_.front());
                recvQueue_.erase(recvQueue_.begin());
            }
        }

        if (!data.empty()) {
            proto::FrameHeader hdr;
            const uint8_t* pPayload = nullptr;
            if (!proto::Decode(data.data(), data.size(), hdr, pPayload)) continue;

            // Replay check
            if (hasRemoteSeq_ && hdr.sequence <= remoteSeq_) continue;
            remoteSeq_ = hdr.sequence;
            hasRemoteSeq_ = true;

            count++;
            if (cb) cb(hdr.type, hdr.sequence, pPayload, hdr.length);
            start = std::chrono::steady_clock::now();
        } else {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) break;
            // No sleep needed ? caller polls signal_ separately
            break;
        }
    }
    return count;
}

} // namespace network
