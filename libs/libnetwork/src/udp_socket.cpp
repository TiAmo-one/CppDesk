#include "libnetwork/udp_socket.h"

namespace network {

WSAInit::WSAInit() {
    WSADATA wsaData;
    ok_ = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
}
WSAInit::~WSAInit() {
    if (ok_) WSACleanup();
}

UdpSocket::~UdpSocket() { Close(); }

bool UdpSocket::Create(uint16_t port) {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) return false;
    if (port > 0) {
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock_, (sockaddr*)&addr, sizeof(addr)) != 0) {
            Close();
            return false;
        }
    }
    u_long mode = 1;
    ioctlsocket(sock_, FIONBIO, &mode);
    return true;
}

void UdpSocket::Close() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

bool UdpSocket::SendTo(const sockaddr_in& addr, const uint8_t* data, int len) {
    return sendto(sock_, (const char*)data, len, 0,
                  (const sockaddr*)&addr, sizeof(addr)) == len;
}

int UdpSocket::RecvFrom(sockaddr_in& from, uint8_t* buf, int bufLen, int timeoutMs) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock_, &fds);
    timeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    int sel = select(0, &fds, nullptr, nullptr, &tv);
    if (sel <= 0) return sel;

    int fromLen = sizeof(from);
    return recvfrom(sock_, (char*)buf, bufLen, 0,
                    (sockaddr*)&from, &fromLen);
}

uint16_t UdpSocket::GetPort() const {
    sockaddr_in addr = {};
    int len = sizeof(addr);
    if (getsockname(sock_, (sockaddr*)&addr, &len) == 0) {
        return ntohs(addr.sin_port);
    }
    return 0;
}

} // namespace network
