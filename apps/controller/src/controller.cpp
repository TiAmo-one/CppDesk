#include "controller.h"
#include <iostream>
#include <chrono>
#include "libinput.h"

Controller::Controller(HINSTANCE hInst, const std::string& serverHost,
                       uint16_t serverPort, const std::string& roomId,
                       const std::string& password)
    : hInst_(hInst), serverHost_(serverHost), serverPort_(serverPort),
      roomId_(roomId), password_(password) {}

Controller::~Controller() {
    running_ = false;
    if (networkThread_.joinable()) networkThread_.join();
    if (codecCtx_) avcodec_free_context(&codecCtx_);
    if (decFrame_) av_frame_free(&decFrame_);
    if (swsCtx_)   sws_freeContext(swsCtx_);
}

bool Controller::InitDecoder(int, int) { return false; }
std::vector<uint8_t> Controller::DecodeFrame(const uint8_t*, int) { return {}; }

void Controller::NetworkThread() {
    constexpr uint32_t CHUNK = 65480;
    auto lastFrameTime = std::chrono::steady_clock::now();

    int pollCount = 0;
    while (running_) {
        signal_.Poll(p2pReady_ ? 0 : 100);
        if (p2pReady_) {
            int pkts = channel_.Poll(10, [this, &lastFrameTime](proto::FrameType type, uint64_t,
                                      const uint8_t* data, uint16_t len) {
                if (type == proto::FrameType::Video) {
                    auto* vp = (const proto::VideoPayload*)data;
                    const uint8_t* rawData = data + sizeof(proto::VideoPayload);
                    int rawLen = len - (int)sizeof(proto::VideoPayload);

                    uint16_t vpWidth = vp->width;
                    uint16_t vpHeight = vp->height;
                    if (vpWidth > 0 && vpHeight > 0 &&
                        (vpWidth != (uint16_t)decWidth_ || vpHeight != (uint16_t)decHeight_)) {
                        std::cout << "CTRL: resolution " << vpWidth << "x" << vpHeight << std::endl;
                        decWidth_ = vpWidth;
                        decHeight_ = vpHeight;
                        resolutionChanged_ = true;
                    }

                    if (resolutionChanged_) {
                        reasmActive_ = false;
                        reasmWritten_ = 0;
                        reasmExpected_ = 0;
                        resolutionChanged_ = false;
                    }

                    if (vp->fragmentIndex == 0 && decWidth_ > 0 && decHeight_ > 0) {
                        reasmExpected_ = decWidth_ * decHeight_ * 4;
                        if (reasmBuf_.size() != reasmExpected_)
                            reasmBuf_.resize(reasmExpected_);
                        reasmWritten_ = 0;
                        reasmActive_ = true;
                    }

                    if (reasmActive_ && reasmExpected_ > 0) {
                        uint32_t offset = vp->fragmentIndex * CHUNK;
                        if (offset < reasmExpected_) {
                            uint32_t copyLen = (std::min)((uint32_t)rawLen, reasmExpected_ - offset);
                            if (offset + copyLen <= reasmBuf_.size()) {
                                memcpy(reasmBuf_.data() + offset, rawData, copyLen);
                                reasmWritten_ += copyLen;
                            }
                        }
                    }

                    if (reasmActive_ && reasmWritten_ >= reasmExpected_ && reasmExpected_ > 0) {
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
                    }
                }
            });

            pollCount++;
            if (pollCount == 1)
                std::cout << "CTRL: first poll, " << pkts << " pkts" << std::endl;
            else if (pollCount % 60 == 0)
                std::cout << "CTRL: poll " << pollCount << ", " << pkts << " pkts" << std::endl;
        }
    }
}

