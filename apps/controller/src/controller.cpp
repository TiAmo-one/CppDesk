#include "controller.h"
#include <iostream>
#include <chrono>
#include "libinput.h"
#include "libinput.h"
#include <chrono>

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

bool Controller::InitDecoder(int width, int height) {
    if (width == decWidth_ && height == decHeight_ && codecCtx_) return true;
    if (codecCtx_) { avcodec_free_context(&codecCtx_); av_frame_free(&decFrame_); sws_freeContext(swsCtx_); }

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) return false;

    codecCtx_ = avcodec_alloc_context3(codec);
    codecCtx_->width   = width;
    codecCtx_->height  = height;
    codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) return false;

    decFrame_ = av_frame_alloc();
    decWidth_ = width; decHeight_ = height;

    swsCtx_ = sws_getContext(width, height, AV_PIX_FMT_YUV420P,
                              width, height, AV_PIX_FMT_BGRA,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    return true;
}

std::vector<uint8_t> Controller::DecodeFrame(const uint8_t* data, int len) {
    std::vector<uint8_t> bgra;
    if (!codecCtx_) return bgra;

    AVPacket* pkt = av_packet_alloc();
    pkt->data = (uint8_t*)data; pkt->size = len;

    if (avcodec_send_packet(codecCtx_, pkt) < 0) { av_packet_free(&pkt); return bgra; }
    if (avcodec_receive_frame(codecCtx_, decFrame_) < 0) { av_packet_free(&pkt); return bgra; }

    bgra.resize(decWidth_ * decHeight_ * 4);
    uint8_t* dst[1] = { bgra.data() };
    int dstStride[1] = { decWidth_ * 4 };
    sws_scale(swsCtx_, decFrame_->data, decFrame_->linesize, 0,
              decHeight_, dst, dstStride);

    av_packet_free(&pkt);
    return bgra;
}

void Controller::NetworkThread() {
    while (running_) {
        signal_.Poll(100);
        if (p2pReady_) {
            channel_.Poll(50, [this](proto::FrameType type, uint64_t,
                                      const uint8_t* data, uint16_t len) {
                if (type == proto::FrameType::Video) {
                    auto* vp = (const proto::VideoPayload*)data;
                    const uint8_t* nalData = data + sizeof(proto::VideoPayload);
                    int nalLen = len - (int)sizeof(proto::VideoPayload);

                    if (!codecCtx_) InitDecoder(decWidth_ ? decWidth_ : 1920, decHeight_ ? decHeight_ : 1080);
                    auto bgra = DecodeFrame(nalData, nalLen);
                    if (!bgra.empty())
                        window_.UpdateFrame(bgra.data(), decWidth_, decHeight_, decWidth_ * 4);

                } else if (type == proto::FrameType::ClipboardText) {
                    std::string utf8((const char*)data, len);
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
                    if (wlen > 0) {
                        std::wstring wstr(wlen, 0);
                        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);
                        clipboard::ClipboardData cb;
                        cb.text = wstr; cb.hasText = true;
                        clipboard::Monitor::Write(cb);
                    }
                } else if (type == proto::FrameType::FileBlock) {
                    // File receive would go here
                }
            });
        }
    }
}

int Controller::Run() {
    std::cout << "Controller starting..." << std::endl;

    // Create window
    if (!window_.Create(hInst_, L"Remote Desktop", 1280, 720)) {
        std::cerr << "Failed to create window" << std::endl;
        return 1;
    }

    // Input callbacks
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
    uiCb.onFileDrop = [this](const wchar_t* path) {
        if (!p2pReady_) return;
        // File transfer would be triggered here
    };
    uiCb.onClose = [this]() { running_ = false; };
    window_.SetCallbacks(uiCb);

    // Connect signal
    signal_.SetCallbacks({
        .onConnected = [this]() {
            std::cout << "Connected to signal server" << std::endl;
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
                    std::cout << "Joined room, waiting for P2P..." << std::endl;
                } else if (type == "peer_connect") {
                    sockaddr_in stunAddr = {};
                    network::stun::GetMappedAddress(p2pSocket_.GetNative(), stunAddr);
                    auto sdp = network::BuildSdp(p2pSocket_.GetNative(), stunAddr);
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
                    if (network::HolePunch(p2pSocket_, remoteSdp, peer)) {
                        std::cout << "P2P hole punch succeeded!" << std::endl;
                        peerAddr_ = peer;
                        uint8_t key[16];
                        if (network::KeyExchange(p2pSocket_, peer, key)) {
                            channel_.Init(p2pSocket_.GetNative(), peer);
                            channel_.SetKey(key);
                            p2pReady_ = true;
                            json done; done["type"] = "p2p_established";
                            signal_.Send(done.dump());
                            std::cout << "P2P ready, starting stream..." << std::endl;
                        }
                    }
                } else if (type == "p2p_established") {
                    p2pReady_ = true;
                } else if (type == "error") {
                    std::cerr << "Server error: " << j.value("message", "") << std::endl;
                }
            } catch (...) {}
        },
        .onError = [](const std::string& err) { std::cerr << "Signal error: " << err << std::endl; }
    });

    if (!p2pSocket_.Create(0)) { std::cerr << "Failed to create P2P socket" << std::endl; return 1; }
    if (!signal_.Connect(serverHost_.c_str(), serverPort_)) {
        std::cerr << "Failed to connect to signal server" << std::endl;
        return 1;
    }

    networkThread_ = std::thread(&Controller::NetworkThread, this);

    window_.Show(SW_SHOW);
    int ret = window_.Run();

    running_ = false;
    if (networkThread_.joinable()) networkThread_.join();
    return ret;
}
