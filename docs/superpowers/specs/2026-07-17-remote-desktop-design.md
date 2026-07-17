# Remote Desktop for Windows — 设计文档

## 概述

构建一个 C++/Win32 远程桌面工具，用于纯公网环境下两台 Windows 电脑之间的远程操控。MVP 聚焦核心体验：屏幕画面传输 + 键鼠操控 + 剪贴板同步 + 文件拖拽传输。

## 约束与范围

- 两端均为 Windows 原生桌面程序（C++/Win32）
- 纯公网 P2P 打洞，自建轻量信令服务器
- 软件编码（x264），不依赖 GPU 硬编码
- 个人效率工具定位，不做企业级功能

**MVP 包含：**
- 屏幕画面实时采集与传输
- 键盘/鼠标输入远程注入
- 剪贴板文本同步（双向）
- 文件拖拽传输（主控端拖文件到远程）

**MVP 不包含：**
- 音频传输、UAC 安全桌面、多显示器、打印机重定向、智能卡、移动端适配

---

## 整体架构

`
┌──────────────┐          ┌─────────────────┐          ┌──────────────┐
│  agent.exe   │          │  signal-server   │          │controller.exe│
│  (被控端)    │◄────────►│  (信令服务器)    │◄────────►│  (主控端)    │
│  Windows服务 │  TCP/WS  │  云服务器部署    │  TCP/WS  │  桌面应用    │
└──────┬───────┘          └─────────────────┘          └──────┬───────┘
       │                                                     │
       │              UDP P2P 直连 (打洞后)                   │
       │◄───────────────────────────────────────────────────►│
       │   视频流 / 输入指令 / 剪贴板 / 文件传输              │
       │                                                     │
`

### 连接流程

1. agent.exe（被控端）和 controller.exe（主控端）各自启动后连接 signal-server
2. agent 注册房间 ID + 密码，等待连接
3. controller 凭房间 ID + 密码连入，向 signal-server 请求与 agent 建立 P2P
4. signal-server 代为交换 SDP 和 ICE Candidate
5. 双方尝试 UDP 打洞，成功则直连，失败则本次连接终止
6. 打洞成功后，所有数据走直连 UDP 通道，不再经过服务器

---

## 模块设计

### 模块总览

| 模块 | 角色 | 编译产物 |
|------|------|----------|
| libproto | 二进制帧协议编解码 | 静态库 |
| libcapture | 屏幕采集 | 静态库 |
| libencode | x264 视频编码 | 静态库 |
| libnetwork | UDP 传输 + 打洞 + 信令 | 静态库 |
| libinput | 键鼠注入 | 静态库 |
| libclipboard | 剪贴板监听与同步 | 静态库 |
| libfiletransfer | 文件分块传输 | 静态库 |
| libui | Win32/Direct2D 渲染窗口 | 静态库 |
| agent.exe | 被控端 Windows 服务 | 可执行文件 |
| controller.exe | 主控端桌面应用 | 可执行文件 |
| signal-server | 信令服务器 | 可执行文件 |

---

### libproto — 二进制帧协议

一帧结构：

`
┌──────────────────────────────────────────────────┐
│  Magic (4B)  │  Type (1B)  │  Length (2B)  │  Payload (N bytes)  │
│  0x524F4B52  │             │  max 65535    │                     │
└──────────────────────────────────────────────────┘
`

帧类型：

| Type | 含义 | 方向 |
|------|------|------|
| 0x01 | 视频帧 (H.264 NAL) | agent → controller |
| 0x02 | 鼠标移动 | controller → agent |
| 0x03 | 鼠标按键 | controller → agent |
| 0x04 | 键盘事件 | controller → agent |
| 0x05 | 剪贴板文本 | 双向 |
| 0x06 | 剪贴板文件元数据 | 双向 |
| 0x07 | 文件传输块 | 双向 |
| 0x08 | 心跳 (Ping/Pong) | 双向 |
| 0x09 | 分辨率变更通知 | agent → controller |

视频帧：Payload 前 4 字节为分片序号（大帧分片传输）。

---

### libcapture — 屏幕采集

- API：DXGI Desktop Duplication API (Windows 8+)
- 输出：RGBA/BGRA 原始帧缓冲区
- 接口：
  - Init(int monitorIndex) — 初始化指定显示器
  - AcquireFrame() — 阻塞获取新帧
  - ReleaseFrame() — 归还缓冲区
  - Resize(int width, int height) — 分辨率变化时重设

---

### libencode — 视频编码

- 使用 x264，预设 ultrafast，zerolatency 模式
- 输出 H.264 NAL 单元
- 接口：
  - Init(int width, int height, int fps, int bitrate)
  - Encode(const CaptureFrame& frame) → EncodedPacket
  - SetBitrate(int bitrate) — 动态码率调整
  - RequestKeyFrame() — 请求 I 帧

---

### libnetwork — 网络传输

**信令通道**（WebSocket 客户端）：
- ConnectToSignal(serverAddr, port) — 连接信令服务器
- RegisterRoom(roomId, password) — agent 注册房间
- JoinRoom(roomId, password, outSdp) — controller 加入房间

**P2P 通道**（UDP）：
- EstablishP2P(remoteSdp) — 打洞建立直连
- SendFrame(ProtoFrame) / RecvFrame(timeoutMs) — 收发帧
- STUN 客户端：向 stun.l.google.com:19302 获取公网映射地址
- 加密：ECDH 密钥交换 + AES-128-GCM

---

### libinput — 键盘鼠标注入

