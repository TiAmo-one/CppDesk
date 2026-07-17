#pragma once
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <memory>

using json = nlohmann::json;
using Server = websocketpp::server<websocketpp::config::asio>;
using ConnHdl = websocketpp::connection_hdl;

struct Room {
    std::string id;
    std::string passwordHash;
    ConnHdl agent;
    ConnHdl controller;
    bool hasAgent = false;
    bool hasController = false;
};

class SignalServer {
public:
    SignalServer(uint16_t port);
    void Run();

private:
    Server server_;
    std::unordered_map<std::string, std::unique_ptr<Room>> rooms_;
    std::unordered_map<ConnHdl, Room*, std::owner_less<ConnHdl>> connRoomMap_;

    void OnOpen(ConnHdl hdl);
    void OnClose(ConnHdl hdl);
    void OnMessage(ConnHdl hdl, Server::message_ptr msg);
    void Send(ConnHdl hdl, const json& msg);
    void ForwardToPeer(ConnHdl from, const json& msg);
    ConnHdl GetPeer(ConnHdl hdl);
    static std::string HashPassword(const std::string& roomId, const std::string& pw);
};
