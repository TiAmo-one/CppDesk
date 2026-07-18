#include "agent.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>

Agent::Agent(const std::string& serverHost, uint16_t serverPort,
             const std::string& roomId, const std::string& password)
    : serverHost_(serverHost), serverPort_(serverPort),
      roomId_(roomId), password_(password) {}

Agent::~Agent() { running_ = false; }

int Agent::Run() {
    std::cerr << "[AGENT] Starting..." << std::endl;

    signal_.SetCallbacks({
        .onConnected = [this]() {
            std::cerr << "[AGENT] Connected to signal server" << std::endl;
            json reg;
            reg["type"]     = "register";
            reg["room"]     = roomId_;
            reg["password"] = password_;
            signal_.Send(reg.dump());
        },
        .onMessage = [this](const std::string&, const std::string& data) {
            try {
                json j = json::parse(data);
                std::string type = j.value("type", "");

                if (type == "registered") {
                    signalRegistered_ = true;
                    std::cerr << "[AGENT] Registered, waiting for controller..." << std::endl;

                } else if (type == "peer_connect") {
                    std::cerr << "[AGENT] peer_connect received, sending SDP..." << std::endl;
                    sockaddr_in stunAddr = {};
                    auto sdp = network::BuildSdp(p2pSocket_.GetNative(), stunAddr, p2pSocket_.GetPort());
                    json sdpMsg;
                    sdpMsg["type"] = "sdp";
                    std::string cands;
                    for (auto& c : sdp.candidates) {
                        if (!cands.empty()) cands += ",";
                        cands += c;
                    }
                    sdpMsg["data"] = cands;
                    std::cerr << "[AGENT] SDP candidates: " << cands << std::endl;
                    signal_.Send(sdpMsg.dump());

                } else if (type == "sdp") {
                    remoteSdp_.candidates.clear();
                    std::string cands = j.value("data", "");
                    std::cerr << "[AGENT] Received remote SDP: " << cands << std::endl;
                    size_t pos = 0;
                    while (pos < cands.size()) {
                        auto comma = cands.find(',', pos);
                        std::string cand = cands.substr(pos, comma - pos);
                        if (!cand.empty()) remoteSdp_.candidates.push_back(cand);
                        pos = (comma == std::string::npos) ? cands.size() : comma + 1;
                    }

                    sockaddr_in peer = {};
                    if (!remoteSdp_.candidates.empty()) {
                        auto& c = remoteSdp_.candidates[0];
                        auto colon = c.find(":");
                        if (colon != std::string::npos) {
                            std::string ip = c.substr(0, colon);
                            uint16_t port = (uint16_t)std::stoi(c.substr(colon + 1));
                            peer.sin_family = AF_INET;
                            peer.sin_port = htons(port);
                            inet_pton(AF_INET, ip.c_str(), &peer.sin_addr);
                        }
                    }
                    if (peer.sin_port != 0) {
                        std::cerr << "[AGENT] Direct connect to " << remoteSdp_.candidates[0] << std::endl;
                        peerAddr_ = peer;

                        uint8_t key[16];
                        std::cerr << "[AGENT] Starting KeyExchange..." << std::endl;
                        if (network::KeyExchange(p2pSocket_, peer, key)) {
                            channel_.Init(p2pSocket_.GetNative(), peer);
                            channel_.SetKey(key);
                            p2pReady_ = true;
                            json done;
                            done["type"] = "p2p_established";
                            signal_.Send(done.dump());
                            std::cerr << "[AGENT] P2P ready!" << std::endl;
                        } else {
                            std::cerr << "[AGENT] KeyExchange FAILED!" << std::endl;
                        }
                    } else {
                        std::cerr << "[AGENT] Failed to parse remote address" << std::endl;
                    }

                } else if (type == "peer_disconnect") {
                    std::cerr << "[AGENT] Controller disconnected" << std::endl;
                    p2pReady_ = false;
                }
            } catch (...) { std::cerr << "[AGENT] Exception in onMessage" << std::endl; }
        },
        .onError = [](const std::string& err) {
            std::cerr << "[AGENT] Signal error: " << err << std::endl;
        }
    });

    std::cerr << "[AGENT] Creating P2P socket..." << std::endl;
    if (!p2pSocket_.Create(0)) {
        std::cerr << "[AGENT] P2P socket creation FAILED" << std::endl;
        return 1;
    }
    std::cerr << "[AGENT] P2P socket on port " << p2pSocket_.GetPort() << std::endl;

    std::cerr << "[AGENT] Connecting to signal server " << serverHost_ << ":" << serverPort_ << "..." << std::endl;
    if (!signal_.Connect(serverHost_.c_str(), serverPort_)) {
        std::cerr << "[AGENT] Signal connect FAILED" << std::endl;
        return 1;
    }

    MainLoop();
    return 0;
}

