#pragma once
#include <cstdint>
#include <windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>

namespace capture {

struct Frame {
    uint8_t* data;      // BGRA pixels (mapped pointer, do not free)
    uint32_t width;
    uint32_t height;
    uint32_t stride;    // bytes per row
    uint64_t timestamp;  // QueryPerformanceCounter value
};

// RAII wrapper around a captured frame
class FrameGuard {
public:
    FrameGuard() = default;
    ~FrameGuard();
    FrameGuard(FrameGuard&& other) noexcept;
    FrameGuard& operator=(FrameGuard&& other) noexcept;
    FrameGuard(const FrameGuard&) = delete;
    FrameGuard& operator=(const FrameGuard&) = delete;

    bool valid() const { return frame_.data != nullptr; }
    const Frame& frame() const { return frame_; }
    Frame& frame() { return frame_; }

private:
    friend class Capture;
    Frame frame_{};
    IDXGIOutputDuplication* dup_ = nullptr;
};

class Capture {
public:
    Capture();
    ~Capture();

    bool Init(int monitorIndex = 0);
    FrameGuard AcquireFrame(int timeoutMs = 100);
    void ReleaseFrame();
    bool GetCurrentResolution(uint32_t& width, uint32_t& height) const;

private:
    ID3D11Device*           device_  = nullptr;
    ID3D11DeviceContext*    ctx_     = nullptr;
    IDXGIOutputDuplication* dup_     = nullptr;
    ID3D11Texture2D*        staging_ = nullptr;
    uint32_t stagingWidth_ = 0, stagingHeight_ = 0;
    uint32_t width_  = 0;
    uint32_t height_ = 0;
    bool released_ = true;
};

} // namespace capture
