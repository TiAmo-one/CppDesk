#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "libnetwork.h"
#include "libui.h"
#include "libclipboard.h"
#include "libfiletransfer.h"
#include <chrono>
#include "libproto.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Controller {
public:
    Controller(HINSTANCE hInst, const std::string& serverHost,
               uint16_t serverPort, const std::string& roomId,
               const std::string& password);
    ~Controller();
    int Run();

private:
    void NetworkThread();

    HINSTANCE   hInst_;
    std::string serverHost_;
    uint16_t    serverPort_;
    std::string roomId_;
    std::string password_;

    network::WSAInit      wsa_;
    network::SignalClient signal_;
    network::PeerChannel  channel_;
    network::RelayChannel relayChannel_;
    network::UdpSocket    p2pSocket_;
    sockaddr_in           peerAddr_ = {};

    // ECDH key exchange
    uint8_t myPublicKey_[32] = {};
    uint8_t mySecretKey_[32] = {};
    uint8_t sharedKey_[16] = {};
    bool    hasSharedKey_ = false;
    bool    relayMode_    = false;

    // Keep-alive
    std::chrono::steady_clock::time_point lastKeepAlive_;

    ui::RenderWindow      window_;
    std::atomic<bool> p2pReady_{false};
    std::atomic<bool> running_{true};
    std::thread networkThread_;

    int decWidth_ = 0, decHeight_ = 0;

    // Backing buffer for dirty rect accumulation
    std::vector<uint8_t> reasmBuf_;
    bool hasNewFrame_ = false;

    // Fragment reassembly for current dirty rect
    std::vector<uint8_t> tmpBuf_;
    uint32_t reasmWritten_ = 0;
    uint32_t reasmExpected_ = 0;
    bool     reasmActive_ = false;
    int16_t  reasmLeft_ = 0, reasmTop_ = 0, reasmRight_ = 0, reasmBottom_ = 0;

    // Raw BGRA fallback
    std::vector<uint8_t> rawReasmBuf_;
    uint32_t rawReasmWritten_ = 0;
    uint32_t rawReasmExpected_ = 0;
    bool     rawReasmActive_ = false;

    // FFmpeg decoder stubs (not used currently)
    void* codecCtx_ = nullptr;
    void* swsCtx_   = nullptr;
    void* decFrame_ = nullptr;

    filetransfer::FileSender   fileSender_;
    filetransfer::FileReceiver fileReceiver_;
    clipboard::Monitor         clipboard_;
};
