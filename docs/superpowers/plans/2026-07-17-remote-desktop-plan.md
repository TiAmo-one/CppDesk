# Remote Desktop for Windows — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建一个 C++/Win32 远程桌面工具，实现纯公网 P2P 环境下的屏幕画面传输、键鼠操控、剪贴板同步和文件拖拽传输。

**Architecture:** 采用模块化静态库架构，agent.exe（被控端/Windows 服务）和 controller.exe（主控端/桌面应用）通过 signal-server（信令服务器）建立 P2P UDP 直连。核心模块包括屏幕采集 (DXGI)、软件编码 (x264)、P2P 打洞 (STUN+UDP)、输入注入 (SendInput) 和 Direct2D 渲染。

**Tech Stack:** C++17, CMake + MSVC, Win32 API, x264, FFmpeg libavcodec, libsodium, nlohmann/json, websocketpp, Google Test, vcpkg

## Global Constraints
- 平台：仅 Windows 10+，MSVC 编译器
- 依赖管理：vcpkg（x264, ffmpeg, libsodium, nlohmann-json, websocketpp, gtest）
- 编码标准：C++17，不使用异常，错误通过返回值和 GetLastError() 传递
- 所有模块编译为静态库
- TDD：先写测试，验证失败，再实现，验证通过
- 构建系统：CMake 3.20+

---

## Task 1: 项目骨架 — CMake + vcpkg + 目录结构

**Files:** 创建 `CMakeLists.txt`, `vcpkg.json`, 各 libs/apps 的 CMakeLists.txt

- [ ] **Step 1:** 创建根 CMakeLists.txt 和 vcpkg.json
- [ ] **Step 2:** 创建所有模块目录和占位文件
- [ ] **Step 3:** CMake configure 验证通过: `cmake -B build -S .`

根 CMakeLists.txt:
```cmake
cmake_minimum_required(VERSION 3.20)
project(remote-desktop VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()
enable_testing()
add_subdirectory(libs/libproto)
add_subdirectory(libs/libcapture)
add_subdirectory(libs/libencode)
add_subdirectory(libs/libnetwork)
add_subdirectory(libs/libinput)
add_subdirectory(libs/libclipboard)
add_subdirectory(libs/libfiletransfer)
add_subdirectory(libs/libui)
add_subdirectory(apps/agent)
add_subdirectory(apps/controller)
add_subdirectory(apps/signal-server)
```

vcpkg.json:
```json
{ "name": "remote-desktop", "version": "0.1.0",
  "dependencies": ["x264","ffmpeg","libsodium","nlohmann-json","websocketpp","gtest"] }
```

各 libs/X/CMakeLists.txt:
```cmake
add_library(libNAME STATIC src/dummy.cpp)
target_include_directories(libNAME PUBLIC include)
if(BUILD_TESTING) add_subdirectory(tests) endif()
```
---

## Task 2: libproto — 帧协议编解码

**Interfaces Produced:**
```cpp
#pragma once
#include <cstdint>
#include <vector>
namespace proto {
constexpr uint32_t MAGIC = 0x524F4B52;
enum class FrameType : uint8_t {
    Video=0x01, MouseMove=0x02, MouseBtn=0x03, KeyEvent=0x04,
    ClipboardText=0x05, ClipboardFile=0x06, FileBlock=0x07, Heartbeat=0x08, Resolution=0x09
};
#pragma pack(push, 1)
struct FrameHeader { uint32_t magic; FrameType type; uint16_t length; uint64_t sequence; };
#pragma pack(pop)
constexpr size_t HEADER_SIZE = sizeof(FrameHeader);
struct VideoPayload { uint32_t fragmentIndex; };
struct MouseMovePayload { float x; float y; };
struct MouseBtnPayload { uint8_t button; uint8_t down; };
struct KeyEventPayload { uint16_t vkCode; uint8_t down; };
struct ResolutionPayload { uint32_t width; uint32_t height; };
std::vector<uint8_t> Encode(FrameType type, uint64_t seq, const void* payload, uint16_t len);
bool Decode(const uint8_t* data, size_t len, FrameHeader& hdr, const uint8_t*& payload);
}
```

- [ ] **Step 1:** 写测试 `libs/libproto/tests/test_frame.cpp` (EncodeDecode round-trip, bad magic rejection, truncated rejection, video fragment)
- [ ] **Step 2:** `cmake --build build --target test_libproto` 期望 FAIL
- [ ] **Step 3:** 实现 `libs/libproto/src/frame.cpp` — Encode: 写入 MAGIC+Type+Length+Seq 到 15 字节头，然后 memcpy payload。Decode: 校验 magic、长度，解析头信息
- [ ] **Step 4:** `cmake --build build --target test_libproto && cd build && ctest -R test_libproto --output-on-failure` 期望 PASS

