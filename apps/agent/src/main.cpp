#include "agent.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cout << "Usage: agent.exe <server_host> <server_port> <room_id> <password>" << std::endl;
        return 1;
    }

    Agent agent(argv[1], (uint16_t)std::stoi(argv[2]), argv[3], argv[4]);
    return agent.Run();
}
