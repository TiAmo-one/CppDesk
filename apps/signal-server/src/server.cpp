#include "server.h"
#include <wincrypt.h>
#include <iostream>
#include <random>
#include <sstream>
#include <algorithm>

std::string SignalServer::HashPassword(const std::string& roomId, const std::string& pw) {
    return std::to_string(std::hash<std::string>{}(roomId + ":" + pw));
}

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

static std::string Sha1Hash(const std::string& input) {
    // Simple SHA1 placeholder using WinCrypt
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    CryptHashData(hHash, (BYTE*)input.c_str(), (DWORD)input.size(), 0);
    BYTE hash[20]; DWORD hashLen = 20;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return Base64Encode(hash, 20);
}

static std::string GenerateWebSocketAccept(const std::string& key) {
    return Sha1Hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
}

SignalServer::SignalServer(uint16_t port) : port_(port) {
    InitializeCriticalSection(&lock_);
}

SignalServer::~SignalServer() {
    Stop();
    DeleteCriticalSection(&lock_);
}

void SignalServer::Run() {
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);

    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(listenSock_, (sockaddr*)&addr, sizeof(addr));
    listen(listenSock_, SOMAXCONN);

    std::cout << "Signal server running on port " << port_ << std::endl;

    while (running_) {
        fd_set fds; FD_ZERO(&fds); FD_SET(listenSock_, &fds);
        SOCKET maxSock = listenSock_;
        EnterCriticalSection(&lock_);
        for (auto& [sock, client] : clients_) {
            FD_SET(sock, &fds);
            if (sock > maxSock) maxSock = sock;
        }
        LeaveCriticalSection(&lock_);

        timeval tv = {0, 100000}; // 100ms
        if (select((int)maxSock + 1, &fds, nullptr, nullptr, &tv) <= 0) continue;

        // Accept new connections
        if (FD_ISSET(listenSock_, &fds)) {
            SOCKET client = accept(listenSock_, nullptr, nullptr);
            if (client != INVALID_SOCKET) {
                EnterCriticalSection(&lock_);
                clients_[client] = std::make_unique<Client>();
                clients_[client]->sock = client;
                LeaveCriticalSection(&lock_);
            }
        }

        // Read from existing clients
        std::vector<SOCKET> toProcess;
        EnterCriticalSection(&lock_);
        for (auto& [sock, c] : clients_) {
            if (FD_ISSET(sock, &fds)) toProcess.push_back(sock);
        }
        LeaveCriticalSection(&lock_);

        for (SOCKET sock : toProcess) {
            char buf[65536];
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) { DisconnectClient(sock); continue; }
            buf[n] = 0;

            EnterCriticalSection(&lock_);
            auto it = clients_.find(sock);
            if (it == clients_.end()) { LeaveCriticalSection(&lock_); continue; }
            auto* client = it->second.get();

            if (!client->handshakeDone) {
                std::string req(buf, n);
                if (DoHandshake(sock, req)) {
                    client->handshakeDone = true;
                    std::cout << "Client handshake done" << std::endl;
                } else {
                    LeaveCriticalSection(&lock_);
                    DisconnectClient(sock);
                    continue;
                }
                LeaveCriticalSection(&lock_);
                continue;
            }

            // WebSocket frame parsing
            client->recvBuf += std::string(buf, n);
            while (client->recvBuf.size() >= 2) {
                uint8_t* raw = (uint8_t*)client->recvBuf.data();
                size_t pos = 0;
                bool fin = (raw[0] & 0x80) != 0;
                if (!fin) break;
                bool masked = (raw[1] & 0x80) != 0;
                uint64_t payloadLen = raw[1] & 0x7F;
                pos = 2;

                if (payloadLen == 126) {
                    if (client->recvBuf.size() < 4) break;
                    payloadLen = (raw[2] << 8) | raw[3];
                    pos = 4;
                } else if (payloadLen == 127) {
                    if (client->recvBuf.size() < 10) break;
                    payloadLen = 0;
                    for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | raw[2 + i];
                    pos = 10;
                }

                uint8_t mask[4] = {};
                if (masked) {
                    if (client->recvBuf.size() < pos + 4 + payloadLen) break;
                    memcpy(mask, raw + pos, 4);
                    pos += 4;
                } else {
                    if (client->recvBuf.size() < pos + payloadLen) break;
                }

                std::string payload((char*)raw + pos, (size_t)payloadLen);
                if (masked)
                    for (size_t i = 0; i < payload.size(); i++)
                        payload[i] ^= mask[i % 4];

                size_t totalFrameSize = pos + payloadLen;
                client->recvBuf.erase(0, totalFrameSize);

                HandleMessage(sock, payload);
            }
            LeaveCriticalSection(&lock_);
        }
    }

    closesocket(listenSock_);
    WSACleanup();
}

