#pragma once
#include <cstdint>
#include <winsock2.h>

namespace network::stun {

bool GetMappedAddress(SOCKET sock, const char* stunServer, uint16_t stunPort,
                       sockaddr_in& mappedAddr);
bool GetMappedAddress(SOCKET sock, sockaddr_in& mappedAddr);

} // namespace network::stun