---

## Task 3: libcapture — DXGI 屏幕采集

**Interfaces Produced:**
```cpp
namespace capture {
struct Frame { uint8_t* data; uint32_t width, height, stride; uint64_t timestamp; };
class Capture {
public:
    bool Init(int monitorIndex=0);
    FrameGuard AcquireFrame(int timeoutMs=100);
    void ReleaseFrame();
    bool GetCurrentResolution(uint32_t& w, uint32_t& h);
};
}
```
- 使用 D3D11CreateDevice → IDXGIOutput1::DuplicateOutput
- AcquireNextFrame → CopyResource 到 staging texture → Map 读回 BGRA
- FrameGuard RAII 包装，析构时 ReleaseFrame

- [ ] **Step 1:** 写测试 `libs/libcapture/tests/test_capture.cpp` (Init succeeds, AcquireFrame returns valid data, resolution query)
- [ ] **Step 2:** 实现 `libs/libcapture/src/capture.cpp`
- [ ] **Step 3:** 构建测试并运行，期望 PASS

---

## Task 4: libinput — SendInput 键盘鼠标注入

**Interfaces Produced:**
```cpp
namespace input {
enum class MouseButton : uint8_t { Left=0, Right=1, Middle=2 };
void MoveMouse(float normalizedX, float normalizedY);
void MouseButtonEvent(MouseButton btn, bool down);
void MouseWheel(int delta);
void KeyEvent(uint16_t vkCode, bool down);
void CharInput(wchar_t ch);
}
```
- MoveMouse: 归一化坐标 (0~1) → GetSystemMetrics 获取虚拟桌面尺寸 → MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE
- KeyEvent: SendInput with KEYBDINPUT, 使用 wVk
- CharInput: 使用 KEYEVENTF_UNICODE

- [ ] **Step 1:** 写测试 `libs/libinput/tests/test_input.cpp` (验证 API 不崩溃即可，因为它们模拟真实输入)
- [ ] **Step 2:** 实现 `libs/libinput/src/input.cpp`
- [ ] **Step 3:** 构建测试并运行，期望 PASS
---

## Task 5: libnetwork Part 1 — UDP Socket + Frame 收发

**Interfaces Produced:**
```cpp
namespace network {
class WSAInit { public: WSAInit(); ~WSAInit(); bool ok() const; };
class UdpSocket {
public:
    bool Create(uint16_t port=0);
    void Close();
    SOCKET GetNative() const;
    bool SendTo(const sockaddr_in& addr, const uint8_t* data, int len);
    int RecvFrom(sockaddr_in& from, uint8_t* buf, int bufLen, int timeoutMs);
    uint16_t GetPort() const;
};
class PeerChannel {
public:
    using FrameCallback = std::function<void(proto::FrameType,uint64_t,const uint8_t*,uint16_t)>;
    bool Init(SOCKET sock, const sockaddr_in& peer);
    bool SendFrame(proto::FrameType type, const void* payload, uint16_t len);
    int Poll(int timeoutMs, FrameCallback cb);
    uint64_t NextSeq();
private: SOCKET sock_; sockaddr_in peer_; uint64_t seq_=0, remoteSeq_=0; uint8_t aesKey_[16]; bool hasKey_=false;
};
}
```
- UdpSocket: socket() + bind + ioctlsocket(FIONBIO), select() for recv timeout
- PeerChannel: 封装 Encode/Decode + sendto/recvfrom, 序列号防重放

- [ ] **Step 1:** 写测试 `libs/libnetwork/tests/test_udp.cpp` (localhost send/recv, PeerChannel frame round-trip)
- [ ] **Step 2:** 实现 `udp_socket.cpp` + `peer_channel.cpp`
- [ ] **Step 3:** 构建测试并运行，期望 PASS

---

## Task 6: libnetwork Part 2 — STUN 客户端

**Interface:** `network::stun::GetMappedAddress(SOCKET sock, const char* server, uint16_t port, sockaddr_in& mapped)` — 向 STUN 服务器发送 Binding Request (RFC 5389)，解析 XOR-MAPPED-ADDRESS 返回公网 IP:Port

- [ ] **Step 1:** 实现 `libs/libnetwork/src/stun.cpp` — 构建 20 字节 STUN 头 (0x0001 Binding Request + magic cookie 0x2112A442 + 随机 transaction ID)，解析响应中的 XOR-MAPPED-ADDRESS (0x0020) 或 MAPPED-ADDRESS (0x0001)
- [ ] **Step 2:** 写测试 `test_stun.cpp` (连接 Google STUN `stun.l.google.com:19302`，网络不可达时 skip)
- [ ] **Step 3:** 构建测试并运行

