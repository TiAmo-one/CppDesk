#pragma once
#include <cstdint>
#include <windows.h>

namespace input {

enum class MouseButton : uint8_t { Left = 0, Right = 1, Middle = 2 };

// normalized: [0.0, 1.0] across entire virtual desktop
void MoveMouse(float normalizedX, float normalizedY);
void MouseButtonEvent(MouseButton btn, bool down);
void MouseWheel(int delta);  // +120 = forward one notch (WHEEL_DELTA)
void KeyEvent(uint16_t vkCode, bool down);
void CharInput(wchar_t ch);

} // namespace input
