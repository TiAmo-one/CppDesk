#include "server.h"
#include <csignal>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <iostream>

int main(int argc, char* argv[]) {
    FILE* fp = _fsopen("C:\\Users\\17410\\Desktop\\remote control\\signal-server.log", "w", _SH_DENYNO);
    if (fp) {
        _dup2(_fileno(fp), _fileno(stderr));
        _dup2(_fileno(fp), _fileno(stdout));
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }

    uint16_t port = 8443;
    if (argc > 1) port = (uint16_t)std::stoi(argv[1]);
    SignalServer server(port);
    server.Run();
    return 0;
}