---

## Task 7: libnetwork Part 3 — WebSocket 信令客户端

**Interface:**
```cpp
class SignalClient {
public:
    bool Connect(const char* host, uint16_t port, const char* path="/");
    void Disconnect();
    bool IsConnected() const;
    bool Send(const std::string& json);
    void Poll(int timeoutMs=0);
    void SetCallbacks(SignalCallbacks cb);
};
```
- 手动实现 RFC 6455: HTTP Upgrade 握手 (Sec-WebSocket-Key + base64) → WebSocket 帧编解码 (FIN+opcode+mask+payload)
- 仅支持文本帧 (opcode 0x1), client→server 必须 mask

- [ ] **Step 1:** 实现 `signal_client.cpp` — TCP connect → HTTP 101 握手 → 帧读取 (处理 126/127 扩展长度) → 帧发送 (mask)
- [ ] **Step 2:** 构建 `libnetwork`，编译通过 (无 signal-server 时跳过集成测)

---

## Task 8: libnetwork Part 4 — Hole Punch + ECDH 密钥交换

**Interfaces:**
```cpp
struct SdpInfo { std::vector<std::string> candidates; };
SdpInfo BuildSdp(SOCKET sock, const sockaddr_in& stunAddr);
bool HolePunch(UdpSocket& sock, const SdpInfo& remoteSdp, sockaddr_in& outPeer);
bool KeyExchange(UdpSocket& sock, const sockaddr_in& peer, uint8_t outKey[16]);
```
- HolePunch: 逐候选地址发 UDP punch 包 (10 轮, 50ms 间隔), 收到对方 punch 包即打通
- KeyExchange: libsodium crypto_box_keypair 生成 Curve25519 密钥对 → 交换公钥 → crypto_box_beforenm 派生共享密钥 → 取前 16 字节为 AES-128 key

- [ ] **Step 1:** 实现 `hole_punch.cpp`
- [ ] **Step 2:** 构建，编译通过
---

## Task 9: libencode — x264 视频编码

**Interfaces:**
```cpp
namespace encode {
struct EncodedPacket { uint8_t* data; int size; bool isKeyFrame; int64_t pts; };
class Encoder {
public:
    bool Init(int width, int height, int fps=30, int bitrateKbps=2000);
    EncodedPacket Encode(const uint8_t* bgra, int stride, int64_t pts);
    void RequestKeyFrame();
    void SetBitrate(int bitrateKbps);
};
}
```
- x264_param_default_preset("ultrafast", "zerolatency") → bframes=0, sync_lookahead=0, repeat_headers=1, annexb=1
- BGRA→I420 转换: BT.601 色彩空间，逐像素转换
- Encode: x264_encoder_encode 返回 NAL 单元列表，拼接为连续字节流

- [ ] **Step 1:** 写测试 `test_encoder.cpp` (Init 成功, Encode 产生输出)
- [ ] **Step 2:** 实现 `encoder.cpp`
- [ ] **Step 3:** 构建测试并运行，期望 PASS

---

## Task 10: libui — Direct2D 渲染窗口

**Interfaces:**
```cpp
namespace ui {
struct UiCallbacks {
    std::function<void(float,float)> onMouseMove;
    std::function<void(int,bool)> onMouseButton;
    std::function<void(int)> onMouseWheel;
    std::function<void(uint16_t,bool)> onKey;
    std::function<void(const wchar_t*)> onFileDrop;
    std::function<void(int,int)> onResize;
    std::function<void()> onClose;
};
class RenderWindow {
public:
    bool Create(HINSTANCE, const wchar_t* title, int w, int h);
    void Show(int cmd=SW_SHOW);
    void UpdateFrame(const uint8_t* bgra, int w, int h, int stride);
    int Run();
    void SetCallbacks(UiCallbacks cb);
private: HWND hwnd_; ID2D1Factory*, ID2D1HwndRenderTarget*, ID2D1Bitmap*; CRITICAL_SECTION;
};
}
```
- 注册窗口类 "RemoteDesktopWindow"，CreateWindow → D2D1CreateFactory → CreateHwndRenderTarget
- UpdateFrame: Lock → CreateBitmap/CopyFromMemory → InvalidateRect → Unlock
- OnPaint: EnterCS → BeginDraw → DrawBitmap (等比缩放居中) → EndDraw → LeaveCS
- 输入: WM_MOUSEMOVE/WM_LBUTTONDOWN/WM_KEYDOWN/WM_DROPFILES → callback

