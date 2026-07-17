#pragma once
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace network {

// RAII WSA startup/shutdown
class WSAInit {
public:
    WSAInit();
    ~WSAInit();
    bool ok() const { return ok_; }
private:
    bool ok_ = false;
};

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    bool Create(uint16_t port = 0);
    void Close();
    bool IsValid() const { return sock_ != INVALID_SOCKET; }
    SOCKET GetNative() const { return sock_; }

    bool SendTo(const sockaddr_in& addr, const uint8_t* data, int len);
    int  RecvFrom(sockaddr_in& from, uint8_t* buf, int bufLen, int timeoutMs);

    uint16_t GetPort() const;

private:
    SOCKET sock_ = INVALID_SOCKET;
};

} // namespace network
