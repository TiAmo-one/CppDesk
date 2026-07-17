#include "libinput.h"

namespace input {

static int GetScreenWidth()  { return GetSystemMetrics(SM_CXVIRTUALSCREEN); }
static int GetScreenHeight() { return GetSystemMetrics(SM_CYVIRTUALSCREEN); }

void MoveMouse(float normalizedX, float normalizedY) {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    int x = static_cast<int>(normalizedX * screenW);
    int y = static_cast<int>(normalizedY * screenH);

    INPUT inputs[1] = {};
    inputs[0].type       = INPUT_MOUSE;
    inputs[0].mi.dx      = (x * 65535) / screenW;
    inputs[0].mi.dy      = (y * 65535) / screenH;
    inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    inputs[0].mi.mouseData = 0;
    SendInput(1, inputs, sizeof(INPUT));
}

void MouseButtonEvent(MouseButton btn, bool down) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    switch (btn) {
        case MouseButton::Left:
            inputs[0].mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;
        case MouseButton::Right:
            inputs[0].mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;
        case MouseButton::Middle:
            inputs[0].mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;
    }
    SendInput(1, inputs, sizeof(INPUT));
}

void MouseWheel(int delta) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_WHEEL;
    inputs[0].mi.mouseData = static_cast<DWORD>(delta);
    SendInput(1, inputs, sizeof(INPUT));
}

void KeyEvent(uint16_t vkCode, bool down) {
    INPUT inputs[1] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vkCode;
    inputs[0].ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, inputs, sizeof(INPUT));
}

void CharInput(wchar_t ch) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

} // namespace input
