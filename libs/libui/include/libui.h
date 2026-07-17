#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cstdint>
#include <functional>

namespace ui {

struct UiCallbacks {
    std::function<void(float, float)> onMouseMove;
    std::function<void(int, bool)>    onMouseButton;
    std::function<void(int)>          onMouseWheel;
    std::function<void(uint16_t, bool)> onKey;
    std::function<void(const wchar_t*)> onFileDrop;
    std::function<void(int, int)>     onResize;
    std::function<void()>             onClose;
};

class RenderWindow {
public:
    RenderWindow();
    ~RenderWindow();

    bool Create(HINSTANCE hInst, const wchar_t* title, int width, int height);
    void Show(int nCmdShow = SW_SHOW);
    HWND GetHwnd() const { return hwnd_; }

    void UpdateFrame(const uint8_t* bgra, int w, int h, int stride);
    int  Run();
    void SetCallbacks(UiCallbacks cb) { cb_ = cb; }
    void SetTitle(const wchar_t* title);

private:
    HWND hwnd_ = nullptr;
    ID2D1Factory*          d2dFactory_ = nullptr;
    ID2D1HwndRenderTarget* rt_ = nullptr;
    ID2D1Bitmap*           bitmap_ = nullptr;
    UiCallbacks            cb_;
    int frameW_ = 0, frameH_ = 0;
    CRITICAL_SECTION frameLock_;

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void OnPaint();
    bool InitD2D();
};

} // namespace ui
