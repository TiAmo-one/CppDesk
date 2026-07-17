#pragma once
#include <string>
#include <thread>
#include <atomic>
#include "libcapture.h"
#include "libencode.h"
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
    encode::Encoder       encoder_;

    clipboard::Monitor    clipboard_;
    filetransfer::FileReceiver fileReceiver_;
    filetransfer::FileSender   fileSender_;

    std::atomic<bool> p2pReady_{false};
    std::atomic<bool> running_{true};
    bool signalRegistered_ = false;
    std::string remoteSdpData_;
    network::SdpInfo remoteSdp_;
};