static void SendFragment(network::PeerChannel& channel, const std::vector<uint8_t>& buf,
                         proto::FrameType ftype, const void* hdr, int hdrSize,
                         uint32_t totalSize, std::atomic<bool>& p2pReady, std::atomic<bool>& running,
                         int& sendFails) {
    constexpr uint32_t MAX_CHUNK = 65472;
    uint32_t totalFrags = (totalSize + MAX_CHUNK - 1) / MAX_CHUNK;
    if (totalFrags == 0) totalFrags = 1;
    uint32_t offset = 0;
    for (uint32_t fi = 0; fi < totalFrags && running && p2pReady; fi++) {
        if (fi > 0 && fi % 50 == 0) {} // signal poll is outside

        uint16_t chunk = (uint16_t)(std::min)((uint32_t)MAX_CHUNK, totalSize - offset);
        std::vector<uint8_t> msg(hdrSize + chunk);
        memcpy(msg.data(), hdr, hdrSize);
        // Patch fragmentIndex and totalFragments
        uint32_t* fiPtr = (uint32_t*)(msg.data()); // first field is fragmentIndex
        *fiPtr = fi;
        uint32_t* tfPtr = fiPtr + 1;
        *tfPtr = totalFrags;
        memcpy(msg.data() + hdrSize, buf.data() + offset, chunk);

        bool sent = false;
        for (int retry = 0; retry < 3 && !sent && running && p2pReady; retry++) {
            sent = channel.SendFrame(ftype, msg.data(), (uint16_t)msg.size());
            if (!sent) { sendFails++; std::this_thread::yield(); }
        }
        if (sendFails > 10) {
            std::cerr << "[AGENT] Too many send failures" << std::endl;
            p2pReady = false;
            return;
        }
        offset += chunk;
    }
}

