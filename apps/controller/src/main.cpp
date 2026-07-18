#include <cstdio>
#include "controller.h"
#include <string>
#include <vector>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int) {
    FILE* fp;
    freopen_s(&fp, "C:\\Users\\17410\\Desktop\\remote control\\controller.log", "w", stdout);
    freopen_s(&fp, "C:\\Users\\17410\\Desktop\\remote control\\controller.log", "a", stderr);

    std::string host = "127.0.0.1";
    std::string port = "8443";
    std::string room = "my-pc";
    std::string pass = "abc123";

    std::string cmdLine(lpCmdLine);
    std::vector<std::string> args;
    size_t pos = 0;
    while (pos < cmdLine.size()) {
        while (pos < cmdLine.size() && cmdLine[pos] == ' ') pos++;
        if (pos >= cmdLine.size()) break;
        if (cmdLine[pos] == '"') {
            pos++;
            auto end = cmdLine.find('"', pos);
            if (end != std::string::npos) { args.push_back(cmdLine.substr(pos, end - pos)); pos = end + 1; }
        } else {
            auto end = cmdLine.find(' ', pos);
            if (end == std::string::npos) { args.push_back(cmdLine.substr(pos)); break; }
            args.push_back(cmdLine.substr(pos, end - pos));
            pos = end;
        }
    }

    if (args.size() >= 1) host = args[0];
    if (args.size() >= 2) port = args[1];
    if (args.size() >= 3) room = args[2];
    if (args.size() >= 4) pass = args[3];

    Controller ctrl(hInst, host, (uint16_t)std::stoi(port), room, pass);
    return ctrl.Run();
}
