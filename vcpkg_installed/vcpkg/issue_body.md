Package: ffmpeg[avcodec,avdevice,avfilter,avformat,core,swresample,swscale]:x64-windows@8.1.2#3

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.44.35213.0
- CMake Version: 4.4.0
-    vcpkg-tool version: 2026-07-13-bf04c909169fdbb30821c02c6eb01f1cd1295d05
    vcpkg-scripts version: 908da3a305 2026-07-16 (8 hours ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://github.com/ffmpeg/ffmpeg/archive/n8.1.2.tar.gz -> ffmpeg-ffmpeg-n8.1.2.tar.gz
Successfully downloaded ffmpeg-ffmpeg-n8.1.2.tar.gz
-- Extracting source C:/Users/17410/Desktop/remote control/work/tools/vcpkg/downloads/ffmpeg-ffmpeg-n8.1.2.tar.gz
-- Applying patch 0002-fix-msvc-link.patch
-- Applying patch 0003-fix-windowsinclude.patch
-- Applying patch 0004-dependencies.patch
-- Applying patch 0005-fix-nasm.patch
-- Applying patch 0007-fix-lib-naming.patch
-- Applying patch 0013-define-WINVER.patch
-- Applying patch 0024-fix-osx-host-c11.patch
-- Applying patch 0040-ffmpeg-add-av_stream_get_first_dts-for-chromium.patch
-- Applying patch 0045-use-prebuilt-bin2c.patch
-- Applying patch 0046-fix-msvc-detection.patch
-- Applying patch 0047-fix-msvc-utf8.patch
-- Applying patch 0048-backport-23039.patch
-- Applying patch 0049-fix-twolame-pkgconfig.patch
-- Applying patch 0050-fix-test-ld-absolute-lib-paths.patch
-- Using source at C:/Users/17410/Desktop/remote control/work/tools/vcpkg/buildtrees/ffmpeg/src/n8.1.2-114db953a4.clean
CMake Error at ports/ffmpeg/portfile.cmake:25 (message):
  Error: ffmpeg will not build with spaces in the path.  Please use a
  directory with no spaces
Call Stack (most recent call first):
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "remote-desktop",
  "version": "0.1.0",
  "dependencies": [
    "x264",
    "ffmpeg",
    "libsodium",
    "nlohmann-json",
    "gtest"
  ]
}

```
</details>
