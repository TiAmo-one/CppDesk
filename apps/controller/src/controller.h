#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "libnetwork.h"
#include "libui.h"
#include "libclipboard.h"
#include "libfiletransfer.h"
#include "libproto.h"
#include <nlohmann/json.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

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
    bool InitDecoder(int width, int height);
    std::vector<uint8_t> DecodeFrame(const uint8_t* data, int len);

    HINSTANCE   hInst_;
    std::string serverHost_;
    uint16_t    serverPort_;
    std::string roomId_;
    std::string password_;

    network::WSAInit      wsa_;
    network::SignalClient signal_;
    network::PeerChannel  channel_;
    network::UdpSocket    p2pSocket_;
    sockaddr_in           peerAddr_ = {};

    ui::RenderWindow      window_;
    std::atomic<bool> p2pReady_{false};
    std::atomic<bool> running_{true};
    std::thread networkThread_;

    // Decoder
    AVCodecContext* codecCtx_ = nullptr;
    SwsContext*     swsCtx_   = nullptr;
    AVFrame*        decFrame_ = nullptr;
    int decWidth_ = 0, decHeight_ = 0;

    // Frame reassembly
    std::vector<uint8_t> reasmBuf_;
    uint32_t reasmWritten_ = 0;
    uint32_t reasmExpected_ = 0;
    bool     reasmActive_ = false;
    bool     resolutionChanged_ = false;

    filetransfer::FileSender   fileSender_;
    filetransfer::FileReceiver fileReceiver_;
    clipboard::Monitor         clipboard_;
};
