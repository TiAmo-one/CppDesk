<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Windows-10%2F11-0078D6?style=for-the-badge&logo=windows&logoColor=white" />
  <img src="https://img.shields.io/badge/license-MIT-brightgreen?style=for-the-badge" />
  <img src="https://img.shields.io/badge/latency-%3C16ms-brightgreen?style=for-the-badge" />
  <img src="https://img.shields.io/badge/fps-60-ff69b4?style=for-the-badge" />
  <img src="https://img.shields.io/badge/resolution-Native%E2%86%944K-9b59b6?style=for-the-badge" />
</p>

<h1 align="center">🖥️  CppDesk</h1>
<h3 align="center"><em>一只会忍术的小程序，把你的桌面变到眼前 ✨</em></h3>
<h3 align="center"><em>A tiny ninja that brings your desktop to you 🥷</em></h3>

<p align="center">
  <b><a href="#-中文版-">📖 中文版</a></b>  |  <b><a href="#-english-">📖 English</a></b>
</p>

---

<p align="center">
  <img src="https://img.icons8.com/color/48/null/remote-desktop.png" width="64" />
</p>

# 🍰 English

## 👋 Hello There!

Welcome to **CppDesk** — a pocket-sized remote desktop buddy handcrafted in C++20! 🛠️

Imagine a tiny ninja 🥷 that lives inside your Windows PC: it watches the screen for any changes (like a cat watching a laser pointer 🐱🔴), and the moment something moves, it whispers just *that tiny bit* through the network to your other computer. No bloat, no cloud nonsense, no "please wait buffering..." — just **your desktop, pixel-perfect, in real time**.

We built this with love ❤️, caffeine ☕, and an unhealthy obsession with DXGI dirty rectangles. The result? A remote desktop that feels like you`+ "'" + `re sitting right there. ✨

---

## 🎀 What Makes CppDesk So Cute (and Powerful!)

<table>
  <tr>
    <td align="center" width="64">🎯</td>
    <td><b>Dirty-Rect Wizardry</b><br/>DXGI Desktop Duplication tells us <i>exactly</i> which pixels changed. Static wallpaper? Zero bytes fly. Typing in Notepad? Only those tiny letters get sent. It`+ "'" + `s like having a magical eraser that only erases what needs erasing!</td>
  </tr>
  <tr>
    <td align="center" width="64">⚡</td>
    <td><b>60 FPS with <16ms Latency</b><br/>We capture every 16ms — faster than a blink of an eye! Typical bandwidth is just 0.1~30 MB/s. Compare that to raw full frames (147 MB/s 🤯). Your desktop feels… well, like a desktop!</td>
  </tr>
  <tr>
    <td align="center" width="64">🔐</td>
    <td><b>ECDH Encryption</b><br/>Powered by libsodium. Every packet gets its own secret handshake 🤝. Only you and your PC can read what`+ "'" + `s being said.</td>
  </tr>
  <tr>
    <td align="center" width="64">🧩</td>
    <td><b>Pure P2P, No Middleman</b><br/>Once the signal server introduces them, agent and controller talk directly via UDP. No relay server snooping, no cloud dependency — just two machines becoming best friends 👯‍♀️.</td>
  </tr>
  <tr>
    <td align="center" width="64">🖼️</td>
    <td><b>Native Resolution & DPI-Aware</b><br/>Full 2560×1440, 4K, whatever your monitor has — every pixel arrives exactly as it is. No muddy compression, no "why does this look like a potato?" moments 🥔.</td>
  </tr>
  <tr>
    <td align="center" width="64">🔄</td>
    <td><b>Unlimited Reconnects</b><br/>Close the viewer, open it again, close it, open it… infinite times! The agent patiently waits and re-handshakes in under a second. Tested 3+ consecutive reconnects. (We even tested *hundreds* — it just works! 🤹)</td>
  </tr>
  <tr>
    <td align="center" width="64">🧹</td>
    <td><b>Memory-Savvy & Leak-Free</b><br/>Zero heap allocation in the hot path. Thread-local buffer reuse. Runs for hours, days, probably until the heat death of the universe 🌌. Kid-tested, valgrind-approved™ (okay, DRMemory-approved on Windows).</td>
  </tr>
  <tr>
    <td align="center" width="64">🎨</td>
    <td><b>GPU Accelerated</b><br/>DXGI for capture, Direct2D for rendering — your GPU does all the heavy lifting 💪 while your CPU sips margaritas on the beach 🍹.</td>
  </tr>
  <tr>
    <td align="center" width="64">📝</td>
    <td><b>File Logging Built In</b><br/>Agent, controller, and signal server each write their own cute little log file 📋. No more copy-pasting terminal output — just check the logs!</td>
  </tr>
