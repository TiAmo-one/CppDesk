#include "server.h"
#include <iostream>
#include <functional>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

std::string SignalServer::HashPassword(const std::string& roomId, const std::string& pw) {
    size_t h = std::hash<std::string>{}(roomId + ":" + pw);
    return std::to_string(h);
}

SignalServer::SignalServer(uint16_t port) {
    server_.init_asio();
    server_.set_open_handler(std::bind(&SignalServer::OnOpen, this, _1));
    server_.set_close_handler(std::bind(&SignalServer::OnClose, this, _1));
    server_.set_message_handler(std::bind(&SignalServer::OnMessage, this, _1, _2));
    server_.listen(port);
}

void SignalServer::Run() {
    server_.start_accept();
    std::cout << "Signal server running on port " << 8443 << std::endl;
    server_.run();
}

void SignalServer::OnOpen(ConnHdl hdl) {
    std::cout << "Client connected" << std::endl;
}

void SignalServer::OnClose(ConnHdl hdl) {
    auto it = connRoomMap_.find(hdl);
    if (it == connRoomMap_.end()) return;
    Room* room = it->second;

    ConnHdl peer = GetPeer(hdl);
    if (peer.lock()) {
        try { Send(peer, {{"type", "peer_disconnect"}}); } catch (...) {}
        connRoomMap_.erase(peer);
    }
    connRoomMap_.erase(it);
    rooms_.erase(room->id);
    std::cout << "Room " << room->id << " cleaned" << std::endl;
}

void SignalServer::OnMessage(ConnHdl hdl, Server::message_ptr msg) {
    try {
        json j = json::parse(msg->get_payload());
        std::string type = j.value("type", "");

        if (type == "register") {
            std::string roomId   = j["room"];
            std::string password = j["password"];
            auto& room = rooms_[roomId];
            if (!room) room = std::make_unique<Room>();
            room->id           = roomId;
            room->passwordHash = HashPassword(roomId, password);
            room->agent        = hdl;
            room->hasAgent     = true;
            connRoomMap_[hdl]  = room.get();
            Send(hdl, {{"type", "registered"}});
            std::cout << "Agent registered: " << roomId << std::endl;

        } else if (type == "join") {
            std::string roomId   = j["room"];
            std::string password = j["password"];
            auto it = rooms_.find(roomId);
            if (it == rooms_.end()) {
                Send(hdl, {{"type", "error"}, {"message", "room not found"}});
                return;
            }
            Room* room = it->second.get();
            if (room->passwordHash != HashPassword(roomId, password)) {
                Send(hdl, {{"type", "error"}, {"message", "wrong password"}});
                return;
            }
            if (room->hasController) {
                Send(hdl, {{"type", "error"}, {"message", "room full"}});
                return;
            }
            room->controller    = hdl;
            room->hasController = true;
            connRoomMap_[hdl]   = room;
            Send(hdl, {{"type", "joined"}});
            Send(room->agent, {{"type", "peer_connect"}});
            std::cout << "Controller joined: " << roomId << std::endl;

        } else if (type == "sdp" || type == "ice" ||
                   type == "p2p_established" || type == "p2p_failed") {
            ForwardToPeer(hdl, j);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void SignalServer::Send(ConnHdl hdl, const json& msg) {
    try { server_.send(hdl, msg.dump(), websocketpp::frame::opcode::text); }
    catch (...) {}
}

void SignalServer::ForwardToPeer(ConnHdl from, const json& msg) {
    ConnHdl peer = GetPeer(from);
    if (peer.lock()) Send(peer, msg);
}

ConnHdl SignalServer::GetPeer(ConnHdl hdl) {
    auto it = connRoomMap_.find(hdl);
    if (it == connRoomMap_.end()) return ConnHdl();
    Room* room = it->second;
    if (room->agent.lock() == hdl.lock()) return room->controller;
    return room->agent;
}
