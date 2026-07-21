#include "controller.h"
#include <iostream>
#include <chrono>
#include "libnetwork/stun.h"
#include "libinput.h"

Controller::Controller(HINSTANCE hInst, const std::string& serverHost,
                       uint16_t serverPort, const std::string& roomId,
                       const std::string& password)
    : hInst_(hInst), serverHost_(serverHost), serverPort_(serverPort),
      roomId_(roomId), password_(password) {}

Controller::~Controller() {
    running_ = false;
    if (networkThread_.joinable()) networkThread_.join();
}

void Controller::NetworkThread() {
    constexpr uint32_t CHUNK = 65472;
    int pollCount = 0;
    auto lastDisplayTime = std::chrono::steady_clock::now();

    while (running_) {
        signal_.Poll(p2pReady_ ? 1 : 100);
        if (p2pReady_) {
            int pkts = 0;
            if (relayMode_) {
                pkts = relayChannel_.Poll(2, [this, &lastDisplayTime](proto::FrameType type, uint64_t,
                                          const uint8_t* data, uint16_t len) {

                    if (type == proto::FrameType::DirtyRect) {
                        auto* drp = (const proto::DirtyRectPayload*)data;
                        int hdrSize = (int)sizeof(proto::DirtyRectPayload);
                        const uint8_t* rawData = data + hdrSize;
                        int rawLen = len - hdrSize;
                        if (rawLen <= 0) return;

                        uint16_t fw = drp->frameWidth, fh = drp->frameHeight;

                        bool newRect = (drp->left != reasmLeft_ || drp->top != reasmTop_ ||
                                        drp->right != reasmRight_ || drp->bottom != reasmBottom_);

                        if (fw > 0 && fh > 0 && (fw != (uint16_t)decWidth_ || fh != (uint16_t)decHeight_)) {
                            decWidth_ = fw;
                            decHeight_ = fh;
                            reasmBuf_.resize(fw * fh * 4);
                            memset(reasmBuf_.data(), 0, reasmBuf_.size());
                            std::cout << "CTRL: buffer " << fw << "x" << fh << std::endl;
                            newRect = true;
                        }

                        if (decWidth_ == 0 || decHeight_ == 0) return;

                        uint32_t rectW = drp->right - drp->left;
                        uint32_t rectH = drp->bottom - drp->top;
                        if (rectW == 0 || rectH == 0) return;
                        uint32_t rectSize = rectW * rectH * 4;
                        uint32_t totalFrags = drp->totalFragments;
                        if (totalFrags == 0) totalFrags = 1;

                        if (newRect || drp->fragmentIndex == 0) {
                            reasmExpected_ = totalFrags * CHUNK;
                            if (tmpBuf_.size() < reasmExpected_)
                                tmpBuf_.resize(reasmExpected_);
                            reasmWritten_ = 0;
                            reasmActive_ = true;
                            reasmLeft_  = drp->left;
                            reasmTop_   = drp->top;
                            reasmRight_ = drp->right;
                            reasmBottom_= drp->bottom;
                        }

                        if (reasmActive_) {
                            uint32_t offset = drp->fragmentIndex * CHUNK;
                            if (offset < reasmExpected_) {
                                uint32_t copyLen = (std::min)((uint32_t)rawLen, reasmExpected_ - offset);
                                if (offset + copyLen <= tmpBuf_.size()) {
                                    memcpy(tmpBuf_.data() + offset, rawData, copyLen);
                                    reasmWritten_ += copyLen;
                                }
                            }

                            uint32_t actualSize = (std::min)(reasmWritten_, reasmExpected_);
                            if (drp->fragmentIndex == totalFrags - 1 || actualSize >= rectSize) {
                                int l = reasmLeft_, t = reasmTop_;
                                int rw = reasmRight_ - l, rh = reasmBottom_ - t;
                                for (int row = 0; row < rh && row * rw * 4 < (int)actualSize; row++) {
                                    int srcOff = row * rw * 4;
                                    int dstOff = ((t + row) * decWidth_ + l) * 4;
                                    int copyBytes = (std::min)(rw * 4, (int)actualSize - srcOff);
                                    if (dstOff + copyBytes <= (int)reasmBuf_.size()) {
                                        memcpy(reasmBuf_.data() + dstOff, tmpBuf_.data() + srcOff, copyBytes);
                                    }
                                }
                                reasmActive_ = false;
                                hasNewFrame_ = true;
                            }
                        }
                    }

                    else if (type == proto::FrameType::Video) {
                        auto* vp = (const proto::VideoPayload*)data;
                        const uint8_t* rawData = data + sizeof(proto::VideoPayload);
                        int rawLen = len - (int)sizeof(proto::VideoPayload);

                        if (vp->fragmentIndex == 0 && vp->width > 0 && vp->height > 0) {
                            if (vp->width != (uint16_t)decWidth_ || vp->height != (uint16_t)decHeight_) {
                                decWidth_ = vp->width;
                                decHeight_ = vp->height;
                                reasmBuf_.resize(decWidth_ * decHeight_ * 4);
                                std::cout << "CTRL: raw " << decWidth_ << "x" << decHeight_ << std::endl;
                            }
                            rawReasmExpected_ = decWidth_ * decHeight_ * 4;
                            if (rawReasmBuf_.size() != rawReasmExpected_)
                                rawReasmBuf_.resize(rawReasmExpected_);
                            rawReasmWritten_ = 0;
                            rawReasmActive_ = true;
                        }

                        if (rawReasmActive_ && rawReasmExpected_ > 0) {
                            uint32_t offset = vp->fragmentIndex * CHUNK;
                            if (offset < rawReasmExpected_) {
                                uint32_t copyLen = (std::min)((uint32_t)rawLen, rawReasmExpected_ - offset);
                                if (offset + copyLen <= rawReasmBuf_.size()) {
                                    memcpy(rawReasmBuf_.data() + offset, rawData, copyLen);
                                    rawReasmWritten_ += copyLen;
                                }
                            }
                        }

                        if (rawReasmActive_ && rawReasmWritten_ >= rawReasmExpected_ && rawReasmExpected_ > 0) {
                            window_.UpdateFrame(rawReasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                            rawReasmActive_ = false;
                            rawReasmWritten_ = 0;
                            rawReasmExpected_ = 0;
                            static int cfc = 0;
                            if (++cfc == 1) std::cout << "CTRL: FIRST RAW FRAME" << std::endl;
                        }
                    }

                    else if (type == proto::FrameType::Heartbeat) {
                        // keep-alive ack, ignore
                    }
                });
            } else {
                pkts = channel_.Poll(2, [this, &lastDisplayTime](proto::FrameType type, uint64_t,
                                      const uint8_t* data, uint16_t len) {

                    if (type == proto::FrameType::DirtyRect) {
                        auto* drp = (const proto::DirtyRectPayload*)data;
                        int hdrSize = (int)sizeof(proto::DirtyRectPayload);
                        const uint8_t* rawData = data + hdrSize;
                        int rawLen = len - hdrSize;
                        if (rawLen <= 0) return;

                        uint16_t fw = drp->frameWidth, fh = drp->frameHeight;

                        bool newRect = (drp->left != reasmLeft_ || drp->top != reasmTop_ ||
                                        drp->right != reasmRight_ || drp->bottom != reasmBottom_);

                        if (fw > 0 && fh > 0 && (fw != (uint16_t)decWidth_ || fh != (uint16_t)decHeight_)) {
                            decWidth_ = fw;
                            decHeight_ = fh;
                            reasmBuf_.resize(fw * fh * 4);
                            memset(reasmBuf_.data(), 0, reasmBuf_.size());
                            std::cout << "CTRL: buffer " << fw << "x" << fh << std::endl;
                            newRect = true;
                        }

                        if (decWidth_ == 0 || decHeight_ == 0) return;

                        uint32_t rectW = drp->right - drp->left;
                        uint32_t rectH = drp->bottom - drp->top;
                        if (rectW == 0 || rectH == 0) return;
                        uint32_t rectSize = rectW * rectH * 4;
                        uint32_t totalFrags = drp->totalFragments;
                        if (totalFrags == 0) totalFrags = 1;

                        if (newRect || drp->fragmentIndex == 0) {
                            reasmExpected_ = totalFrags * CHUNK;
                            if (tmpBuf_.size() < reasmExpected_)
                                tmpBuf_.resize(reasmExpected_);
                            reasmWritten_ = 0;
                            reasmActive_ = true;
                            reasmLeft_  = drp->left;
                            reasmTop_   = drp->top;
                            reasmRight_ = drp->right;
                            reasmBottom_= drp->bottom;
                        }

                        if (reasmActive_) {
                            uint32_t offset = drp->fragmentIndex * CHUNK;
                            if (offset < reasmExpected_) {
                                uint32_t copyLen = (std::min)((uint32_t)rawLen, reasmExpected_ - offset);
                                if (offset + copyLen <= tmpBuf_.size()) {
                                    memcpy(tmpBuf_.data() + offset, rawData, copyLen);
                                    reasmWritten_ += copyLen;
                                }
                            }

                            uint32_t actualSize = (std::min)(reasmWritten_, reasmExpected_);
                            if (drp->fragmentIndex == totalFrags - 1 || actualSize >= rectSize) {
                                int l = reasmLeft_, t = reasmTop_;
                                int rw = reasmRight_ - l, rh = reasmBottom_ - t;
                                for (int row = 0; row < rh && row * rw * 4 < (int)actualSize; row++) {
                                    int srcOff = row * rw * 4;
                                    int dstOff = ((t + row) * decWidth_ + l) * 4;
                                    int copyBytes = (std::min)(rw * 4, (int)actualSize - srcOff);
                                    if (dstOff + copyBytes <= (int)reasmBuf_.size()) {
                                        memcpy(reasmBuf_.data() + dstOff, tmpBuf_.data() + srcOff, copyBytes);
                                    }
                                }
                                reasmActive_ = false;
                                hasNewFrame_ = true;
                            }
                        }
                    }

                    else if (type == proto::FrameType::Video) {
                        auto* vp = (const proto::VideoPayload*)data;
                        const uint8_t* rawData = data + sizeof(proto::VideoPayload);
                        int rawLen = len - (int)sizeof(proto::VideoPayload);

                        if (vp->fragmentIndex == 0 && vp->width > 0 && vp->height > 0) {
                            if (vp->width != (uint16_t)decWidth_ || vp->height != (uint16_t)decHeight_) {
                                decWidth_ = vp->width;
                                decHeight_ = vp->height;
                                reasmBuf_.resize(decWidth_ * decHeight_ * 4);
                                std::cout << "CTRL: raw " << decWidth_ << "x" << decHeight_ << std::endl;
                            }
                            rawReasmExpected_ = decWidth_ * decHeight_ * 4;
                            if (rawReasmBuf_.size() != rawReasmExpected_)
                                rawReasmBuf_.resize(rawReasmExpected_);
                            rawReasmWritten_ = 0;
                            rawReasmActive_ = true;
                        }

                        if (rawReasmActive_ && rawReasmExpected_ > 0) {
                            uint32_t offset = vp->fragmentIndex * CHUNK;
                            if (offset < rawReasmExpected_) {
                                uint32_t copyLen = (std::min)((uint32_t)rawLen, rawReasmExpected_ - offset);
                                if (offset + copyLen <= rawReasmBuf_.size()) {
                                    memcpy(rawReasmBuf_.data() + offset, rawData, copyLen);
                                    rawReasmWritten_ += copyLen;
                                }
                            }
                        }

                        if (rawReasmActive_ && rawReasmWritten_ >= rawReasmExpected_ && rawReasmExpected_ > 0) {
                            window_.UpdateFrame(rawReasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                            rawReasmActive_ = false;
                            rawReasmWritten_ = 0;
                            rawReasmExpected_ = 0;
                            static int cfc = 0;
                            if (++cfc == 1) std::cout << "CTRL: FIRST RAW FRAME" << std::endl;
                        }
                    }

                    else if (type == proto::FrameType::Heartbeat) {
                        // keep-alive ack, ignore
                    }
                });
            }

            // Display accumulated dirty rects at ~60 FPS
            if (hasNewFrame_ && !reasmBuf_.empty() && decWidth_ > 0 && decHeight_ > 0) {
                auto now = std::chrono::steady_clock::now();
                auto since = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDisplayTime).count();
                if (since >= 16) {
                    window_.UpdateFrame(reasmBuf_.data(), decWidth_, decHeight_, decWidth_ * 4);
                    lastDisplayTime = now;
                    hasNewFrame_ = false;
                    static int cfc2 = 0;
                    if (++cfc2 == 1) std::cout << "CTRL: FIRST DIRTY FRAME " << decWidth_ << "x" << decHeight_ << std::endl;
                }
            }

            pollCount++;
            if (pollCount == 1)
                std::cout << "CTRL: first poll, " << pkts << " pkts" << std::endl;
            else if (pollCount % 120 == 0)
                std::cout << "CTRL: poll " << pollCount << ", " << pkts << " pkts" << std::endl;
        }

        // Keep-alive: send heartbeat every 15s
        auto now = std::chrono::steady_clock::now();
        if (p2pReady_ && std::chrono::duration_cast<std::chrono::seconds>(now - lastKeepAlive_).count() >= 15) {
            if (relayMode_) {
                relayChannel_.SendFrame(proto::FrameType::Heartbeat, nullptr, 0);
            } else {
                channel_.SendFrame(proto::FrameType::Heartbeat, nullptr, 0);
            }
            lastKeepAlive_ = now;
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
        if (relayMode_)
                relayChannel_.SendFrame(proto::FrameType::MouseMove, &m, sizeof(m));
            else
                channel_.SendFrame(proto::FrameType::MouseMove, &m, sizeof(m));
    };
    uiCb.onMouseButton = [this](int btn, bool down) {
        if (!p2pReady_) return;
        proto::MouseBtnPayload b = {(uint8_t)btn, (uint8_t)(down ? 1 : 0)};
        if (relayMode_)
                relayChannel_.SendFrame(proto::FrameType::MouseBtn, &b, sizeof(b));
            else
                channel_.SendFrame(proto::FrameType::MouseBtn, &b, sizeof(b));
    };
    uiCb.onMouseWheel = [this](int delta) {
        if (!p2pReady_) return;
        input::MouseWheel(delta);
    };
    uiCb.onKey = [this](uint16_t vk, bool down) {
        if (!p2pReady_) return;
        proto::KeyEventPayload k = {vk, (uint8_t)(down ? 1 : 0)};
        if (relayMode_)
                relayChannel_.SendFrame(proto::FrameType::KeyEvent, &k, sizeof(k));
            else
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
                    std::cout << "CTRL: peer_connect, generating keypair..." << std::endl;
                    if (!network::GenerateKeyPair(myPublicKey_, mySecretKey_)) {
                        std::cerr << "CTRL: Keypair generation failed!" << std::endl;
                        return;
                    }
                    hasSharedKey_ = false;
                    relayMode_ = false;
                    std::cout << "CTRL: Keypair generated, querying STUN..." << std::endl;
                    sockaddr_in stunAddr = {};
                    if (!network::stun::GetMappedAddress(p2pSocket_.GetNative(), stunAddr)) {
                        std::cerr << "CTRL: STUN query failed, using LAN-only candidates" << std::endl;
                    } else {
                        char ip[64];
                        inet_ntop(AF_INET, &stunAddr.sin_addr, ip, sizeof(ip));
                        std::cerr << "CTRL: STUN mapped: " << ip << ":" << ntohs(stunAddr.sin_port) << std::endl;
                    }
                    auto sdp = network::BuildSdp(p2pSocket_.GetNative(), stunAddr, p2pSocket_.GetPort());
                    json sdpMsg;
                    sdpMsg["type"] = "sdp";
                    std::string cands;
                    for (auto& c : sdp.candidates) {
                        if (!cands.empty()) cands += ",";
                        cands += c;
                    }
                    sdpMsg["data"] = cands;
                    {
                        static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                        std::string pk;
                        pk.reserve(44);
                        for (int i = 0; i < 32; i += 3) {
                            uint32_t n = (uint32_t)myPublicKey_[i] << 16;
                            if (i + 1 < 32) n |= (uint32_t)myPublicKey_[i + 1] << 8;
                            if (i + 2 < 32) n |= (uint32_t)myPublicKey_[i + 2];
                            pk += b64[(n >> 18) & 0x3F];
                            pk += b64[(n >> 12) & 0x3F];
                            pk += (i + 1 < 32) ? b64[(n >> 6) & 0x3F] : '=';
                            pk += (i + 2 < 32) ? b64[n & 0x3F] : '=';
                        }
                        sdpMsg["public_key"] = pk;
                    }
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

                    if (!hasSharedKey_) {
                        if (j.contains("public_key") && j["public_key"].is_string()) {
                            std::string pkB64 = j["public_key"].get<std::string>();
                            uint8_t peerPk[32] = {};
                            try {
                                static const unsigned char dtable[256] = {
                                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                                    255,255,255,255,255,255,255,255,255,255,255,62, 255,255,255,63,
                                    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255,255,255,0,  255,255,
                                    255,0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                                    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255,255,255,255,255,
                                    255,26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                                    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255,255,255,255,255,
                                };
                                uint32_t accum = 0; int bits = 0; int outIdx = 0;
                                for (size_t i = 0; i < pkB64.size() && outIdx < 32; i++) {
                                    unsigned char v = dtable[(unsigned char)pkB64[i]];
                                    if (v == 255) continue;
                                    accum = (accum << 6) | v;
                                    bits += 6;
                                    if (bits >= 8) { bits -= 8; peerPk[outIdx++] = (uint8_t)(accum >> bits); }
                                }
                                if (outIdx == 32 && network::DeriveSharedKey(peerPk, mySecretKey_, sharedKey_)) {
                                    hasSharedKey_ = true;
                                    std::cout << "CTRL: Shared key derived from SDP" << std::endl;
                                } else {
                                    std::cerr << "CTRL: Failed to derive shared key" << std::endl;
                                }
                            } catch (...) {
                                std::cerr << "CTRL: Public key decode error" << std::endl;
                            }
                        }
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
                        sockaddr_in punchedPeer = {};
                        bool punched = network::HolePunch(p2pSocket_, remoteSdp, punchedPeer);

                        if (punched) {
                            char ip[64];
                            inet_ntop(AF_INET, &punchedPeer.sin_addr, ip, sizeof(ip));
                            std::cout << "CTRL: hole punch OK " << ip << ":" << ntohs(punchedPeer.sin_port) << std::endl;
                            peerAddr_ = punchedPeer;
                            relayMode_ = false;
                            if (hasSharedKey_) {
                                channel_.Init(p2pSocket_.GetNative(), punchedPeer);
                                channel_.SetKey(sharedKey_);
                                p2pReady_ = true;
                                lastKeepAlive_ = std::chrono::steady_clock::now();
                                json done; done["type"] = "p2p_established";
                                signal_.Send(done.dump());
                                std::cout << "CTRL: P2P channel (direct)" << std::endl;
                            } else {
                                std::cerr << "CTRL: P2P punched but no shared key!" << std::endl;
                            }
                        } else {
                            std::cout << "CTRL: hole punch failed, falling back to relay..." << std::endl;
                            if (hasSharedKey_) {
                                relayChannel_.Init(&signal_);
                                relayChannel_.SetKey(sharedKey_);
                                relayMode_ = true;
                                p2pReady_ = true;
                                lastKeepAlive_ = std::chrono::steady_clock::now();
                                json done; done["type"] = "p2p_established";
                                signal_.Send(done.dump());
                                std::cout << "CTRL: Relay channel (relay mode)" << std::endl;
                            } else {
                                std::cerr << "CTRL: Relay fallback failed: no shared key" << std::endl;
                            }
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



