#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "libcapture.h"
//#include "libencode.h"  # requires x264
#include "libnetwork.h"
#include "libinput.h"
#include "libclipboard.h"
#include "libfiletransfer.h"
#include "libproto.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Agent {
public:
    Agent(const std::string& serverHost, uint16_t serverPort,
          const std::string& roomId, const std::string& password);
    ~Agent();
    int Run();

private:
    void MainLoop();

    std::string serverHost_;
    uint16_t    serverPort_;
    std::string roomId_;
    std::string password_;

    network::WSAInit      wsa_;
    network::SignalClient signal_;
    network::PeerChannel  channel_;
    network::UdpSocket    p2pSocket_;
    sockaddr_in           peerAddr_ = {};

    capture::Capture      capture_;
    //encode::Encoder encoder_;  # requires x264

    clipboard::Monitor    clipboard_;
    filetransfer::FileReceiver fileReceiver_;
    filetransfer::FileSender   fileSender_;

    std::atomic<bool> p2pReady_{false};
    std::atomic<bool> running_{true};
    bool signalRegistered_ = false;
    std::string remoteSdpData_;
    network::SdpInfo remoteSdp_;
    uint32_t lastWidth_ = 0, lastHeight_ = 0;
    std::vector<uint8_t> cachedFrame_;
    uint32_t cachedWidth_ = 0, cachedHeight_ = 0;
    std::vector<uint8_t> scaledBuf_;   // pre-allocated scaled buffer
    std::vector<uint8_t> sendBuf_;     // pre-allocated send buffer
    std::vector<uint8_t> prevFrame_;   // for frame differencing
    uint32_t prevWidth_ = 0, prevHeight_ = 0;
};
