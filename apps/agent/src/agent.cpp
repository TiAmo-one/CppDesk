#include "agent.h"
#include <iostream>
#include <chrono>

Agent::Agent(const std::string& serverHost, uint16_t serverPort,
             const std::string& roomId, const std::string& password)
    : serverHost_(serverHost), serverPort_(serverPort),
      roomId_(roomId), password_(password) {}

Agent::~Agent() { running_ = false; }

int Agent::Run() {
    std::cout << "Agent starting..." << std::endl;

    // 1. Connect to signal server
    signal_.SetCallbacks({
        .onConnected = [this]() {
            std::cout << "Connected to signal server" << std::endl;
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
                    std::cout << "Room registered, waiting for controller..." << std::endl;

                } else if (type == "peer_connect") {
                    std::cout << "Controller connected, starting P2P setup..." << std::endl;

                    // Get STUN address
                    sockaddr_in stunAddr = {};
                    if (network::stun::GetMappedAddress(p2pSocket_.GetNative(), stunAddr)) {
                        char ip[64];
                        inet_ntop(AF_INET, &stunAddr.sin_addr, ip, sizeof(ip));
                        std::cout << "STUN: " << ip << ":" << ntohs(stunAddr.sin_port) << std::endl;
                    }

                    auto sdp = network::BuildSdp(p2pSocket_.GetNative(), stunAddr);
                    // Send SDP with candidates
                    json sdpMsg;
                    sdpMsg["type"] = "sdp";
                    std::string cands;
                    for (auto& c : sdp.candidates) {
                        if (!cands.empty()) cands += ",";
                        cands += c;
                    }
                    sdpMsg["data"] = cands;
                    signal_.Send(sdpMsg.dump());

                } else if (type == "sdp") {
                    // Parse remote candidates
                    remoteSdp_.candidates.clear();
                    std::string cands = j.value("data", "");
                    size_t pos = 0;
                    while (pos < cands.size()) {
                        auto comma = cands.find(',', pos);
                        std::string cand = cands.substr(pos, comma - pos);
                        if (!cand.empty()) remoteSdp_.candidates.push_back(cand);
                        pos = (comma == std::string::npos) ? cands.size() : comma + 1;
                    }

                    // Attempt hole punch
                    sockaddr_in peer = {};
                    if (network::HolePunch(p2pSocket_, remoteSdp_, peer)) {
                        std::cout << "P2P hole punch succeeded!" << std::endl;
                        peerAddr_ = peer;

                        // ECDH key exchange
                        uint8_t key[16];
                        if (network::KeyExchange(p2pSocket_, peer, key)) {
                            channel_.Init(p2pSocket_.GetNative(), peer);
                            channel_.SetKey(key);
                            p2pReady_ = true;

                            json done;
                            done["type"] = "p2p_established";
                            signal_.Send(done.dump());

                            std::cout << "P2P ready, starting capture..." << std::endl;
                        }
                    } else {
                        std::cerr << "Hole punch failed" << std::endl;
                        json fail;
                        fail["type"] = "p2p_failed";
                        signal_.Send(fail.dump());
                    }

                } else if (type == "peer_disconnect") {
                    std::cout << "Controller disconnected" << std::endl;
                    p2pReady_ = false;
                }
            } catch (...) {}
        },
        .onError = [](const std::string& err) {
            std::cerr << "Signal error: " << err << std::endl;
        }
    });

    if (!p2pSocket_.Create(0)) {
        std::cerr << "Failed to create P2P socket" << std::endl;
        return 1;
    }

    if (!signal_.Connect(serverHost_.c_str(), serverPort_)) {
        std::cerr << "Failed to connect to signal server " << serverHost_ << ":" << serverPort_ << std::endl;
        return 1;
    }

    // 2. Wait for P2P setup via signal polling
    std::cout << "Waiting for P2P setup..." << std::endl;
    while (running_ && !p2pReady_) {
        signal_.Poll(100);
    }
    if (!p2pReady_) return 1;

    // 3. Init capture + encoder
    if (!capture_.Init(0)) {
        std::cerr << "Failed to init capture" << std::endl;
        return 1;
    }
    uint32_t w, h;
    capture_.GetCurrentResolution(w, h);
    std::cout << "Capture: " << w << "x" << h << std::endl;

    // encoder skipped (x264 not built)

    // 4. Main loop
    std::cout << "Agent running. Press Ctrl+C to stop." << std::endl;
    MainLoop();

    return 0;
}

void Agent::MainLoop() {
    while (running_) {
        // Capture + Encode + Send
        auto frame = capture_.AcquireFrame(16);
        if (frame.valid()) {
            auto& f = frame.frame();
        // Encode disabled (x264 not built)
        }

        // Process incoming input frames
        channel_.Poll(0, [this](proto::FrameType type, uint64_t,
                                 const uint8_t* data, uint16_t len) {
            switch (type) {
            case proto::FrameType::MouseMove:
                if (len >= sizeof(proto::MouseMovePayload)) {
                    auto* m = (const proto::MouseMovePayload*)data;
                    input::MoveMouse(m->x, m->y);
                }
                break;
            case proto::FrameType::MouseBtn:
                if (len >= sizeof(proto::MouseBtnPayload)) {
                    auto* b = (const proto::MouseBtnPayload*)data;
                    input::MouseButtonEvent((input::MouseButton)b->button, b->down != 0);
                }
                break;
            case proto::FrameType::KeyEvent:
                if (len >= sizeof(proto::KeyEventPayload)) {
                    auto* k = (const proto::KeyEventPayload*)data;
                    input::KeyEvent(k->vkCode, k->down != 0);
                }
                break;
            case proto::FrameType::ClipboardText: {
                std::string utf8((const char*)data, len);
                int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
                if (wlen > 0) {
                    std::wstring wstr(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);
                    clipboard::ClipboardData cb;
                    cb.text = wstr; cb.hasText = true;
                    clipboard::Monitor::Write(cb);
                }
                break;
            }
            case proto::FrameType::FileBlock:
                // File block handling would go here
                break;
            default: break;
            }
        });

        // Clipboard monitoring
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
            // File clipboard handling would go here
        }

        // Signal keepalive
        signal_.Poll(0);
    }
}
