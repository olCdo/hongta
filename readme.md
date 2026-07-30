# Honta AMR 视觉定位模块

Honta 是面向 AMR 顶盖标定、圆形目标检测和喇叭口坐标换算的原生视觉模块。项目提供 C++17 核心库、C ABI、Android JNI 集成工程、Android 动态库交付包和 RTSP 处理链路。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| RTSP 摄像头输入 | 已实现，使用 FFmpeg 解码 |
| 入口通孔检测 | 已实现 |
| 中心喇叭口检测 | 已实现 |
| 检测画面叠加 | 已实现 |
| RTSP publisher | 已实现，可发布到 MediaMTX |
| Android ARM64 | 已提供 `arm64-v8a/libhonta_native.so` |
| Android 模拟器 | 已提供 `x86_64/libhonta_native.so` |
| Android native 日志 | 已实现，统一标签为 `HontaNative` |
| RTSP 凭据脱敏 | 已实现，日志不会输出用户名和密码 |
| RTSP 低延迟处理 | 已实现，只处理最新输入帧并按墙钟生成发布 PTS |
| 处理分辨率 | 通用交付配置为 `processing_scale=0.5` |
| Modbus TCP 通信 | 尚未实现，当前只有未接入业务链路的命令门控类 |

## 处理链路

```text
RTSP 摄像头
    ↓
FFmpeg 解复用和视频解码
    ↓
最新帧缓存、裁剪、按比例缩放
    ↓
圆形检测、候选目标更新
    ↓
OpenCV 绘制候选圆、中心十字和运行状态
    ↓
FFmpeg 视频编码
    ↓
RTSP publisher
    ↓
MediaMTX
    ↓
VLC、FFplay 或其他 RTSP 播放器
```

Android 端是 RTSP publisher/client，不是 RTSP server。MediaMTX 必须先运行，`.so` 在 `startDetect()` 读到并处理第一帧后才会向 MediaMTX 注册发布路径。

## 仓库结构

```text
code/include/                 C++ 公共头文件和 C ABI
code/src/                     配置、检测、标定和 RTSP 核心实现
code/tests/                   C++ 单元测试和错误传播测试
code/tools/                   标定和视频测试工具
code/configs/                 示例运行配置
android/                      Android JNI smoke-test 工程
android_handoff/              Android 工程师最小交付包
materials/docs/               API、集成和阶段验证文档
tools/                        Android 构建及测试脚本
CMakeLists.txt                C++ 顶层构建入口
vcpkg.json                    PC 和 Android 原生依赖清单
```

## Android 交付库

交付目录：

```text
android_handoff/libs/arm64-v8a/libhonta_native.so
android_handoff/libs/x86_64/libhonta_native.so
```

| ABI | 用途 | SHA-256 |
| --- | --- | --- |
| `arm64-v8a` | ARM64 安卓真机 | `0C937DF46A64E6031FCC9F22E9AF4F031BF0675313E3A842E1DF398ACF9B728B` |
| `x86_64` | Android Emulator | `0DACDDF29C080032C44CD358DE8C3542F5D3FB37B417F1028A475BCB661D0FC9` |

两个动态库均启用真实 RTSP 检测，并包含 Android `HontaNative` 日志。

## 运行配置

`runtime.json` 中的 `rtsp` 节点决定 RTSP 输入和 publisher 目标，`runtime.processing_scale` 决定检测和叠加流的处理尺寸：

```json
{
  "rtsp": {
    "input_url": "rtsp://<用户名>:<密码>@<摄像头IP>:554/11",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "<MediaMTX所在PC的局域网IP>",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay"
  },
  "runtime": {
    "camera_config_path": "camera/top_rgb_camera.json",
    "top_cover_data_dir": "top_cover",
    "processing_scale": 0.5
  }
}
```

配置说明：

