#include "controller.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    Controller ctrl(hInst, "127.0.0.1", 8443, "my-pc", "abc123");
    return ctrl.Run();
}
