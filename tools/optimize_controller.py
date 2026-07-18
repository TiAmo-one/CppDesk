"""Optimize controller.cpp for low-latency rendering."""

p = r"C:\Users\17410\Desktop\remote control\apps\controller\src\controller.cpp"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

# 1. Fix frame-drop bug + remove 16ms throttle
#    Old: checks elapsed >= 16, resets reasmActive_ regardless (drops frames < 16ms)
#    New: always display complete frame, reset only after successful display
old_display = '''                    if (reasmActive_ && reasmWritten_ >= reasmExpected_ && reasmExpected_ > 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - lastFrameTime).count();
                        if (elapsed >= 16) {
                            window_.UpdateFrame(reasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                            lastFrameTime = now;
                            static int cfc = 0;
                            if (++cfc == 1) std::cout << "CTRL: FIRST FRAME DISPLAYED" << std::endl;
                        }
                        reasmActive_ = false;
                        reasmWritten_ = 0;
                        reasmExpected_ = 0;
                    }'''

new_display = '''                    if (reasmActive_ && reasmWritten_ >= reasmExpected_ && reasmExpected_ > 0) {
                        window_.UpdateFrame(reasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                        static int cfc = 0;
                        if (++cfc == 1) std::cout << "CTRL: FIRST FRAME DISPLAYED" << std::endl;
                        reasmActive_ = false;
                        reasmWritten_ = 0;
                        reasmExpected_ = 0;
                    }'''

c = c.replace(old_display, new_display)
print("1. Removed 16ms throttle + fixed frame-drop bug")

# 2. Reduce Poll timeout from 10ms to 2ms
c = c.replace('channel_.Poll(10, [this, &lastFrameTime]', 'channel_.Poll(2, [this]')
# Remove unused lastFrameTime reference in capture list
c = c.replace('auto lastFrameTime = std::chrono::steady_clock::now();', '// lastFrameTime removed (no throttle)')
print("2. Reduced Poll timeout: 10ms -> 2ms")

with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("Controller optimization complete.")