- `input_url`：摄像头原始 RTSP 地址。
- `overlay_public_host`：Android 设备可访问的 MediaMTX 地址，真机不能填写 `127.0.0.1`。
- `overlay_port`：MediaMTX RTSP 监听端口，默认使用 `8554`。
- `overlay_path`：处理后视频的发布路径。
- `overlay_bind_ip`：当前配置校验要求非空，publisher 目标仍由 `overlay_public_host` 决定。
- `processing_scale`：裁剪后的图像缩放比例，默认 `1.0`，有效范围为 `(0, 1]`。输入为 `1280×720`、关闭裁剪且配置为 `0.5` 时，检测和叠加流尺寸为 `640×360`。
- `debug_rtsp`：当前尚未接入 `HontaContext`，不要把 `/honta_debug` 当作验收路径。

检测器中以像素表示的半径和距离参数会按 `processing_scale` 同步缩放，检测坐标在写入业务结果前会恢复到原始输入坐标系，不需要在配置中手工把半径再除以二。

不要把真实摄像头密码提交到 Git。正式 App 应从受控配置目录读取运行配置。

## 低延迟处理

- FFmpeg 输入线程持续读取摄像头数据，检测线程只获取最新帧；检测速度低于输入帧率时会丢弃过期帧，避免延迟持续累积。
- 丢帧达到日志阈值时会输出 `FfmpegRtspInput: dropped stale frames ...`，这是低延迟策略生效，不是输入错误。
- RTSP publisher 使用单调墙钟生成视频 PTS。即使检测只能输出约 3fps，也不会再把数据错误标记为固定 10fps，避免播放器出现“卡住数秒后快速追赶”。
- `processing_scale=0.5` 用于降低检测和编码开销；它降低处理分辨率，不改变业务坐标输出所使用的原始坐标系。

## 启动 MediaMTX

Windows 示例：

```powershell
Set-Location D:\path\to\mediamtx
.\mediamtx.exe .\mediamtx.yml
```

正常启动时应看到：

```text
[RTSP] started with listeners on :8554
```

当 Android publisher 注册成功后，MediaMTX 控制台会显示 `/honta_overlay` 正在发布。

处理流播放地址：

```text
rtsp://<MediaMTX所在PC的局域网IP>:8554/honta_overlay
```

PC 播放示例：

```powershell
ffplay -rtsp_transport tcp rtsp://<PC-IP>:8554/honta_overlay
```

## Android 日志

连接设备：

```powershell
adb devices -l
```

清空旧日志并只查看 native 日志：

```powershell
adb -s <设备序列号> logcat -c
adb -s <设备序列号> logcat -v time "HontaNative:V" "AndroidRuntime:E" "*:S"
```

正常发布时应依次看到：

```text
DetectionService: start detect_type=entrance_hole
FfmpegRtspInput: opening input=rtsp://***@<摄像头IP>:554/11
FfmpegRtspInput: input opened
RtspPublisher: opening publisher=rtsp://<PC-IP>:8554/honta_overlay
RtspPublisher: publisher online
```

日志中的 `***` 是凭据脱敏结果。

如果出现：

```text
input read failed: failed to read RTSP packet: End of file
```

表示摄像头输入会话结束。检测服务会关闭旧 publisher、重新连接摄像头并再次注册 publisher。重连期间播放器可能短暂黑屏或显示路径暂无流。

## Android 原生库构建

前置条件：

- Android SDK
- Android NDK `26.1.10909125` 或兼容版本
- CMake
- vcpkg
- Ninja

从仓库根目录构建 ARM64：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi arm64-v8a `
  -CopyToAndroidProject
```

构建 x86_64：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi x86_64 `
  -CopyToAndroidProject
```

连续构建两个 ABI：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi all `
  -CopyToAndroidProject
```

其他机器可显式指定路径：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi all `
  -AndroidNdkHome <NDK路径> `
  -CMakeExe <cmake.exe路径> `
  -VcpkgRoot <vcpkg路径> `
  -CopyToAndroidProject