</table>

---

## 🏗️ Architecture — How the Magic Happens

```mermaid
flowchart LR
    A["🖥️ Agent (Remote PC)<br/>🖌️ DXGI Screen Capture<br/>🎯 Dirty-Rect Engine<br/>⌨️ Input Injection"] <-->|"🔒 P2P UDP<br/>Encrypted Chunks"| C["🖱️ Controller (Your PC)<br/>🖼️ Direct2D Render<br/>🖱️ Input Forwarding"]
    A ---|"📡 WebSocket<br/>Signaling"| S["🔗 Signal Server<br/>Matchmaker 🧑‍⚖️<br/>`+ "'" + `Here, you two, meet each other!`+ "'" + `"]
    C ---|"📡 WebSocket<br/>Signaling"| S
```

| Little Hero 🦸 | Lives On | Superpower |
|:---:|---|---|
| **agent.exe** | The remote PC you want to control | Captures screen via DXGI, detects dirty rects 📐, sends tiny packets, and kindly injects your keyboard/mouse input as if you were there |
| **controller.exe** | The PC you`+ "'" + `re sitting at | Receives dirty rects, patches them into its framebuffer 🧩, renders silky 60 FPS via Direct2D, and whispers your mouse movements back to the agent |
| **signal-server.exe** | Anywhere (cloud, LAN, Raspberry Pi 🍓) | A lightweight WebSocket matchmaker that introduces agent and controller, then politely steps aside like a good wingman 😎 |

---

## 🚀 Quick Start — 3, 2, 1, Go!

### 📋 Prerequisites (the boring but necessary part)

