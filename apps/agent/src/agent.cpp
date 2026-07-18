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

void Agent::MainLoop() {
    using namespace std::chrono;
    constexpr uint32_t MAX_CHUNK = 65480;

    int loopCount = 0;
    bool captureInit = false;

    while (running_) {
        signal_.Poll(0);
        loopCount++;

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
                std::cerr << "[AGENT] Capture: " << capW << "x" << capH << std::endl;
                sendBuf_.resize(65536);
            }

            auto acqResult = capture_.AcquireFrame();
            if (acqResult.valid()) {
                auto& frm = acqResult.frame();
                uint32_t w = frm.width, h = frm.height;
                uint32_t stride = frm.stride;

                uint32_t dstW = w, dstH = h;
                if (w != cachedWidth_ || h != cachedHeight_) {
                    cachedFrame_.resize(w * h * 4);
                    cachedWidth_ = w; cachedHeight_ = h;
                    lastWidth_ = dstW; lastHeight_ = dstH;
                }

                const uint8_t* sendData = cachedFrame_.data();
                uint8_t* dst = cachedFrame_.data();
                const uint8_t* src = frm.data;
                if (stride == w * 4) {
                    memcpy(dst, src, w * h * 4);
                } else {
                    for (uint32_t row = 0; row < h; row++) {
                        memcpy(dst + row * w * 4, src + row * stride, w * 4);
                    }
                }

                uint32_t sendSize = cachedWidth_ * cachedHeight_ * 4;
                auto* vp = (proto::VideoPayload*)sendBuf_.data();
                vp->width = (uint16_t)dstW;
                vp->height = (uint16_t)dstH;
                uint32_t offset = 0;
                uint32_t fragIdx = 0;
                int sendFails = 0;

                while (offset < sendSize && running_ && p2pReady_) {
                    // Poll signal server during long send to detect disconnect promptly
                    if (fragIdx % 50 == 0 && fragIdx > 0) {
                        signal_.Poll(0);
                        if (!p2pReady_) break;
                    }

                    uint16_t chunkSize = (uint16_t)(std::min)((uint32_t)MAX_CHUNK, sendSize - offset);
                    vp->fragmentIndex = fragIdx++;
                    memcpy(sendBuf_.data() + sizeof(proto::VideoPayload), sendData + offset, chunkSize);
                    bool sent = false;
                    for (int retry = 0; retry < 3 && !sent && running_ && p2pReady_; retry++) {
                        sent = channel_.SendFrame(proto::FrameType::Video, sendBuf_.data(),
                                                   (uint16_t)(sizeof(proto::VideoPayload) + chunkSize));
                        if (!sent) {
                            sendFails++;
                            std::this_thread::yield();
                        }
                    }

                    // If every fragment is failing, peer is likely gone
                    if (sendFails > 10 && fragIdx > 0 && (int)fragIdx <= sendFails) {
                        std::cerr << "[AGENT] Too many send failures, peer likely disconnected" << std::endl;
                        p2pReady_ = false;
                        break;
                    }

                    offset += chunkSize;
                }

                static int frameCount = 0;
                static auto lastPaceTime = steady_clock::now();
                ++frameCount;

                auto paceNow = steady_clock::now();
                auto paceSince = duration_cast<milliseconds>(paceNow - lastPaceTime).count();
                if (paceSince < 100) {
                    std::this_thread::sleep_for(milliseconds(100 - paceSince));
                }
                lastPaceTime = steady_clock::now();

                if (frameCount == 1)
                    std::cerr << "[AGENT] Frame 1 sent" << std::endl;
                else if (frameCount % 30 == 0)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent" << std::endl;
            } else {
                std::this_thread::sleep_for(microseconds(500));
            }
        } else {
            if (loopCount <= 3)
                std::cerr << "[AGENT] Waiting for controller..." << std::endl;
            std::this_thread::sleep_for(milliseconds(100));
        }

        // Process incoming input
        channel_.Poll(0, [this](proto::FrameType type, uint64_t,
                                 const uint8_t* data, uint16_t len) {
            switch (type) {
            case proto::FrameType::MouseMove:
                if (serverHost_ != "127.0.0.1" && len >= sizeof(proto::MouseMovePayload)) {
                    auto* m = (const proto::MouseMovePayload*)data;
                    input::MoveMouse(m->x, m->y);
                }
                break;
            case proto::FrameType::MouseBtn:
                if (serverHost_ != "127.0.0.1" && len >= sizeof(proto::MouseBtnPayload)) {
                    auto* b = (const proto::MouseBtnPayload*)data;
                    input::MouseButtonEvent((input::MouseButton)b->button, b->down != 0);
                }
                break;
            case proto::FrameType::KeyEvent:
                if (serverHost_ != "127.0.0.1" && len >= sizeof(proto::KeyEventPayload)) {
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
