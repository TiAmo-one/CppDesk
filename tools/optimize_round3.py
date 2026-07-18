"""Round 3: fix UDP buffer overflow + frame pacing."""

# ===== PEER CHANNEL: increase recv buffer to 128MB =====
p = r"C:\Users\17410\Desktop\remote control\libs\libnetwork\src\peer_channel.cpp"
with open(p, "r", encoding="utf-8", errors="replace") as f:
    c = f.read()

c = c.replace("int bufSize = 16 * 1024 * 1024;", "int bufSize = 128 * 1024 * 1024;")
with open(p, "w", encoding="utf-8") as f:
    f.write(c)
print("1. SO_RCVBUF/SO_SNDBUF: 16MB -> 128MB")

# ===== AGENT: add frame-level pacing (max ~60 FPS equivalent) =====
p2 = r"C:\Users\17410\Desktop\remote control\apps\agent\src\agent.cpp"
with open(p2, "r", encoding="utf-8", errors="replace") as f:
    c2 = f.read()

# Add frame pacing: track send time, throttle to ~60fps
old_after_send = '''                static int frameCount = 0;
                ++frameCount;
                if (frameCount == 1)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent: " << sendSuccess << " ok, " << sendFails << " retries, " << fragIdx << " frags" << std::endl;
                else if (frameCount % 30 == 0)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent" << std::endl;'''

new_after_send = '''                static int frameCount = 0;
                static auto lastSendTime = std::chrono::steady_clock::now();
                ++frameCount;
                if (frameCount == 1)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent: " << sendSuccess << " ok, " << sendFails << " retries, " << fragIdx << " frags" << std::endl;
                else if (frameCount % 30 == 0)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent" << std::endl;

                // Frame-level pacing: limit to ~60 FPS to prevent UDP buffer overflow
                // Each raw 2560x1440 BGRA frame is 14.7MB; sending faster than
                // the controller can drain its 128MB recv buffer causes packet loss.
                auto now = std::chrono::steady_clock::now();
                auto sinceLast = std::chrono::duration_cast<std::chrono::microseconds>(now - lastSendTime).count();
                if (sinceLast < 14000) {
                    std::this_thread::sleep_for(std::chrono::microseconds(14000 - sinceLast));
                }
                lastSendTime = std::chrono::steady_clock::now();'''

c2 = c2.replace(old_after_send, new_after_send)
with open(p2, "w", encoding="utf-8") as f:
    f.write(c2)
print("2. Added frame-level pacing: max ~70 FPS (14ms between frames)")

print("\nRound 3 complete.")