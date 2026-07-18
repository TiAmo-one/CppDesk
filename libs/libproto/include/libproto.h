#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

namespace proto {

constexpr uint32_t MAGIC = 0x524F4B52;

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
    DirtyRect      = 0x0D,
};

#pragma pack(push, 1)
struct FrameHeader {
    uint32_t magic;
    FrameType type;
    uint16_t length;
    uint64_t sequence;
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(FrameHeader);

#pragma pack(push, 1)
struct VideoPayload {
    uint32_t fragmentIndex;
    uint16_t width;
    uint16_t height;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct DirtyRectPayload {
    uint32_t fragmentIndex;
    uint32_t totalFragments;
    uint16_t frameWidth;
    uint16_t frameHeight;
    int16_t  left;
    int16_t  top;
    int16_t  right;
    int16_t  bottom;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MouseMovePayload {
    float x;
    float y;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MouseBtnPayload {
    uint8_t button;
    uint8_t down;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct KeyEventPayload {
    uint16_t vkCode;
    uint8_t  down;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct ResolutionPayload {
    uint32_t width;
    uint32_t height;
};
#pragma pack(pop)

std::vector<uint8_t> Encode(FrameType type, uint64_t seq,
                             const void* payload, uint16_t payloadLen);

bool Decode(const uint8_t* data, size_t len,
            FrameHeader& outHeader, const uint8_t*& outPayload);

} // namespace proto