void SignalServer::Stop() {
    running_ = false;
}

bool SignalServer::DoHandshake(SOCKET sock, const std::string& request) {
    // Extract WebSocket key
    auto keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) return false;
    keyPos += 19;
    auto keyEnd = request.find("\r\n", keyPos);
    std::string key = request.substr(keyPos, keyEnd - keyPos);
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);

    std::string acceptKey = GenerateWebSocketAccept(key);
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";

    std::string resp = response.str();
    return send(sock, resp.c_str(), (int)resp.size(), 0) == (int)resp.size();
}

void SignalServer::HandleMessage(SOCKET sock, const std::string& msg) {
    try {
        json j = json::parse(msg);
        std::string type = j.value("type", "");

        auto it = clients_.find(sock);
        if (it == clients_.end()) return;
        auto* client = it->second.get();

        if (type == "register") {
            std::string roomId = j["room"], password = j["password"];
            auto& room = rooms_[roomId];
            if (!room) room = std::make_unique<Room>();
            room->id = roomId;
            room->passwordHash = HashPassword(roomId, password);
            room->agentSock = sock;
            client->role = "agent";
            client->roomId = roomId;
            SendJson(sock, {{"type", "registered"}});
            std::cout << "Agent registered: " << roomId << std::endl;

        } else if (type == "join") {
            std::string roomId = j["room"], password = j["password"];
            auto rit = rooms_.find(roomId);
            if (rit == rooms_.end()) {
                SendJson(sock, {{"type", "error"}, {"message", "room not found"}});
                return;
            }
            auto* room = rit->second.get();
            if (room->passwordHash != HashPassword(roomId, password)) {
                SendJson(sock, {{"type", "error"}, {"message", "wrong password"}});
                return;
            }
            if (room->controllerSock != INVALID_SOCKET) {
                SendJson(sock, {{"type", "error"}, {"message", "room full"}});
                return;
            }
            room->controllerSock = sock;
            client->role = "controller";
            client->roomId = roomId;
            SendJson(sock, {{"type", "joined"}});
            SendJson(sock, {{"type", "peer_connect"}});
            SendJson(room->agentSock, {{"type", "peer_connect"}});
            std::cout << "Controller joined: " << roomId << std::endl;

        } else if (type == "sdp" || type == "ice" ||
                   type == "p2p_established" || type == "p2p_failed" || type == "relay_data") {
            // Forward to peer
            SOCKET peer = GetPeer(sock);
            if (peer != INVALID_SOCKET) SendJson(peer, j);
        }
    } catch (...) {}
}

void SignalServer::SendJson(SOCKET sock, const json& j) {
    std::string data = j.dump();
    std::string frame;
    frame += (char)0x81; // FIN + text
    if (data.size() < 126) {
        frame += (char)data.size();
    } else if (data.size() < 65536) {
        frame += (char)126;
        frame += (char)(data.size() >> 8);
        frame += (char)(data.size() & 0xFF);
    }
    frame += data;
    send(sock, frame.c_str(), (int)frame.size(), 0);
}

void SignalServer::DisconnectClient(SOCKET sock) {
    EnterCriticalSection(&lock_);
    auto it = clients_.find(sock);
    if (it != clients_.end()) {
        SOCKET peer = GetPeer(sock);
        if (peer != INVALID_SOCKET) {
            SendJson(peer, {{"type", "peer_disconnect"}});
        }
        if (!it->second->roomId.empty()) {
            auto rit = rooms_.find(it->second->roomId);
            if (rit != rooms_.end()) {
                if (rit->second->agentSock == sock) rit->second->agentSock = INVALID_SOCKET;
                if (rit->second->controllerSock == sock) rit->second->controllerSock = INVALID_SOCKET;
                if (rit->second->agentSock == INVALID_SOCKET && rit->second->controllerSock == INVALID_SOCKET)
                    rooms_.erase(rit);
            }
        }
        closesocket(sock);
        clients_.erase(it);
    }
    LeaveCriticalSection(&lock_);
}

SOCKET SignalServer::GetPeer(SOCKET sock) {
    auto it = clients_.find(sock);
    if (it == clients_.end()) return INVALID_SOCKET;
    auto* c = it->second.get();
    auto rit = rooms_.find(c->roomId);
    if (rit == rooms_.end()) return INVALID_SOCKET;
    if (rit->second->agentSock == sock) return rit->second->controllerSock;
    return rit->second->agentSock;
}
