#pragma once
#include <string>
#include <functional>
#include <cstdint>
#include <winsock2.h>

namespace network {

struct SignalCallbacks {
    std::function<void()> onConnected;
    std::function<void(const std::string& type, const std::string& data)> onMessage;
    std::function<void(const std::string& error)> onError;
};

class SignalClient {
public:
    SignalClient() = default;
    ~SignalClient();

    bool Connect(const char* host, uint16_t port, const char* path = "/");
    void Disconnect();
    bool IsConnected() const { return connected_; }

    bool Send(const std::string& json);
    void Poll(int timeoutMs = 0);
    void SetCallbacks(SignalCallbacks cb) { cb_ = cb; }

private:
    SOCKET sock_ = INVALID_SOCKET;
    std::string recvBuf_;
    SignalCallbacks cb_;
    bool connected_ = false;

    bool Handshake(const char* host, const char* path);
    std::string ReadFrame();
};

} // namespace network