- [ ] **Step 1:** 实现 `render_window.cpp`
- [ ] **Step 2:** 构建 `libui`，编译通过

---

## Task 11: libclipboard — 剪贴板监控与同步

**Interfaces:**
```cpp
namespace clipboard {
struct ClipboardData { std::wstring text; std::vector<std::wstring> filePaths; bool hasText,hasFiles; };
class Monitor {
public:
    bool Check(ClipboardData& data);
    static bool Write(const ClipboardData& data);
    void SetIgnoreNext();
};
}
```
- Check: GetClipboardSequenceNumber() 对比 → OpenClipboard → CF_UNICODETEXT + CF_HDROP
- Write: OpenClipboard+EmptyClipboard → SetClipboardData → CloseClipboard
- SetIgnoreNext: 防止远程写入触发本地检测回环

- [ ] **Step 1:** 实现 `clipboard.cpp`
- [ ] **Step 2:** 构建 `libclipboard`

---

## Task 12: libfiletransfer — 文件分块传输

**Interfaces:**
```cpp
namespace filetransfer {
constexpr size_t BLOCK_SIZE = 60*1024; constexpr int WINDOW_SIZE = 32;
struct FileMeta { uint32_t transferId; std::wstring fileName; uint64_t fileSize; uint32_t totalBlocks; };
class FileSender {
    uint32_t StartSend(const std::wstring& path);
    std::vector<std::vector<uint8_t>> GetNextBlocks(int max);
    void OnAck(uint32_t tid, uint32_t idx);
    bool IsDone(uint32_t tid) const;
};
class FileReceiver {
    void StartReceive(const FileMeta& meta, const std::wstring& savePath);
    void OnBlock(uint32_t tid, uint32_t idx, const uint8_t* data, uint32_t len);
    std::vector<std::pair<uint32_t,uint32_t>> GetPendingAcks();
    bool IsDone(uint32_t tid) const;
};
}
```
- 分块 60KB，滑动窗口 32，选择性 ACK
- 发送: StartSend → 逐块读文件 → GetNextBlocks 返回窗口内未确认块
- 接收: StartReceive 创建文件 → OnBlock 写入并记录 ACK

- [ ] **Step 1:** 实现 `file_transfer.cpp`
- [ ] **Step 2:** 构建 `libfiletransfer`

---

## Task 13: signal-server — 信令服务器

**Files:** `apps/signal-server/src/main.cpp`, `server.h`, `server.cpp`
**Depends on:** websocketpp, nlohmann/json
**Protocol:** WebSocket JSON

```
register: {"type":"register","room":"xxx","password":"xxx"} → {"type":"registered"}
join: {"type":"join","room":"xxx","password":"xxx"} → {"type":"joined"} + peer_connect
sdp/ice: transparent forward to peer
p2p_established/p2p_failed: forward to peer
```

- [ ] **Step 1:** 实现 `server.h` — Room {id, passwordHash, agent conn, controller conn}, SignalServer {OnOpen, OnClose, OnMessage, ForwardToPeer}
- [ ] **Step 2:** 实现 `server.cpp` — register 创建房间+aesKey, join 验证密码+配对, sdp/ice 透明转发
- [ ] **Step 3:** 实现 `main.cpp` — 解析端口参数，启动 ASIO 事件循环

**CMakeLists:**
```cmake
find_package(websocketpp REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(signal-server src/main.cpp src/server.cpp)
target_link_libraries(signal-server PRIVATE websocketpp::websocketpp nlohmann_json::nlohmann_json)
```

- [ ] **Step 4:** `cmake --build build --target signal-server`
---

## Task 14: agent.exe — 被控端集成

**Files:** `apps/agent/src/main.cpp`, `agent.h`, `agent.cpp`
**Links:** libcapture, libencode, libnetwork, libinput, libclipboard, libfiletransfer, libproto, libsodium

**Main loop:**
1. 连接 signal-server → 注册房间 → 等待 controller
2. 创建 UDP socket → STUN 获取公网地址 → 交换 SDP/ICE → 打洞 → ECDH 密钥交换
3. 初始化 Capture(0) + Encoder(w,h,30,2000)
4. 每 16ms: AcquireFrame → Encode → 打包 VideoPayload → SendFrame(Video)
5. Poll PeerChannel: MouseMove/MouseBtn/KeyEvent → SendInput 执行
6. 每 500ms 检查剪贴板变化 → 发送 ClipboardText
7. 接收 FileBlock → FileReceiver.OnBlock