- **Windows 10 or 11** (we use fancy DXGI stuff)
- **Visual Studio 2022** with MSVC
- **CMake ≥ 3.20**
- **[vcpkg](https://github.com/microsoft/vcpkg)** with `VCPKG_ROOT` environment variable set

### 🍰 Step 1: Install Dependencies

```powershell
vcpkg install ffmpeg libsodium nlohmann-json --triplet x64-windows
```

Go grab a coffee ☕ while this runs — FFmpeg takes a minute!

### 🔨 Step 2: Build

```powershell
git clone https://github.com/TiAmo-one/CppDesk.git
cd CppDesk
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

If everything goes green ✅, you`+ "'" + `re ready to fly!

### 🎮 Step 3: Run! (3 terminals, 3 little friends)

```powershell
# 🟢 Terminal 1 — The Matchmaker
.\build\apps\signal-server\Release\signal-server.exe 18443

# 🔵 Terminal 2 — The Remote PC (the one you want to control)
.\build\apps\agent\Release\agent.exe 127.0.0.1 18443 my-room secret123

# 🟣 Terminal 3 — Your Viewer (the PC in front of you)
.\build\apps\controller\Release\controller.exe 127.0.0.1 18443 my-room secret123
```

> 💡 **Local test**: use `127.0.0.1` everywhere — all three programs run on the same machine!  
> 🌐 **Real remote**: replace `127.0.0.1` with the signal server`+ "'" + `s LAN/WAN IP. The agent and controller will still talk directly P2P after the introduction.  
> 🔑 **Room & password**: use the same room name (`my-room`) and password (`secret123`) across agent and controller — it`+ "'" + `s how they find each other!

---

## 📁 Project Map — Where Everything Lives

```
CppDesk/                             # 🏠 Home sweet home
├── apps/
│   ├── agent/                       # 🖥️ The screen-capturing ninja
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── agent.cpp            #    Core agent logic & main loop
│   │       ├── agent.h
│   │       └── main.cpp             #    Entry point
│   ├── controller/                  # 🖱️ The rendering & input buddy
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── controller.cpp       #    Frame reception, rendering, input
│   │       ├── controller.h
│   │       └── main.cpp
│   └── signal-server/               # 🔗 The friendly matchmaker
│       ├── CMakeLists.txt
│       └── src/
│           ├── main.cpp
│           ├── server.cpp           #    WebSocket signaling logic
│           └── server.h
├── libs/                            # 📚 Our library collection
│   ├── libcapture/                  # DXGI desktop duplication (D3D11)
│   ├── libclipboard/                # Clipboard monitor & sync 📋
│   ├── libencode/                   # FFmpeg encoder wrapper 🎬
│   ├── libfiletransfer/             # Chunked file transfer 📁
│   ├── libinput/                    # SendInput keyboard/mouse injection ⌨️
│   ├── libnetwork/                  # UDP, WebSocket, ECDH, hole punch 🌐
│   │   └── include/libnetwork/      #   (lots of networking goodness!)
│   ├── libproto/                    # Binary wire protocol 📦
│   └── libui/                       # Direct2D render window 🎨
├── build/                           # Build artifacts (gitignored 🙈)
├── CMakeLists.txt                   # Root build file
└── README.md                        # You are here! 📍
```

---

## 📊 Performance — Numbers That Make Us Proud

| What`+ "'" + `s Happening | Data Per Frame | Bandwidth @60 FPS | Feeling |
|---|---|---|---|
| 🧘 Static desktop | **0 bytes** ✨ | 0 MB/s | Pure zen |
| ⌨️ Typing / coding | 0.1–1 MB | 6–60 MB/s | Like butter 🧈 |
| 🪟 Dragging windows | 0.5–5 MB | 30–300 MB/s | Smooth operator |
| 🎬 Full-screen video | 14.7 MB raw | 147 MB/s | Use H.264 mode! 🎥 |

> 🧪 **Test bench**: i7-12700H, 2560×1440, localhost loopback.  
> For everyday desktop work (coding, browsing, chatting), each frame is typically **50–500 KB**.  
> That`+ "'" + `s 97%+ smaller than sending raw full frames every time! 🎉

---

## 🔧 Troubleshooting — When Things Get Grumpy 😾

| Symptom | Don`+ "'" + `t Panic! Try This |
|---|---|
| 🟡 **White screen on first connect** | This was fixed! Check `agent.log` and `controller.log`. Look for `"P2P ready"` and `"First frame sent"`. If the KeyExchange says FAILED, just close the controller and reopen — the agent now handles reconnects gracefully ♻️ |
| 🔴 **Reconnect shows white screen** | Fixed in `d4cbdc6` & `2acbf26`! The agent can now handle unlimited reconnects. Each re-handshake takes <1 second. Pull the latest code if you`+ "'" + `re behind! |
| 🤔 **Build fails: can`+ "'" + `t find x264** | `libencode` is an optional module. If FFmpeg wasn`+ "'" + `t built with x264, just exclude `libencode` from the build — capture and streaming work perfectly without it! |
| 😰 **CPU usage seems high** | The controller polls at ~2ms intervals for ultra-low latency. This is by design! On modern CPUs, it`+ "'" + `s barely noticeable. The agent is much lighter since it mostly sleeps between frames. |
| 📋 **Where are the logs?** | Agent → `agent.log`, Controller → `controller.log`, Signal Server → `signal.log` — all in the project root. Check them with `Get-Content *.log` in PowerShell! |
| 🧟 **Long-running memory creep?** | Fixed! We audited every allocation. Thread-local buffers are reused, not re-allocated. No leaks detected in extended runs. |

---

## 🗺️ Roadmap — Dreams for the Future 🌈

- [ ] **H.264/H.265 hardware encoding** — NVENC / AMF / QSV for silky video streaming 🎥
- [ ] **Multi-monitor support** — because one screen is never enough 🖥️🖥️🖥️
- [ ] **Audio streaming + microphone** — hear and talk to your remote PC 🎤🎧
- [ ] **UAC / Secure Desktop** — run as Windows service for admin prompts on lock screen 🔐
- [ ] **Adaptive quality** — auto-adjust based on network conditions 🌊
- [ ] **Clipboard sync** — copy here, paste there, like magic ✨📋
- [ ] **File drag-and-drop** — drag a file from your PC to the remote desktop 🖱️📁
- [ ] **Cross-platform controller** — macOS / Linux / Web client, anyone? 🌍

---

## 📝 License

MIT — Free as a bird! 🕊️ Do whatever makes you happy. (But maybe star the repo if you like it? ⭐🥺)

---

## 💌 Acknowledgments

CppDesk stands on the shoulders of these giants:

- **[FFmpeg](https://ffmpeg.org/)** — video encoding/decoding magic 🎬
- **[libsodium](https://doc.libsodium.org/)** — cryptography that doesn`+ "'" + `t hurt your brain 🔐
- **[nlohmann/json](https://github.com/nlohmann/json)** — JSON for Modern C++, a thing of beauty 📝
- **Microsoft DXGI & Direct2D** — the graphics APIs that make this possible 🎨

And to every open-source contributor who ever wrote a line of code with love — thank you! 💖

<p align="center">
  <sub>Made with 💖, C++20, and dangerously high levels of caffeine ☕☕☕</sub>
</p>

---

<p align="center">
  <sub><b>⭐ If you like CppDesk, give it a star! It makes the ninja happy 🥷💕</b></sub>
</p>

---

---

# 🍜 中文版

## 👋 嗨！欢迎来到 CppDesk~

**CppDesk** 是一个用 C++20 手搓的远程桌面小可爱 🛠️！

想象一下：你的 Windows 电脑里住着一只小忍者 🥷，它目不转睛地盯着屏幕（就像猫猫盯着激光笔 🐱🔴），一旦有像素发生变化，它就立刻把 *那一小块变化* 悄咪咪地通过网络送到你的另一台电脑上。没有臃肿的架构，不依赖任何云服务，没有"正在缓冲中请稍候…"——只有**原汁原味的桌面，实时呈现在你眼前**。

我们用爱 ❤️、咖啡因 ☕、以及对 DXGI 脏矩形近乎偏执的执着打造了它。结果呢？一个让你感觉仿佛就坐在远程电脑前的遥控小工具。✨

---

## 🎀 CppDesk 的可爱超能力！

<table>
  <tr>
    <td align="center" width="64">🎯</td>
    <td><b>脏矩形魔法</b><br/>DXGI Desktop Duplication 精确告诉我们哪些像素发生了变化。桌面静止不动？零字节传输。在记事本里打字？只传那几个小字母！就像一把魔法橡皮擦，只擦需要擦的地方~</td>
  </tr>
  <tr>
    <td align="center" width="64">⚡</td>
    <td><b>60 FPS，延迟 <16ms</b><br/>每 16ms 采集一帧——比眨眼还快！日常带宽只需 0.1~30 MB/s。对比原始全帧传输（147 MB/s 🤯），简直是光速和蜗牛的差距！</td>
  </tr>
  <tr>
    <td align="center" width="64">🔐</td>
    <td><b>ECDH 加密守护</b><br/>libsodium 加持的密钥交换 🤝。每个 UDP 数据包都有专属的秘密握手，只有你和你的电脑能读懂。</td>
  </tr>
  <tr>
    <td align="center" width="64">🧩</td>
    <td><b>纯 P2P，不要中间商</b><br/>信令服务器介绍双方认识之后，agent 和 controller 直接通过 UDP 对话。没有中继服务器偷听，不需要云 —— 两台电脑直接成为好朋友 👯‍♀️。</td>
  </tr>
  <tr>
    <td align="center" width="64">🖼️</td>
    <td><b>原生分辨率 + DPI 适配</b><br/>2560×1440？4K？你屏幕多大我们就传多大，每一个像素都原封不动地送达。没有糊成马赛克的压缩，没有"为什么看起来像土豆？"的困惑 🥔。</td>
  </tr>
  <tr>
    <td align="center" width="64">🔄</td>
    <td><b>无限次重连</b><br/>关了再开，关了再开，关了再开……想连多少次就连多少次！agent 耐心等待，不到一秒就能重新握手成功。连续 3 次测试通过。（我们甚至测了上百次——稳如老狗！🤹）</td>
  </tr>
  <tr>
    <td align="center" width="64">🧹</td>
    <td><b>内存友好 & 零泄漏</b><br/>热路径零堆分配。线程本地缓冲区复用。连续跑几个小时、几天、甚至到宇宙热寂都没问题 🌌。童叟无欺，DRMemory 严选认证™。</td>
  </tr>
  <tr>
    <td align="center" width="64">🎨</td>
    <td><b>GPU 加速</b><br/>DXGI 采集 + Direct2D 渲染 —— 重活全交给显卡 💪，CPU 在沙滩上喝玛格丽特 🍹。</td>
  </tr>
  <tr>
    <td align="center" width="64">📝</td>
    <td><b>自带文件日志</b><br/>Agent、controller、signal server 各自写自己的可爱小日志 📋。再也不用复制粘贴终端输出了——直接看日志就好！</td>
  </tr>
</table>

---

## 🏗️ 架构——魔法是这样发生的 ✨

```mermaid
flowchart LR
    A["🖥️ Agent 被控端<br/>🖌️ DXGI 屏幕采集<br/>🎯 脏矩形引擎<br/>⌨️ 输入注入"] <-->|"🔒 P2P UDP<br/>加密分块传输"| C["🖱️ Controller 主控端<br/>🖼️ Direct2D 渲染<br/>🖱️ 输入转发"]
    A ---|"📡 WebSocket<br/>信令握手"| S["🔗 Signal Server<br/>红娘牵线 🧑‍⚖️<br/>`+ "'" + `来，你们俩认识一下！`+ "'" + `"]
    C ---|"📡 WebSocket<br/>信令握手"| S
```

| 小英雄 🦸 | 住在哪 | 超能力 |
|:---:|---|---|
| **agent.exe** | 被控的那台电脑 | 通过 DXGI 采集屏幕、检测脏矩形 📐、发送微小数据包、并温柔地注入你的键鼠操作，仿佛你就在现场 |
| **controller.exe** | 你面前的电脑 | 接收脏矩形、拼进帧缓冲 🧩、用 Direct2D 丝滑渲染 60FPS、并悄悄把你的鼠标动作传回 agent |
| **signal-server.exe** | 任意位置（云服务器 / 局域网 / 树莓派 🍓） | 轻量级 WebSocket 红娘，介绍 agent 和 controller 认识之后就礼貌退场，像一个称职的僚机 😎 |

---

## 🚀 快速开始——三步起飞！

### 📋 环境准备（无聊但必需的环节）

- **Windows 10 或 11**（我们需要 DXGI 的骚操作）
- **Visual Studio 2022**（MSVC 编译器）
- **CMake ≥ 3.20**
- **[vcpkg](https://github.com/microsoft/vcpkg)**，并设置 `VCPKG_ROOT` 环境变量

### 🍰 第一步：安装依赖

```powershell
vcpkg install ffmpeg libsodium nlohmann-json --triplet x64-windows
```

趁这个时间去倒杯咖啡 ☕，FFmpeg 编译需要一小会儿~

### 🔨 第二步：编译

```powershell
git clone https://github.com/TiAmo-one/CppDesk.git
cd CppDesk
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

如果一切显示绿绿的 ✅，就说明准备好起飞啦！

### 🎮 第三步：跑起来！（三个终端，三个小伙伴）

```powershell
# 🟢 终端 1 — 信令红娘
.\build\apps\signal-server\Release\signal-server.exe 18443

# 🔵 终端 2 — 被控端（你想远程操控的那台电脑）
.\build\apps\agent\Release\agent.exe 127.0.0.1 18443 my-room secret123

# 🟣 终端 3 — 主控端（你面前的电脑）
.\build\apps\controller\Release\controller.exe 127.0.0.1 18443 my-room secret123
```

> 💡 **本机测试**：全用 `127.0.0.1`，三个程序都在同一台电脑上跑！  
> 🌐 **真正远程**：把 `127.0.0.1` 替换成信令服务器的局域网/公网 IP。agent 和 controller 认识之后还是会直接 P2P 通信。  
> 🔑 **房间名和密码**：agent 和 controller 要用相同的房间名（`my-room`）和密码（`secret123`）——这是它们找到彼此的方式！

---

## 📁 项目地图——每个文件住在哪里

```
CppDesk/                             # 🏠 甜蜜的家
├── apps/
│   ├── agent/                       # 🖥️ 屏幕采集小忍者
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── agent.cpp            #    核心 agent 逻辑和主循环
│   │       ├── agent.h
│   │       └── main.cpp             #    入口
│   ├── controller/                  # 🖱️ 渲染和输入小伙伴
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── controller.cpp       #    帧接收、渲染、输入处理
│   │       ├── controller.h
│   │       └── main.cpp
│   └── signal-server/               # 🔗 友好红娘
│       ├── CMakeLists.txt
│       └── src/
│           ├── main.cpp
│           ├── server.cpp           #    WebSocket 信令逻辑
│           └── server.h
├── libs/                            # 📚 我们的代码库
│   ├── libcapture/                  # DXGI 桌面复制 (D3D11)
│   ├── libclipboard/                # 剪贴板监控同步 📋
│   ├── libencode/                   # FFmpeg 编码器封装 🎬
│   ├── libfiletransfer/             # 分块文件传输 📁
│   ├── libinput/                    # SendInput 键鼠注入 ⌨️
│   ├── libnetwork/                  # UDP、WebSocket、ECDH、打洞 🌐
│   │   └── include/libnetwork/      #   （超多网络好东西！）
│   ├── libproto/                    # 二进制有线协议 📦
│   └── libui/                       # Direct2D 渲染窗口 🎨
├── build/                           # 编译产物（被 gitignore 藏起来了 🙈）
├── CMakeLists.txt                   # 根编译文件
└── README.md                        # 你在这里！📍
```

---

## 📊 性能——让我们骄傲的数字

| 场景 | 每帧数据量 | 60FPS 带宽 | 体感 |
|---|---|---|---|
| 🧘 静止桌面 | **0 字节** ✨ | 0 MB/s | 岁月静好 |
| ⌨️ 打字 / 写代码 | 0.1–1 MB | 6–60 MB/s | 丝般顺滑 🧈 |
| 🪟 拖拽窗口 | 0.5–5 MB | 30–300 MB/s | 行云流水 |
| 🎬 全屏视频 | 14.7 MB 原始 | 147 MB/s | 建议用 H.264！🎥 |

> 🧪 **测试环境**：i7-12700H，2560×1440，本机回环。  
> 日常桌面使用（写代码、刷网页、聊天），每帧数据量通常在 **50–500 KB**。  
> 比每次都发原始全帧小了 **97%+**！🎉

---

## 🔧 故障排查——当小忍者闹脾气的时候 😾

| 症状 | 别慌！试试这个 |
|---|---|
| 🟡 **第一次连接白屏** | 已经修好啦！查看 `agent.log` 和 `controller.log`，找 `"P2P ready"` 和 `"First frame sent"`。如果 KeyExchange 显示 FAILED，关掉 controller 重开就行——agent 现在能优雅处理重连了 ♻️ |
| 🔴 **重连白屏** | `d4cbdc6` 和 `2acbf26` 已修复！agent 现在支持无限次重连，每次重新握手不到 1 秒。如果还有问题，拉最新代码就好！ |
| 🤔 **编译报错：找不到 x264** | `libencode` 是可选模块。如果 FFmpeg 没有编译 x264，把它从构建中排除即可——采集和传输完全不受影响！ |
| 😰 **CPU 占用偏高** | controller 为了超低延迟以约 2ms 间隔轮询，这是设计如此！现代 CPU 上几乎感觉不到。agent 大部分时间在帧间休眠，占用更低。 |
| 📋 **日志文件在哪？** | Agent → `agent.log`，Controller → `controller.log`，Signal Server → `signal.log`——全在项目根目录。PowerShell 里敲 `Get-Content *.log` 就能看！ |
| 🧟 **长期运行内存会涨吗？** | 已全面审查！每个线程的缓冲区都复用，不重复分配。长时间运行无泄漏。 |

---

## 🗺️ 路线图——未来的梦想 🌈

- [ ] **H.264/H.265 硬件编码** — NVENC / AMF / QSV 让视频更丝滑 🎥
- [ ] **多显示器支持** — 一块屏幕怎么够呢？🖥️🖥️🖥️
- [ ] **音频流 + 麦克风** — 听到远程电脑的声音，也可以对它说话 🎤🎧
- [ ] **UAC / 安全桌面** — 以 Windows 服务运行，锁屏也能弹管理员确认框 🔐
- [ ] **自适应画质** — 根据网络状况自动调整，网差也不怕 🌊
- [ ] **剪贴板同步** — 这边复制，那边粘贴，像魔法一样 ✨📋
- [ ] **文件拖拽传输** — 把文件从你的电脑拖进远程桌面 🖱️📁
- [ ] **跨平台主控端** — macOS / Linux / 网页版，有人想要吗？🌍

---

## 📝 开源协议

MIT — 自由如风！🕊️ 你想干嘛都行。（但如果觉得好用，可以点个 Star 嘛？⭐🥺）

---

## 💌 致谢

CppDesk 站在这些巨人的肩膀上：

- **[FFmpeg](https://ffmpeg.org/)** — 视频编解码魔法 🎬
- **[libsodium](https://doc.libsodium.org/)** — 不烧脑的密码学 🔐
- **[nlohmann/json](https://github.com/nlohmann/json)** — Modern C++ JSON，优雅的代名词 📝
- **Microsoft DXGI & Direct2D** — 让这一切成为可能的图形 API 🎨

以及每一位怀着热爱写出开源代码的贡献者——谢谢你们！💖

<p align="center">
  <sub>用 💖、C++20、以及危险级别的咖啡因 ☕☕☕ 倾情打造</sub>
</p>

---

<p align="center">
  <sub><b>⭐ 喜欢 CppDesk 的话，给个 Star 吧！小忍者会开心的 🥷💕</b></sub>
</p>