- 底层：SendInput() API
- 坐标：归一化比例值 (0~1)，agent 端转为实际像素
- 接口：
  - MoveMouse(normalizedX, normalizedY)
  - MouseButton(button, down)
  - MouseWheel(delta)
  - KeyEvent(vkCode, down)
  - KeyCombo(vkCodes, count)

---

### libclipboard — 剪贴板同步

- agent 端每 500ms 检查 GetClipboardSequenceNumber() 变化
- 文本：GetClipboardData(CF_UNICODETEXT) → 编码发送
- 文件：读取文件路径列表 + 元数据，配合 libfiletransfer 传输
- controller 端收到后写入本地剪贴板
- 回环防护：来自远程的写入跳过本地检测

---

### libfiletransfer — 文件传输

- 分块大小：60KB（适配 UDP MTU）
- 流程：元数据帧 → 逐块发送 → 收齐 ACK
- 丢包重传：滑动窗口（窗口大小 32）+ 选择性 ACK
- controller 端集成 WM_DROPFILES 消息

---

## 线程模型

### agent.exe 线程

| 线程 | 职责 |
|------|------|
| CaptureThread | DXGI AcquireNextFrame 循环，帧间 Sleep(16ms)，推给编码线程 |
| EncodeThread | x264_encoder_encode → libproto 打包 → 推网络线程 |
| NetworkThread | 单 UDP socket select 循环：发视频 / 收输入并分发 |
| InputThread | 收到输入事件后调用 SendInput() 执行 |
| ClipboardThrd | 500ms 轮询剪贴板序列号，变化时读取推送 |
| FileTransfer | 接收文件块写磁盘 + SACK；发送时读磁盘分块 |

线程间通信：自制无锁环形缓冲区（SPSC FIFO）。

### controller.exe 线程

| 线程 | 职责 |
|------|------|
| UI Thread | Win32 消息循环：Direct2D 渲染视频 + 捕获输入（WM_INPUT / WM_DROPFILES） |
| NetworkThread | UDP 收发：视频解码（FFmpeg libavcodec）→ 推渲染缓冲；发送输入/文件帧 |

---

## 信令服务器

### 设计

- 独立 C++ 进程，单端口监听
- 职责：房间管理、SDP/ICE 透明转发
- 数据模型：
  - Room：roomId + passwordHash + agent Session + controller Session
  - Session：socket fd + 角色 + 状态

### 信令协议（WebSocket JSON）

`
// agent 注册
{ "type": "register", "room": "my-pc", "password": "abc123" }
// server 响应: { "type": "registered" }

// controller 加入
{ "type": "join", "room": "my-pc", "password": "abc123" }
// server 响应: { "type": "joined" }

// 通知对方开始打洞
{ "type": "peer_connect" }

// SDP 交换
{ "type": "sdp", "data": "v=0\r\no=- ..." }

// ICE 候选交换
{ "type": "ice", "candidate": "1.2.3.4:45678" }

// 打洞结果
{ "type": "p2p_established" }
{ "type": "error", "message": "hole punch failed" }
`

---

## 连接建立详细流程

1. 双方各自向 stun.l.google.com:19302 发送 STUN Binding Request，获取公网 IP:Port
2. 通过 signal-server 交换 SDP（本端信息）和 ICE Candidate（候选地址列表）
3. 同时打洞：双方 bind 本地端口，向对方所有候选地址连续发 UDP 打洞包（50ms 间隔 × 10 次）
4. 任一方收到对方打洞包 = 打通
5. 在 UDP 通道上 ECDH（Curve25519）交换 AES-128-GCM 密钥
6. 开始加密传输视频流和输入指令
7. 打洞阶段总超时 10 秒

---

## 安全设计

| 层级 | 机制 |
|------|------|
| 房间认证 | 房间 ID + SHA256(密码) |
| 信令通道 | 可选 TLS（nginx 反代 + LetsEncrypt） |
| 打洞包防伪 | HMAC(roomId+password, nonce) |
| P2P 加密 | ECDH + AES-128-GCM，每会话独立密钥 |
| 完整性 | AES-GCM auth tag |
| 重放防护 | 48 位递增序列号 |

---

## 项目目录结构

`
remote-desktop/
├── CMakeLists.txt
├── README.md
├── libs/
│   ├── libproto/
│   ├── libcapture/
│   ├── libencode/
│   ├── libnetwork/
│   ├── libinput/
│   ├── libclipboard/
│   ├── libfiletransfer/
│   └── libui/
├── apps/
│   ├── agent/
│   ├── controller/
│   └── signal-server/
├── thirdparty/
│   ├── x264
│   ├── ffmpeg (libavcodec)
│   └── libsodium
├── tests/
└── docs/
`

---

## 第三方依赖

| 能力 | 库 | 说明 |
|------|----|------|
| 视频编码 | x264 | 软件 H.264 编码，预设 ultrafast |
| 视频解码 | FFmpeg libavcodec | 软件 H.264 解码 |
| 加密 | libsodium | ECDH + AES-128-GCM |
| JSON | nlohmann/json | 单头文件 C++ JSON |
| 构建 | CMake + MSVC | Windows 原生构建 |
### 信令服务器 WebSocket 实现

由于整个项目统一使用 C++，signal-server 的 WebSocket 采用以下方案之一：
- 优先使用 **websocketpp** 库（header-only，基于 Boost.Asio），快速实现 WebSocket 帧解析和连接管理
- 备选：手动实现 RFC 6455 WebSocket 握手 + 帧编解码（协议本身较简单，约 200 行代码可覆盖基础需求）