- [ ] **Step 1:** 实现 `agent.h` — 聚合所有模块，定义线程管理
- [ ] **Step 2:** 实现 `agent.cpp` — Run() 主循环：signal connect → P2P setup → capture+encode loop + input/clipboard/file polling
- [ ] **Step 3:** 实现 `main.cpp` — 解析 CLI 参数 `<host> <port> <room> <password>`

**CMakeLists:**
```cmake
add_executable(agent src/main.cpp src/agent.cpp)
target_link_libraries(agent PRIVATE libcapture libencode libnetwork libinput libclipboard libfiletransfer libproto libsodium::libsodium)
```

- [ ] **Step 4:** `cmake --build build --target agent`

---

## Task 15: controller.exe — 主控端集成

**Files:** `apps/controller/src/main.cpp`, `controller.h`, `controller.cpp`
**Links:** libui, libnetwork, libclipboard, libfiletransfer, libproto, FFmpeg (avcodec+avutil+swscale), libsodium

**Main loop:**
1. 连接 signal-server → 加入房间 → P2P 打洞 → ECDH 密钥交换
2. 创建 RenderWindow (1280×720)
3. 设置 UiCallbacks: onMouseMove/onMouseButton/onKey → SendFrame(MouseMove/MouseBtn/KeyEvent)
4. NetworkThread: Poll PeerChannel → 收 Video frame → FFmpeg avcodec_send_packet/avcodec_receive_frame → sws_scale YUV→BGRA → window_.UpdateFrame
5. onFileDrop → FileSender.StartSend → 逐块传输
6. 收到 ClipboardText → clipboard::Monitor::Write 写入本地剪贴板

**FFmpeg decoder wrapper:**
- avcodec_find_decoder(AV_CODEC_ID_H264) → avcodec_alloc_context3 → avcodec_open2
- 解码循环: avcodec_send_packet → avcodec_receive_frame
- sws_getContext(YUV420P→BGRA) → sws_scale

- [ ] **Step 1:** 实现 `controller.h` — 聚合 UI + network + decoder
- [ ] **Step 2:** 实现 `controller.cpp` — InitDecoder (初始化 FFmpeg), DecodeFrame (H.264→BGRA), Run() 主流程
- [ ] **Step 3:** 实现 `main.cpp` — WinMain 入口

**CMakeLists:**
```cmake
find_package(FFMPEG REQUIRED COMPONENTS avcodec avutil swscale)
add_executable(controller src/main.cpp src/controller.cpp)
target_link_libraries(controller PRIVATE libui libnetwork libclipboard libfiletransfer libproto FFMPEG::avcodec FFMPEG::avutil FFMPEG::swscale libsodium::libsodium)
```

- [ ] **Step 4:** `cmake --build build --target controller`

---

## Task 16: 端到端集成构建与冒烟测试

- [ ] **Step 1:** `cmake --build build --config Release` 全量构建，期望三个 exe 全成功
- [ ] **Step 2:** `cd build && ctest --output-on-failure` 所有单元测试通过
- [ ] **Step 3:** 启动 signal-server: `./build/apps/signal-server/Release/signal-server.exe 8443`
- [ ] **Step 4:** 启动 agent: `./build/apps/agent/Release/agent.exe 127.0.0.1 8443 test-room password123` → 期望 "Agent running"
- [ ] **Step 5:** 启动 controller → 期望窗口打开，显示远程桌面画面，键鼠操作生效

---

## 实施顺序与依赖图

```
Task  1 (CMake骨架)
       │
Task  2 (libproto)
       ├──► Task  3 (libcapture) ──► Task  9 (libencode)
       ├──► Task  4 (libinput)
       ├──► Task  5 (UDP Socket) ──┬──► Task 7 (WebSocket)
       │                           ├──► Task 6 (STUN)
       │                           └──► Task 8 (HolePunch+ECDH)
       ├──► Task 10 (libui)
       ├──► Task 11 (libclipboard)
       ├──► Task 12 (libfiletransfer)
       └──► Task 13 (signal-server) [完全独立，可并行]

Task 3+4+5+6+7+8+9+11+12 ──► Task 14 (agent.exe)
Task 5+6+7+8+10+11+12 ──► Task 15 (controller.exe)
Task 14+15 ──► Task 16 (集成)
```

**并行机会:**
- Task 3, 4, 5 可同时开发
- Task 6, 7, 8 依赖 Task 5，但彼此独立
- Task 9, 10, 11, 12 可并行
- Task 13 (signal-server) 与所有其他任务并行
- Task 14 和 15 可并行开发