```

## Android APK 构建

将对应 ABI 的 `libhonta_native.so` 放到：

```text
android/app/src/main/jniLibs/<abi>/libhonta_native.so
```

然后使用 Android Studio 打开 `android/`，或通过 Gradle 构建。ABI 示例：

```powershell
gradle -p android :app:assembleDebug "-Pandroid.injected.build.abi=arm64-v8a"
gradle -p android :app:assembleDebug "-Pandroid.injected.build.abi=x86_64"
```

使用注入 ABI 属性生成的测试 APK 可能带有 `testOnly` 标记，安装时使用：

```powershell
adb -s <设备序列号> install -t -r <APK路径>
```

## Android 调用顺序

核心调用顺序：

```text
init(configPath)
  → setTopCoverModel(modelId)
  → startDetect("entrance_hole")
  → confirmEntranceCandidate(...)
  → startDetect("center_horn")
  → confirmCenterCandidate(...)
  → calculateAllHorns()
  → getHornCoordinate(...)
  → stopDetect()
  → release()
```

规则：

- `init/start/stop/release` 必须串行调用。
- 耗时 native 调用不能运行在 Android UI 线程。
- Activity 或业务服务销毁时必须调用 `release()`。
- 切换检测类型时，内部会先停止旧检测线程再启动新线程。

## 2026-07-27 实际验证

已完成以下端到端验证：

```text
真实 HEVC 摄像头
  → Android Emulator 中的 x86_64 libhonta_native.so
  → 圆形检测和画面叠加
  → MediaMTX
  → PC FFprobe/FFmpeg/FFplay
```

验证结果：

- 输入视频：HEVC，`1920×1080`，20fps。
- 输出视频：`1280×720`，10fps。
- `HontaNative` 出现 `input opened`。
- `HontaNative` 出现 `publisher online`。
- MediaMTX 的 `/honta_overlay` 可被 FFprobe 和 FFplay读取。
- 输出画面包含检测类型、帧号、候选数量、运行状态和中心十字。
- 测试中观察到摄像头偶发 EOF，自动重连可以恢复 publisher。

## 2026-07-30 低延迟验证

- 最终 strip 后的 ARM64 和 x86_64 交付库已逐字节核对进入测试 APK。
- ARM64 真机和 x86_64 模拟器均以 `center_horn` 启动，日志出现 `input opened`、`processing frame source=1280x720 scaled=640x360 scale=0.500000` 和 `publisher online`。
- 测试输入为 `1280×720`、10fps 的标准 H.264 RTSP 流；两个 ABI 的输出均为 MPEG-4 Part 2、`640×360`。
- ARM64 连续采样中，5.83 秒墙钟对应 4.8 秒 PTS，采集 18 个视频包。
- x86_64 连续采样中，5.36 秒墙钟对应 4.9 秒 PTS，采集 50 个视频包。
- 两个设备均未匹配到 `DESCRIBE 404`、`ExoPlaybackException`、检测失败或 publisher 失败日志。
- 实际处理帧率仍取决于检测耗时；低帧率时画面可能呈现逐帧感，但媒体时间轴不会再出现数秒停顿后集中追赶。

## 已知限制

1. Android 构建未包含 `libx264` 时，输出编码器会从 H.264 回退为 MPEG-4 Part 2。本次验证输出为 `mpeg4`，FFplay 可以播放，但 Android Media3 可能不兼容。正式验收若要求 H.264，需要补齐可用的 H.264 编码器。
2. `debug_rtsp` 配置尚未接入实际运行链路。
3. smoke-test App 会自动确认固定候选编号。场景中没有对应目标时，界面可能显示 `JNI smoke test: FAIL`，但这不等于 RTSP 输入或 publisher 失败，应同时查看 `HontaNative` 日志和 MediaMTX 状态。
4. 某些 Android Emulator 系统镜像可能没有正确初始化虚拟网络。真实网络和性能验收应优先使用 ARM64 安卓真机。
5. 当前仓库没有完整的 Modbus TCP 收发实现。

## PC 构建和测试

示例：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg路径>\scripts\buildsystems\vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

## 进一步阅读

- `android/README.md`
- `android_handoff/README.md`
- `materials/docs/phase7_android_api_reference.md`
- `materials/docs/phase7_android_engineer_handoff.md`
- `materials/docs/phase7_android_real_native_handoff.md`
- `materials/docs/native_rtsp_ffmpeg_boundary.md`
