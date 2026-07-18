"""Round 2 optimizations: fix fingerprint bug, reduce capture timeout, optimize copy."""

import re

# ===== AGENT =====
p = r"C:\Users\17410\Desktop\remote control\apps\agent\src\agent.cpp"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

# -----------------------------------------------------------
# 1. Fix fingerprint comparison alignment bug
# -----------------------------------------------------------
old_comp = '''                const uint8_t* cur = cachedFrame_.data();
                const uint8_t* prv = prevFrame_.data();
                size_t sz = cachedFrame_.size();
                changed = false;
                for (size_t i = 0; i < sz; i += 1024) {
                    if (*(const uint32_t*)(cur + i) != *(const uint32_t*)(prv + i)) {
                        changed = true;
                        break;
                    }
                }'''

new_comp = '''                const uint32_t* fpPrev = (const uint32_t*)prevFrame_.data();
                const uint8_t* cur = cachedFrame_.data();
                size_t fpCount = prevFrame_.size() / 4;
                size_t sz = cachedFrame_.size();
                changed = false;
                for (size_t i = 0; i < fpCount; i++) {
                    size_t off = i * 1024;
                    if (off >= sz) break;
                    if (*(const uint32_t*)(cur + off) != fpPrev[i]) {
                        changed = true;
                        break;
                    }
                }'''

c = c.replace(old_comp, new_comp)
print("1. Fixed fingerprint comparison alignment bug")

# -----------------------------------------------------------
# 2. Reduce AcquireFrame timeout 8ms -> 2ms
# -----------------------------------------------------------
c = c.replace('capture_.AcquireFrame(8)', 'capture_.AcquireFrame(2)')
print("2. Reduced AcquireFrame timeout: 8ms -> 2ms")

# -----------------------------------------------------------
# 3. Optimize row copy: single memcpy when stride == rowBytes
# -----------------------------------------------------------
old_copy = '''            uint32_t rowBytes = f.width * 4;
            if (cachedFrame_.size() != rowBytes * f.height)
                cachedFrame_.resize(rowBytes * f.height);
            for (uint32_t row = 0; row < f.height; row++)
                memcpy(cachedFrame_.data() + row * rowBytes, f.data + row * f.stride, rowBytes);'''

new_copy = '''            uint32_t rowBytes = f.width * 4;
            uint32_t totalBytes = rowBytes * f.height;
            if (cachedFrame_.size() != totalBytes)
                cachedFrame_.resize(totalBytes);
            if (f.stride == (int)rowBytes) {
                memcpy(cachedFrame_.data(), f.data, totalBytes);
            } else {
                for (uint32_t row = 0; row < f.height; row++)
                    memcpy(cachedFrame_.data() + row * rowBytes, f.data + row * f.stride, rowBytes);
            }'''

c = c.replace(old_copy, new_copy)
print("3. Optimized row copy: single memcpy when stride == rowBytes")

# -----------------------------------------------------------
# 4. Fast-path input processing: skip signal poll when P2P ready
#    (signal is only needed for setup/teardown, not during streaming)
# -----------------------------------------------------------
old_end = '''        signal_.Poll(0);
    }
    std::cerr << "[AGENT] MainLoop exited" << std::endl;'''

new_end = '''        if (!p2pReady_)
            signal_.Poll(0);
    }
    std::cerr << "[AGENT] MainLoop exited" << std::endl;'''

c = c.replace(old_end, new_end)
print("4. Fast-path: skip signal poll when P2P ready")

with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("\nAgent round 2 optimization complete.")

# ===== CONTROLLER =====
p2 = r"C:\Users\17410\Desktop\remote control\apps\controller\src\controller.cpp"
with open(p2, "r", encoding="utf-8", errors="replace") as f:
    c2 = f.read()

# 5. Controller: also skip signal poll when P2P ready
old_sig = '''        signal_.Poll(p2pReady_ ? 0 : 100);'''
new_sig = '''        if (!p2pReady_) signal_.Poll(100);'''
c2 = c2.replace(old_sig, new_sig)
print("5. Controller: skip signal poll when P2P ready")

with open(p2, "w", encoding="utf-8") as f:
    f.write(c2)
print("Controller round 2 optimization complete.")