int Controller::Run() {
    std::cout << "CTRL: starting" << std::endl;

    if (!window_.Create(hInst_, L"Remote Desktop", 1280, 720)) {
        std::cerr << "CTRL: window failed" << std::endl;
        return 1;
    }

    ui::UiCallbacks uiCb;
    uiCb.onMouseMove = [this](float x, float y) {
        if (!p2pReady_) return;
        proto::MouseMovePayload m = {x, y};
        channel_.SendFrame(proto::FrameType::MouseMove, &m, sizeof(m));
    };
    uiCb.onMouseButton = [this](int btn, bool down) {
        if (!p2pReady_) return;
        proto::MouseBtnPayload b = {(uint8_t)btn, (uint8_t)(down ? 1 : 0)};
        channel_.SendFrame(proto::FrameType::MouseBtn, &b, sizeof(b));
    };
    uiCb.onMouseWheel = [this](int delta) {
        if (!p2pReady_) return;
        input::MouseWheel(delta);
    };
    uiCb.onKey = [this](uint16_t vk, bool down) {
        if (!p2pReady_) return;
        proto::KeyEventPayload k = {vk, (uint8_t)(down ? 1 : 0)};
        channel_.SendFrame(proto::FrameType::KeyEvent, &k, sizeof(k));
    };
    uiCb.onClose = [this]() { running_ = false; };
    window_.SetCallbacks(uiCb);

    signal_.SetCallbacks({
        .onConnected = [this]() {
            std::cout << "CTRL: signal connected" << std::endl;
            json join;
            join["type"]     = "join";
            join["room"]     = roomId_;
            join["password"] = password_;
            signal_.Send(join.dump());
        },
        .onMessage = [this](const std::string&, const std::string& data) {
            try {
                json j = json::parse(data);
                std::string type = j.value("type", "");

                if (type == "joined") {
                    std::cout << "CTRL: joined" << std::endl;
                } else if (type == "peer_connect") {
                    std::cout << "CTRL: peer_connect, sending SDP" << std::endl;
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
                    signal_.Send(sdpMsg.dump());
                } else if (type == "sdp") {
                    network::SdpInfo remoteSdp;
                    std::string cands = j.value("data", "");
                    size_t pos = 0;
                    while (pos < cands.size()) {
                        auto comma = cands.find(',', pos);
                        std::string cand = cands.substr(pos, comma - pos);
                        if (!cand.empty()) remoteSdp.candidates.push_back(cand);
                        pos = (comma == std::string::npos) ? cands.size() : comma + 1;
                    }
                    sockaddr_in peer = {};
                    if (!remoteSdp.candidates.empty()) {
                        auto& c = remoteSdp.candidates[0];
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
                        std::cout << "CTRL: connect to " << remoteSdp.candidates[0] << std::endl;
                        peerAddr_ = peer;
                        uint8_t key[16];
                        std::cout << "CTRL: KeyExchange..." << std::endl;
                        if (network::KeyExchange(p2pSocket_, peer, key)) {
                            channel_.Init(p2pSocket_.GetNative(), peer);
                            channel_.SetKey(key);
                            p2pReady_ = true;
                            json done; done["type"] = "p2p_established";
                            signal_.Send(done.dump());
                            std::cout << "CTRL: P2P ready" << std::endl;
                        } else {
                            std::cout << "CTRL: KeyExchange FAILED" << std::endl;
                        }
                    } else {
                        std::cout << "CTRL: bad peer addr" << std::endl;
                    }
                } else if (type == "p2p_established") {
                    p2pReady_ = true;
                }
            } catch (...) {}
        },
        .onError = [](const std::string& err) { std::cerr << "CTRL: sig err " << err << std::endl; }
    });

    std::cout << "CTRL: creating P2P socket" << std::endl;
    if (!p2pSocket_.Create(0)) { std::cerr << "CTRL: socket fail" << std::endl; return 1; }
    std::cout << "CTRL: P2P port " << p2pSocket_.GetPort() << std::endl;

    std::cout << "CTRL: connecting signal " << serverHost_ << ":" << serverPort_ << std::endl;
    if (!signal_.Connect(serverHost_.c_str(), serverPort_)) {
        std::cerr << "CTRL: signal connect fail" << std::endl;
        return 1;
    }

    networkThread_ = std::thread(&Controller::NetworkThread, this);

    window_.Show(SW_SHOW);
    int ret = window_.Run();

    running_ = false;
    if (networkThread_.joinable()) networkThread_.join();
    return ret;
}
