#include "agent.h"
#include <iostream>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <share.h>

int main(int argc, char* argv[]) {
    // Open log with FILE_SHARE_READ so we can read it while agent runs
    FILE* fp = _fsopen("C:\\Users\\17410\\Desktop\\remote control\\agent.log", "w", _SH_DENYNO);
    if (fp) {
        _dup2(_fileno(fp), _fileno(stderr));
        _dup2(_fileno(fp), _fileno(stdout));
        setvbuf(stderr, NULL, _IONBF, 0);
        setvbuf(stdout, NULL, _IONBF, 0);
    }

    if (argc < 5) {
        std::cerr << "Usage: agent.exe <server_host> <server_port> <room_id> <password>" << std::endl;
        return 1;
    }

    Agent agent(argv[1], (uint16_t)std::stoi(argv[2]), argv[3], argv[4]);
    return agent.Run();
}
