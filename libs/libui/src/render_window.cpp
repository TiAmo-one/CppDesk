#include "libui.h"
#include <shellapi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace ui {

RenderWindow::RenderWindow()  { InitializeCriticalSection(&frameLock_); }
RenderWindow::~RenderWindow() {
    if (bitmap_)     bitmap_->Release();
    if (rt_)         rt_->Release();
    if (d2dFactory_) d2dFactory_->Release();
    DeleteCriticalSection(&frameLock_);
}

bool RenderWindow::InitD2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
    if (FAILED(hr)) return false;

    RECT rc; GetClientRect(hwnd_, &rc);
    hr = d2dFactory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_,
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)), &rt_);
    return SUCCEEDED(hr);
}

LRESULT CALLBACK RenderWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = (RenderWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        auto* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_PAINT:
        if (self) self->OnPaint();
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_SIZE:
        if (self && self->rt_) {
            RECT rc; GetClientRect(hwnd, &rc);
            self->rt_->Resize(D2D1::SizeU(rc.right, rc.bottom));
            if (self->cb_.onResize) self->cb_.onResize(rc.right, rc.bottom);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (self && self->cb_.onMouseMove && self->frameW_ > 0 && self->frameH_ > 0)
            self->cb_.onMouseMove((float)LOWORD(lParam) / self->frameW_,
                                   (float)HIWORD(lParam) / self->frameH_);
        return 0;
    case WM_LBUTTONDOWN: if (self && self->cb_.onMouseButton) self->cb_.onMouseButton(0, true); return 0;
    case WM_LBUTTONUP:   if (self && self->cb_.onMouseButton) self->cb_.onMouseButton(0, false); return 0;
    case WM_RBUTTONDOWN: if (self && self->cb_.onMouseButton) self->cb_.onMouseButton(1, true); return 0;
    case WM_RBUTTONUP:   if (self && self->cb_.onMouseButton) self->cb_.onMouseButton(1, false); return 0;
    case WM_MOUSEWHEEL:
        if (self && self->cb_.onMouseWheel) self->cb_.onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_KEYDOWN:
        if (self && self->cb_.onKey) self->cb_.onKey((uint16_t)wParam, true);
        return 0;
    case WM_KEYUP:
        if (self && self->cb_.onKey) self->cb_.onKey((uint16_t)wParam, false);
        return 0;
    case WM_DROPFILES:
        if (self && self->cb_.onFileDrop) {
            HDROP drop = (HDROP)wParam;
            wchar_t path[MAX_PATH];
            if (DragQueryFile(drop, 0, path, MAX_PATH) > 0)
                self->cb_.onFileDrop(path);
            DragFinish(drop);
        }
        return 0;
    case WM_DESTROY:
        if (self && self->cb_.onClose) self->cb_.onClose();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool RenderWindow::Create(HINSTANCE hInst, const wchar_t* title, int width, int height) {
    WNDCLASS wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"RemoteDesktopWindow";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    RECT wr = {0, 0, width, height};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindow(L"RemoteDesktopWindow", title, WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          wr.right - wr.left, wr.bottom - wr.top,
                          nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    DragAcceptFiles(hwnd_, TRUE);
    return InitD2D();
}

void RenderWindow::Show(int nCmdShow) { ShowWindow(hwnd_, nCmdShow); }

void RenderWindow::UpdateFrame(const uint8_t* bgra, int w, int h, int stride) {
    EnterCriticalSection(&frameLock_);

    if (w != frameW_ || h != frameH_ || !bitmap_) {
        if (bitmap_) bitmap_->Release();
        bitmap_ = nullptr;
        frameW_ = w; frameH_ = h;
        if (w > 0 && h > 0 && rt_) {
            rt_->CreateBitmap(D2D1::SizeU(w, h),
                D2D1::BitmapProperties(
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
                &bitmap_);
        }
    }

    if (bitmap_) bitmap_->CopyFromMemory(nullptr, bgra, stride);
    LeaveCriticalSection(&frameLock_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void RenderWindow::OnPaint() {
    EnterCriticalSection(&frameLock_);
    if (rt_ && bitmap_) {
        rt_->BeginDraw();
        rt_->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        D2D1_SIZE_F size   = bitmap_->GetSize();
        D2D1_SIZE_F rtSize = rt_->GetSize();

        float scale = min(rtSize.width / size.width, rtSize.height / size.height);
        float x = (rtSize.width  - size.width  * scale) / 2;
        float y = (rtSize.height - size.height * scale) / 2;

        rt_->DrawBitmap(bitmap_,
            D2D1::RectF(x, y, x + size.width * scale, y + size.height * scale));
        rt_->EndDraw();
    }
    LeaveCriticalSection(&frameLock_);
}

int RenderWindow::Run() {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void RenderWindow::SetTitle(const wchar_t* title) { SetWindowText(hwnd_, title); }

} // namespace ui
