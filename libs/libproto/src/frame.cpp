#include "libproto.h"
#include <cstring>

namespace proto {

std::vector<uint8_t> Encode(FrameType type, uint64_t seq,
                             const void* payload, uint16_t payloadLen) {
    std::vector<uint8_t> buf(HEADER_SIZE + payloadLen);
    auto* hdr = reinterpret_cast<FrameHeader*>(buf.data());
    hdr->magic    = MAGIC;
    hdr->type     = type;
    hdr->length   = payloadLen;
    hdr->sequence = seq;
    if (payloadLen > 0 && payload) {
        memcpy(buf.data() + HEADER_SIZE, payload, payloadLen);
    }
    return buf;
}

bool Decode(const uint8_t* data, size_t len,
            FrameHeader& outHeader, const uint8_t*& outPayload) {
    if (len < HEADER_SIZE) return false;
    const auto* hdr = reinterpret_cast<const FrameHeader*>(data);
    if (hdr->magic != MAGIC) return false;
    if (len < HEADER_SIZE + hdr->length) return false;

    outHeader = *hdr;
    outPayload = data + HEADER_SIZE;
    return true;
}

} // namespace proto
