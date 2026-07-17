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
        other.frame_.data = nullptr;
        other.dup_ = nullptr;
    }
    return *this;
}

// ---- Capture ----

Capture::Capture() = default;

Capture::~Capture() {
    if (dup_)    dup_->Release();
    if (ctx_)    ctx_->Release();
    if (device_) device_->Release();
}

bool Capture::Init(int monitorIndex) {
    if (device_) return false; // already initialized

    // Create D3D11 device
    D3D_FEATURE_LEVEL feats[] = { D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        feats, ARRAYSIZE(feats), D3D11_SDK_VERSION,
        &device_, nullptr, &ctx_);
    if (FAILED(hr)) return false;

    // Get DXGI adapter output
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

    // Get initial resolution
    DXGI_OUTDUPL_DESC desc;
    dup_->GetDesc(&desc);
    width_  = desc.ModeDesc.Width;
    height_ = desc.ModeDesc.Height;
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

    ID3D11Texture2D* tex = nullptr;
    res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    res->Release();

    D3D11_TEXTURE2D_DESC texDesc;
    tex->GetDesc(&texDesc);

    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags      = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags      = 0;

    ID3D11Texture2D* staging = nullptr;
    hr = device_->CreateTexture2D(&stagingDesc, nullptr, &staging);
    if (FAILED(hr)) {
        tex->Release();
        dup_->ReleaseFrame();
        return guard;
    }

    ctx_->CopyResource(staging, tex);
    tex->Release();

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ctx_->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        staging->Release();
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

    // staging texture is leaked intentionally (unmapped memory)
    // FrameGuard dtor calls dup_->ReleaseFrame() which releases the staging
    staging->Release();
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
