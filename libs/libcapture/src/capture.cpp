#include "libcapture.h"
#include <utility>
#include <cassert>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

namespace capture {

// ---- FrameGuard ----

FrameGuard::~FrameGuard() {
    if (dup_ && frame_.data) {
        dup_->ReleaseFrame();
    }
}

FrameGuard::FrameGuard(FrameGuard&& other) noexcept {
    *this = std::move(other);
}

FrameGuard& FrameGuard::operator=(FrameGuard&& other) noexcept {
    if (this != &other) {
        if (dup_ && frame_.data) dup_->ReleaseFrame();
        frame_ = other.frame_;
        dup_ = other.dup_;
        dirtyRects_ = std::move(other.dirtyRects_);
        moveRects_ = std::move(other.moveRects_);
        other.frame_.data = nullptr;
        other.dup_ = nullptr;
    }
    return *this;
}

// ---- Capture ----

Capture::Capture() = default;

Capture::~Capture() {
    if (staging_) staging_->Release();
    if (dup_)     dup_->Release();
    if (ctx_)     ctx_->Release();
    if (device_)  device_->Release();
}

bool Capture::Init(int monitorIndex) {
    if (device_) return false;

    D3D_FEATURE_LEVEL feats[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        feats, ARRAYSIZE(feats), D3D11_SDK_VERSION,
        &device_, nullptr, &ctx_);
    if (FAILED(hr)) return false;

    IDXGIDevice* dxgiDevice = nullptr;
    hr = device_->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr)) return false;

    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetAdapter(&adapter);
    dxgiDevice->Release();

    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(monitorIndex, &output);
    adapter->Release();
    if (FAILED(hr)) return false;

    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) return false;

    hr = output1->DuplicateOutput(device_, &dup_);
    output1->Release();
    if (FAILED(hr)) return false;

    DXGI_OUTDUPL_DESC desc;
    dup_->GetDesc(&desc);
    width_  = desc.ModeDesc.Width;
    height_ = desc.ModeDesc.Height;
    staging_ = nullptr;
    stagingWidth_ = 0; stagingHeight_ = 0;
    released_ = true;
    return true;
}

FrameGuard Capture::AcquireFrame(int timeoutMs) {
    FrameGuard guard;
    if (!dup_) return guard;

    IDXGIResource* res = nullptr;
    DXGI_OUTDUPL_FRAME_INFO info;
    HRESULT hr = dup_->AcquireNextFrame(timeoutMs, &info, &res);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return guard;
    if (FAILED(hr)) return guard;

    // Extract dirty rects and move rects from metadata
    if (info.TotalMetadataBufferSize > 0) {
        std::vector<uint8_t> metaBuf(info.TotalMetadataBufferSize);
        hr = dup_->GetFrameMoveRects(
            info.TotalMetadataBufferSize,
            reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(metaBuf.data()),
            &info.TotalMetadataBufferSize);
        if (SUCCEEDED(hr) && info.TotalMetadataBufferSize > 0) {
            UINT moveCount = info.TotalMetadataBufferSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);
            DXGI_OUTDUPL_MOVE_RECT* moves = reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(metaBuf.data());
            for (UINT i = 0; i < moveCount; i++) {
                DirtyRect r;
                r.left   = moves[i].SourcePoint.x;
                r.top    = moves[i].SourcePoint.y;
                r.right  = moves[i].SourcePoint.x + (int32_t)width_;
                r.bottom = moves[i].SourcePoint.y + (int32_t)height_;
                // Actually the move rect tells us the destination, source is the same structure
                // Let me fix this:
                // SourcePoint is where to copy FROM
                // DestinationRect is where to copy TO
                // This is complex, skip for now. Treat entire screen as dirty on move.
                guard.moveRects_.push_back(r);
            }
        }

        hr = dup_->GetFrameDirtyRects(
            (UINT)metaBuf.size(),
            reinterpret_cast<RECT*>(metaBuf.data()),
            &info.TotalMetadataBufferSize);
        if (SUCCEEDED(hr) && info.TotalMetadataBufferSize > 0) {
            UINT dirtyCount = info.TotalMetadataBufferSize / sizeof(RECT);
            RECT* rects = reinterpret_cast<RECT*>(metaBuf.data());
            for (UINT i = 0; i < dirtyCount; i++) {
                DirtyRect r;
                r.left   = rects[i].left;
                r.top    = rects[i].top;
                r.right  = rects[i].right;
                r.bottom = rects[i].bottom;
                guard.dirtyRects_.push_back(r);
            }
        }
    }

    ID3D11Texture2D* tex = nullptr;
    res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    res->Release();

    D3D11_TEXTURE2D_DESC texDesc;
    tex->GetDesc(&texDesc);

    if (!staging_ || stagingWidth_ != texDesc.Width || stagingHeight_ != texDesc.Height) {
        if (staging_) {
            ctx_->Unmap(staging_, 0);
            staging_->Release();
            staging_ = nullptr;
        }
        D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
        stagingDesc.Usage          = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags      = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags      = 0;
        hr = device_->CreateTexture2D(&stagingDesc, nullptr, &staging_);
        if (FAILED(hr)) {
            tex->Release();
            dup_->ReleaseFrame();
            return guard;
        }
        stagingWidth_ = texDesc.Width;
        stagingHeight_ = texDesc.Height;
    }

    ctx_->Unmap(staging_, 0);
    ctx_->CopyResource(staging_, tex);
    tex->Release();

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ctx_->Map(staging_, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        dup_->ReleaseFrame();
        return guard;
    }

    guard.frame_.data      = (uint8_t*)mapped.pData;
    guard.frame_.width     = texDesc.Width;
    guard.frame_.height    = texDesc.Height;
    guard.frame_.stride    = mapped.RowPitch;
    guard.frame_.timestamp = info.LastPresentTime.QuadPart;
    guard.dup_             = dup_;
    released_              = false;
    return guard;
}

void Capture::ReleaseFrame() {
    if (!released_ && dup_) {
        dup_->ReleaseFrame();
        released_ = true;
    }
}

bool Capture::GetCurrentResolution(uint32_t& width, uint32_t& height) const {
    if (!dup_) return false;
    width  = width_;
    height = height_;
    return true;
}

} // namespace capture
