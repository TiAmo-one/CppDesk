#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

class SignalServer {
public:
    SignalServer(uint16_t port);
    ~SignalServer();
    void Run();
    void Stop();

private:
    struct Client {
        SOCKET sock = INVALID_SOCKET;
        std::string role; // "agent" or "controller"
        std::string roomId;
        std::string recvBuf;
        bool handshakeDone = false;
    };

    struct Room {
        std::string id;
        std::string passwordHash;
        SOCKET agentSock = INVALID_SOCKET;
        SOCKET controllerSock = INVALID_SOCKET;
    };

    uint16_t port_;
    SOCKET listenSock_ = INVALID_SOCKET;
    std::atomic<bool> running_{true};
    std::thread acceptThread_;
    std::unordered_map<SOCKET, std::unique_ptr<Client>> clients_;
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    CRITICAL_SECTION lock_;

    void AcceptLoop();
    bool DoHandshake(SOCKET sock, const std::string& request);
    void HandleMessage(SOCKET sock, const std::string& msg);
    void SendJson(SOCKET sock, const json& j);
    void DisconnectClient(SOCKET sock);
    SOCKET GetPeer(SOCKET sock);
    static std::string ReadAll(SOCKET sock);
    static std::string HashPassword(const std::string& roomId, const std::string& pw);};
