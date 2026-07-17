#include "server.h"
#include <csignal>

int main(int argc, char* argv[]) {
    uint16_t port = 8443;
    if (argc > 1) port = (uint16_t)std::stoi(argv[1]);
    SignalServer server(port);
    server.Run();
    return 0;
}