void Agent::MainLoop() {
    using namespace std::chrono;
    std::cerr << "[AGENT] Entering MainLoop..." << std::endl;

    bool captureInit = false;
    int frameCount = 0;
    int sendFails = 0;
    auto lastFrameTime = steady_clock::now();
    sendBuf_.resize(65536);

    while (running_) {
        signal_.Poll(0);

        if (p2pReady_) {
            if (!captureInit) {
                std::cerr << "[AGENT] Initializing capture..." << std::endl;
                if (!capture_.Init()) {
                    std::cerr << "[AGENT] Capture init FAILED" << std::endl;
                    std::this_thread::sleep_for(milliseconds(1000));
                    continue;
                }
                captureInit = true;
                uint32_t capW, capH;
                capture_.GetCurrentResolution(capW, capH);
                std::cerr << "[AGENT] Capture: " << capW << "x" << capH << " (dirty-rect mode)" << std::endl;
                cachedFrame_.resize(capW * capH * 4);
                cachedWidth_ = capW; cachedHeight_ = capH;
                sendFails = 0;
            }

            auto acqResult = capture_.AcquireFrame(30); // 30ms = ~33 FPS max
            if (acqResult.valid()) {
                auto& frm = acqResult.frame();
                uint32_t w = frm.width, h = frm.height;
                uint32_t stride = frm.stride;

                if (w != cachedWidth_ || h != cachedHeight_) {
                    cachedFrame_.resize(w * h * 4);
                    cachedWidth_ = w; cachedHeight_ = h;
                }

                // Copy full frame to local buffer
                uint8_t* dst = cachedFrame_.data();
                const uint8_t* src = frm.data;
                if (stride == w * 4) {
                    memcpy(dst, src, w * h * 4);
                } else {
                    for (uint32_t row = 0; row < h; row++)
                        memcpy(dst + row * w * 4, src + row * stride, w * 4);
                }

                auto& dirtyRects = acqResult.dirtyRects();
                int rectCount = (int)dirtyRects.size();

                // Always send at least one rect on first frame or if dirty rects empty
                bool forceFull = (frameCount == 0 || rectCount == 0);
                if (forceFull) {
                    // Send full frame as one big rect
                    proto::DirtyRectPayload drp = {};
                    drp.fragmentIndex  = 0;
                    drp.totalFragments = 0; // patched in SendFragment
                    drp.frameWidth  = (uint16_t)w;
                    drp.frameHeight = (uint16_t)h;
                    drp.left   = 0;
                    drp.top    = 0;
                    drp.right  = (int16_t)w;
                    drp.bottom = (int16_t)h;

                    int rectSize = w * h * 4;
                    SendFragment(channel_, cachedFrame_, proto::FrameType::DirtyRect,
                                 &drp, sizeof(drp), rectSize, p2pReady_, running_, sendFails);
                } else {
                    // Send each dirty rect
                    for (const auto& rect : dirtyRects) {
                        if (!p2pReady_ || !running_) break;

                        int rw = rect.right - rect.left;
                        int rh = rect.bottom - rect.top;
                        if (rw <= 0 || rh <= 0) continue;

                        // Skip tiny rects (likely noise)
                        if (rw * rh < 64) continue;

                        proto::DirtyRectPayload drp = {};
                        drp.fragmentIndex  = 0;
                        drp.totalFragments = 0;
                        drp.frameWidth  = (uint16_t)w;
                        drp.frameHeight = (uint16_t)h;
                        drp.left   = (int16_t)rect.left;
                        drp.top    = (int16_t)rect.top;
                        drp.right  = (int16_t)rect.right;
                        drp.bottom = (int16_t)rect.bottom;

                        // Extract rect pixels into temp buffer
                        std::vector<uint8_t> rectPixels(rw * rh * 4);
                        for (int row = 0; row < rh; row++) {
                            memcpy(rectPixels.data() + row * rw * 4,
                                   cachedFrame_.data() + (rect.top + row) * w * 4 + rect.left * 4,
                                   rw * 4);
                        }

                        SendFragment(channel_, rectPixels, proto::FrameType::DirtyRect,
                                     &drp, sizeof(drp), rectPixels.size(),
                                     p2pReady_, running_, sendFails);

                        // Poll signal server periodically
                        static int rectSendCount = 0;
                        if (++rectSendCount % 5 == 0) {
                            signal_.Poll(0);
                            if (!p2pReady_) break;
                        }
                    }
                }

                frameCount++;
                if (frameCount == 1)
                    std::cerr << "[AGENT] First frame sent (dirty rects: " << rectCount << ")" << std::endl;
                else if (frameCount % 60 == 0)
                    std::cerr << "[AGENT] Frame " << frameCount << " (" << rectCount << " rects)" << std::endl;

                // 16ms pacing = 60 FPS capture rate (with dirty rects, bandwidth is low)
                auto now = steady_clock::now();
                auto since = duration_cast<milliseconds>(now - lastFrameTime).count();
                if (since < 16) {
                    std::this_thread::sleep_for(milliseconds(16 - since));
                }
                lastFrameTime = steady_clock::now();
                sendFails = 0; // reset on successful frame
            }
        } else {
            std::this_thread::sleep_for(milliseconds(100));
        }

        // Process incoming input
        channel_.Poll(0, [this](proto::FrameType type, uint64_t,
                                 const uint8_t* data, uint16_t len) {
            switch (type) {
            case proto::FrameType::MouseMove:
                if (serverHost_ == "127.0.0.1") break;
                if (len >= sizeof(proto::MouseMovePayload)) {
                    auto* m = (const proto::MouseMovePayload*)data;
                    input::MoveMouse(m->x, m->y);
                }
                break;
            case proto::FrameType::MouseBtn:
                if (serverHost_ == "127.0.0.1") break;
                if (len >= sizeof(proto::MouseBtnPayload)) {
                    auto* b = (const proto::MouseBtnPayload*)data;
                    input::MouseButtonEvent((input::MouseButton)b->button, b->down != 0);
                }
                break;
            case proto::FrameType::KeyEvent:
                if (serverHost_ == "127.0.0.1") break;
                if (len >= sizeof(proto::KeyEventPayload)) {
                    auto* k = (const proto::KeyEventPayload*)data;
                    input::KeyEvent(k->vkCode, k->down != 0);
                }
                break;
            default: break;
            }
        });

        clipboard::ClipboardData cbData;
        if (clipboard_.Check(cbData)) {
            if (cbData.hasText) {
                int utf8len = WideCharToMultiByte(CP_UTF8, 0, cbData.text.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (utf8len > 1) {
                    std::string utf8(utf8len - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, cbData.text.c_str(), -1, &utf8[0], utf8len, nullptr, nullptr);
                    channel_.SendFrame(proto::FrameType::ClipboardText, utf8.data(), (uint16_t)utf8.size());
                }
            }
        }
    }
    std::cerr << "[AGENT] MainLoop exited" << std::endl;
}



