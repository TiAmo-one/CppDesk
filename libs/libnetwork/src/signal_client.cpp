#include "libnetwork/signal_client.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#include <ws2tcpip.h>
#include <random>
#include <sstream>
#include <cstring>

namespace network {

static std::string Base64Encode(const uint8_t* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out += tbl[(n >> 18) & 0x3F];
        out += tbl[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? tbl[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? tbl[n & 0x3F] : '=';
    }
    return out;
}

static std::string GenerateKey() {
    std::random_device rd;
    uint8_t key[16];
    for (int i = 0; i < 16; i++) key[i] = (uint8_t)(rd() & 0xFF);
    return Base64Encode(key, 16);
}

SignalClient::~SignalClient() { Disconnect(); }

bool SignalClient::Connect(const char* host, uint16_t port, const char* path) {
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock_, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(sock_); sock_ = INVALID_SOCKET;
        return false;
    }

    if (!Handshake(host, path)) {
        closesocket(sock_); sock_ = INVALID_SOCKET;
        return false;
    }

    connected_ = true;
    if (cb_.onConnected) cb_.onConnected();
    return true;
}

bool SignalClient::Handshake(const char* host, const char* path) {
    std::string key = GenerateKey();
    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Key: " << key << "\r\n"
        << "Sec-WebSocket-Version: 13\r\n\r\n";

    std::string reqStr = req.str();
    int sent = send(sock_, reqStr.c_str(), (int)reqStr.size(), 0);
    if (sent != (int)reqStr.size()) return false;

    char buf[4096] = {};
    int n = recv(sock_, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    std::string response(buf, n);

    return response.find("101") != std::string::npos &&
           response.find("Upgrade: websocket") != std::string::npos;
}

std::string SignalClient::ReadFrame() {
    uint8_t hdr[2];
    fd_set fds; FD_ZERO(&fds); FD_SET(sock_, &fds);
    timeval tv = {0, 0};
    if (select(0, &fds, nullptr, nullptr, &tv) <= 0) return "";

    int n = recv(sock_, (char*)hdr, 2, 0);
    if (n < 2) return "";

    bool fin = (hdr[0] & 0x80) != 0;
    if (!fin) return "";

    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t payloadLen = hdr[1] & 0x7F;

    if (payloadLen == 126) {
        uint8_t ext[2]; recv(sock_, (char*)ext, 2, 0);
        payloadLen = (ext[0] << 8) | ext[1];
    } else if (payloadLen == 127) {
        uint8_t ext[8]; recv(sock_, (char*)ext, 8, 0);
        payloadLen = 0;
        for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | ext[i];
    }

    uint8_t mask[4] = {};
    if (masked) recv(sock_, (char*)mask, 4, 0);

    std::string payload;
    if (payloadLen > 0 && payloadLen < 1024 * 1024) {
        payload.resize((size_t)payloadLen);
        int total = 0;
        while (total < (int)payloadLen) {
            int r = recv(sock_, &payload[total], (int)payloadLen - total, 0);
            if (r <= 0) return "";
            total += r;
        }
        if (masked)
            for (size_t i = 0; i < payload.size(); i++)
                payload[i] ^= mask[i % 4];
    }
    return payload;
}

void SignalClient::Poll(int timeoutMs) {
    if (!connected_) return;
    timeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };

    while (true) {
        fd_set fds; FD_ZERO(&fds); FD_SET(sock_, &fds);
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) break;
        std::string msg = ReadFrame();
        if (msg.empty()) break;
        if (cb_.onMessage) cb_.onMessage("", msg);
        // Check for relay_data
        try {
            json j = json::parse(msg);
            if (j.value("type", "") == "relay_data" && cb_.onRelayData) {
                cb_.onRelayData(j.value("data", ""));
            }
        } catch (...) {}
        tv = {0, 0};
    }
}

bool SignalClient::SendRelayData(const std::string& base64Data) {
    json j;
    j["type"] = "relay_data";
    j["data"] = base64Data;
    return Send(j.dump());
}

void SignalClient::Disconnect() {
    if (sock_ != INVALID_SOCKET) {
        // Send close frame
        uint8_t closeFrame[] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00};
        send(sock_, (const char*)closeFrame, sizeof(closeFrame), 0);
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    connected_ = false;
}

bool SignalClient::Send(const std::string& json) {
    if (!connected_) return false;
    std::string frame;
    frame += (char)0x81; // FIN + text

    if (json.size() < 126) {
        frame += (char)(json.size() | 0x80);
    } else if (json.size() < 65536) {
        frame += (char)(126 | 0x80);
        frame += (char)(json.size() >> 8);
        frame += (char)(json.size() & 0xFF);
    }

    std::random_device rd;
    uint8_t mask[4];
    for (int i = 0; i < 4; i++) mask[i] = (uint8_t)(rd() & 0xFF);
    frame.append((char*)mask, 4);

    for (size_t i = 0; i < json.size(); i++)
        frame += (char)(json[i] ^ mask[i % 4]);

    return send(sock_, frame.c_str(), (int)frame.size(), 0) == (int)frame.size();
}

} // namespace network

