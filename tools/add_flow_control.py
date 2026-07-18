"""Add ACK-based flow control to prevent UDP buffer overflow."""

# ===== 1. Add ACK frame type to proto =====
p = r"C:\Users\17410\Desktop\remote control\libs\libproto\include\libproto.h"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

c = c.replace(
    "Resolution     = 0x09,",
    "Resolution     = 0x09,\n    FrameAck       = 0x0A,  // controller->agent: frame received, send next"
)
with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("1. Added FrameAck type to proto")

# ===== 2. Agent: wait for ACK before sending next frame =====
p2 = r"C:\Users\17410\Desktop\remote control\apps\agent\src\agent.cpp"
with open(p2, "r", encoding="utf-8", errors="replace") as f:
    c2 = f.read()

# Add atomic flag for ACK
c2 = c2.replace(
    '#include <cstring>',
    '#include <cstring>\n#include <atomic>'
)

# In MainLoop, add ACK wait after sending a frame:
old_pacing = '''                // Frame-level pacing: limit to ~60 FPS to prevent UDP buffer overflow
                // Each raw 2560x1440 BGRA frame is 14.7MB; sending faster than
                // the controller can drain its 128MB recv buffer causes packet loss.
                auto now = std::chrono::steady_clock::now();
                auto sinceLast = std::chrono::duration_cast<std::chrono::microseconds>(now - lastSendTime).count();
                if (sinceLast < 14000) {
                    std::this_thread::sleep_for(std::chrono::microseconds(14000 - sinceLast));
                }
                lastSendTime = std::chrono::steady_clock::now();'''

new_pacing = '''                // Wait for controller ACK before sending next frame
                // This prevents UDP buffer overflow from 14.7MB raw BGRA frames
                static std::atomic<bool> frameAcked_{true};
                frameAcked_ = false;
                auto ackStart = std::chrono::steady_clock::now();
                while (!frameAcked_ && running_) {
                    channel_.Poll(5, [&frameAcked_](proto::FrameType type, uint64_t,
                                       const uint8_t*, uint16_t) {
                        if (type == proto::FrameType::FrameAck) {
                            frameAcked_ = true;
                        }
                    });
                    auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - ackStart).count();
                    if (waited > 1000) break; // 1s timeout to prevent deadlock
                }'''

c2 = c2.replace(old_pacing, new_pacing)

with open(p2, "w", encoding="utf-8") as f:
    f.write(c2)
print("2. Agent: ACK-based flow control (wait for FrameAck)")

# ===== 3. Controller: send ACK after frame display =====
p3 = r"C:\Users\17410\Desktop\remote control\apps\controller\src\controller.cpp"
with open(p3, "r", encoding="utf-8", errors="replace") as f:
    c3 = f.read()

# After UpdateFrame, send ACK
old_ack = '''                        window_.UpdateFrame(reasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                        static int cfc = 0;
                        if (++cfc == 1) std::cout << "CTRL: FIRST FRAME DISPLAYED" << std::endl;'''

new_ack = '''                        window_.UpdateFrame(reasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                        channel_.SendFrame(proto::FrameType::FrameAck, nullptr, 0);
                        static int cfc = 0;
                        if (++cfc == 1) std::cout << "CTRL: FIRST FRAME DISPLAYED" << std::endl;'''

c3 = c3.replace(old_ack, new_ack)
with open(p3, "w", encoding="utf-8") as f:
    f.write(c3)
print("3. Controller: send FrameAck after each displayed frame")

# ===== 4. Remove frame pacing (replaced by ACK flow control) =====
# The static variables for pacing are no longer needed, but removing them
# would be complex. They'll just be unused.

print("\nFlow control added.")