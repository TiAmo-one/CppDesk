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
        std::cerr << "[AGENT] Failed to create P2P socket" << std::endl;
        return 1;
    }
    std::cerr << "[AGENT] P2P socket on port " << p2pSocket_.GetPort() << std::endl;

    std::cerr << "[AGENT] Connecting to signal server " << serverHost_ << ":" << serverPort_ << "..." << std::endl;
    if (!signal_.Connect(serverHost_.c_str(), serverPort_)) {
        std::cerr << "[AGENT] Failed to connect to signal server" << std::endl;
        return 1;
    }

    std::cerr << "[AGENT] Waiting for P2P..." << std::endl;
    while (running_ && !p2pReady_) {
        signal_.Poll(100);
    }
    if (!p2pReady_) { std::cerr << "[AGENT] P2P not ready, exiting" << std::endl; return 1; }

    std::cerr << "[AGENT] Initializing capture..." << std::endl;
    if (!capture_.Init(0)) {
        std::cerr << "[AGENT] Failed to init capture" << std::endl;
        return 1;
    }
    uint32_t w, h;
    capture_.GetCurrentResolution(w, h);
    std::cerr << "[AGENT] Capture: " << w << "x" << h << std::endl;

    std::cerr << "[AGENT] Entering MainLoop..." << std::endl;
    MainLoop();

    return 0;
}

void Agent::MainLoop() {
    constexpr uint16_t MAX_CHUNK = 65480;
    sendBuf_.resize(sizeof(proto::VideoPayload) + MAX_CHUNK);
    std::cerr << "[AGENT] MainLoop started" << std::endl;

    int loopCount = 0;
    while (running_) {
        loopCount++;
        auto frame = capture_.AcquireFrame(8);
        if (frame.valid()) {
            auto& f = frame.frame();
            uint32_t rowBytes = f.width * 4;
            if (cachedFrame_.size() != rowBytes * f.height)
                cachedFrame_.resize(rowBytes * f.height);
            for (uint32_t row = 0; row < f.height; row++)
                memcpy(cachedFrame_.data() + row * rowBytes, f.data + row * f.stride, rowBytes);
            cachedWidth_ = f.width;
            cachedHeight_ = f.height;
            if (loopCount == 1)
                std::cerr << "[AGENT] First frame captured: " << cachedWidth_ << "x" << cachedHeight_ << std::endl;
        }

        if (!cachedFrame_.empty() && running_) {
            {
                prevWidth_ = cachedWidth_;
                prevHeight_ = cachedHeight_;

                uint32_t dstW = cachedWidth_, dstH = cachedHeight_;
                if (dstW != lastWidth_ || dstH != lastHeight_) {
                    proto::ResolutionPayload res = {dstW, dstH};
                    channel_.SendFrame(proto::FrameType::Resolution, &res, sizeof(res));
                    lastWidth_ = dstW; lastHeight_ = dstH;
                }

                const uint8_t* sendData = cachedFrame_.data();
                uint32_t sendSize = cachedWidth_ * cachedHeight_ * 4;

                auto* vp = (proto::VideoPayload*)sendBuf_.data();
                vp->width = (uint16_t)dstW;
                vp->height = (uint16_t)dstH;
                uint32_t offset = 0;
                uint32_t fragIdx = 0;
                int sendFails = 0;
                int sendSuccess = 0;
                std::cerr << "[AGENT] SEND: " << sendSize << " bytes, ~" << ((sendSize+MAX_CHUNK-1)/MAX_CHUNK) << " frags" << std::endl;

                while (offset < sendSize && running_) {
                    uint16_t chunkSize = (uint16_t)(std::min)((uint32_t)MAX_CHUNK, sendSize - offset);
                    vp->fragmentIndex = fragIdx++;
                    memcpy(sendBuf_.data() + sizeof(proto::VideoPayload), sendData + offset, chunkSize);
                    bool sent = false;
                    for (int retry = 0; retry < 3 && !sent && running_; retry++) {
                        sent = channel_.SendFrame(proto::FrameType::Video, sendBuf_.data(),
                                                   (uint16_t)(sizeof(proto::VideoPayload) + chunkSize));
                        if (!sent) {
                            sendFails++;
                            std::this_thread::yield();
                        } else {
                            sendSuccess++;
                        }
                    }
                    offset += chunkSize;
                }
                std::cerr << "[AGENT] SEND done: " << sendSuccess << " ok, " << sendFails << " fails, " << fragIdx << " frags" << std::endl;

                static int frameCount = 0;
                static auto lastPaceTime = std::chrono::steady_clock::now();
                ++frameCount;
                // Frame pacing: limit to ~30 FPS to prevent UDP buffer overflow
                // 14.7MB raw BGRA at 2560x1440 needs controlled send rate
                auto paceNow = std::chrono::steady_clock::now();
                auto paceSince = std::chrono::duration_cast<std::chrono::milliseconds>(paceNow - lastPaceTime).count();
                if (paceSince < 100) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 - paceSince));
                }
                lastPaceTime = std::chrono::steady_clock::now();
                if (frameCount == 1)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent: " << sendSuccess << " ok, " << sendFails << " retries, " << fragIdx << " frags" << std::endl;
                else if (frameCount % 30 == 0)
                    std::cerr << "[AGENT] Frame " << frameCount << " sent" << std::endl;
            }
        } else {
            if (loopCount <= 3)
                std::cerr << "[AGENT] Loop " << loopCount << ": cachedFrame empty or not running" << std::endl;
            std::this_thread::sleep_for(std::chrono::microseconds(500));
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

        signal_.Poll(0);
    }
    std::cerr << "[AGENT] MainLoop exited" << std::endl;
}
