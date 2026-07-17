#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

namespace proto {

constexpr uint32_t MAGIC = 0x524F4B52; // "ROKR"

enum class FrameType : uint8_t {
    Video          = 0x01,
    MouseMove      = 0x02,
    MouseBtn       = 0x03,
    KeyEvent       = 0x04,
    ClipboardText  = 0x05,
    ClipboardFile  = 0x06,
    FileBlock      = 0x07,
    Heartbeat      = 0x08,
    Resolution     = 0x09,
};

#pragma pack(push, 1)
struct FrameHeader {
    uint32_t magic;
    FrameType type;
    uint16_t length;   // payload length, max 65535
    uint64_t sequence;  // monotonic, for replay protection
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(FrameHeader); // 15 bytes

// Video payload: first 4 bytes = fragment index (for large frames)
#pragma pack(push, 1)
struct VideoPayload {
    uint32_t fragmentIndex;
    // followed by H.264 NAL data
};
#pragma pack(pop)

// Mouse move: normalized coordinates [0.0, 1.0]
#pragma pack(push, 1)
struct MouseMovePayload {
    float x;
    float y;
};
#pragma pack(pop)

// Mouse button
#pragma pack(push, 1)
struct MouseBtnPayload {
    uint8_t button;   // 0=left, 1=right, 2=middle
    uint8_t down;     // 0=up, 1=down
};
#pragma pack(pop)

// Key event
#pragma pack(push, 1)
struct KeyEventPayload {
    uint16_t vkCode;
    uint8_t  down;
};
#pragma pack(pop)

// Resolution change
#pragma pack(push, 1)
struct ResolutionPayload {
    uint32_t width;
    uint32_t height;
};
#pragma pack(pop)

// Encode a frame into wire format. Returns binary buffer.
std::vector<uint8_t> Encode(FrameType type, uint64_t seq,
                             const void* payload, uint16_t payloadLen);

// Decode a frame from wire format. Returns false if magic mismatch or truncated.
// outPayload points into the input buffer (no copy).
bool Decode(const uint8_t* data, size_t len,
            FrameHeader& outHeader, const uint8_t*& outPayload);

} // namespace proto
