# Remote Desktop for Windows

A C++/Win32 remote desktop tool for controlling Windows PCs over the internet via P2P UDP.

## Architecture

- **agent.exe** — Runs on the remote PC (Windows service), captures screen via DXGI, encodes with x264
- **controller.exe** — Runs on your local PC, renders remote screen via Direct2D, sends mouse/keyboard input
- **signal-server** — Lightweight WebSocket server for NAT traversal (SDP/ICE relay)

## Build

Requires: CMake 3.20+, MSVC, vcpkg

```powershell
vcpkg install x264 ffmpeg libsodium nlohmann-json websocketpp gtest
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Usage

1. Deploy `signal-server` on a cloud server:
   ```
   signal-server.exe 8443
   ```

2. On the remote PC:
   ```
   agent.exe <server_ip> 8443 my-room password123
   ```

3. On your local PC:
   Edit `apps/controller/src/main.cpp` to set server IP/port/room/password, then run `controller.exe`.

## Project Structure

```
libs/libproto/       — Binary frame protocol
libs/libcapture/     — DXGI desktop capture
libs/libencode/      — x264 H.264 encoder
libs/libnetwork/     — UDP socket, STUN, WebSocket client, hole punch, ECDH
libs/libinput/       — SendInput keyboard/mouse injection
libs/libclipboard/   — Clipboard monitoring and sync
libs/libfiletransfer/— Chunked file transfer with sliding window
libs/libui/          — Direct2D render window
apps/agent/          — Agent (controlled PC)
apps/controller/     — Controller (viewer PC)
apps/signal-server/  — Signaling server
```
