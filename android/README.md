# Honta Android 集成说明

本目录是 Android JNI 集成工程。当前 App 是用于验证 Java、JNI bridge、`libhonta_native.so`、真实 RTSP 输入和 RTSP publisher 的 smoke-test App，不是最终业务界面。

## 工程组成

```text
app/src/main/java/com/honta/vision/
  HontaNative.java                 Java native API
  HontaNativeController.java       native 生命周期串行控制器
  MainActivity.java                自动 smoke test 页面
  AmrPoseSnapshot.java             AMR 位姿数据
  AmrPoseValidator.java            AMR 位姿有效性检查
  ModbusCommandGate.java           命令门控，尚未接入 Modbus 通信
  CoordinateTableMetadata.java     坐标表元数据

app/src/main/cpp/
  HontaNativeBridge.cpp            JNI 到 C ABI 的薄封装
  CMakeLists.txt                   JNI 构建和预编译库导入

app/src/main/assets/phase7_smoke/
  runtime.json                     smoke-test 运行配置
  camera/top_rgb_camera.json       相机配置
  top_cover/A001.json              顶盖模型

app/src/main/jniLibs/<abi>/
  libhonta_native.so               本地构建产物，不提交到 Git
```

可发布的动态库位于：

```text
../android_handoff/libs/arm64-v8a/libhonta_native.so
../android_handoff/libs/x86_64/libhonta_native.so
```

## ABI 选择

| ABI | 运行环境 |
| --- | --- |
| `arm64-v8a` | ARM64 安卓平板或手机 |
| `x86_64` | Android Emulator |

ABI 必须与设备一致。x86_64 模拟器不能加载 ARM64 动态库，ARM64 真机也不能使用 x86_64 动态库。

查看设备 ABI：

```powershell
adb -s <设备序列号> shell getprop ro.product.cpu.abi
```

## 放置动态库

真机：

```text
app/src/main/jniLibs/arm64-v8a/libhonta_native.so
```

模拟器：

```text
app/src/main/jniLibs/x86_64/libhonta_native.so
```

JNI bridge 的 CMake 配置会在缺少对应 ABI 的 `libhonta_native.so` 时直接失败，避免生成运行时才因缺库崩溃的 APK。

## RTSP 配置

编辑：

```text
app/src/main/assets/phase7_smoke/runtime.json
```

示例：

```json
{
  "rtsp": {
    "input_url": "rtsp://<用户名>:<密码>@<摄像头IP>:554/11",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "<MediaMTX所在PC的局域网IP>",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay"
  }
}
```

注意事项：

- 真机中的 `127.0.0.1` 是真机自身，不能代表 PC。
- 模拟器中的 `127.0.0.1` 是模拟器自身。
- 局域网部署建议填写 PC 的 WLAN 或有线网卡地址。
- App 启动时会把 assets 配置复制到应用内部目录，并把内部 `runtime.json` 绝对路径传给 `HontaNative.init()`。
- 修改本地配置后必须重新构建和安装 APK，旧 APK 不会自动更新。
- 当前代码只使用 `rtsp` 节点；`debug_rtsp` 尚未接入 `HontaContext`。

## 网络预检查

PC 检查摄像头：

```powershell
Test-NetConnection <摄像头IP> -Port 554
ffprobe -rtsp_transport tcp "rtsp://<用户名>:<密码>@<摄像头IP>:554/11"
```

Android 检查摄像头和 MediaMTX：

```powershell
adb -s <设备序列号> shell toybox nc -z -w 3 <摄像头IP> 554
adb -s <设备序列号> shell toybox nc -z -w 3 <PC-IP> 8554
```

只有两项都连通，才开始判断 `.so` 行为。

## 构建 native 库

在仓库根目录运行：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi arm64-v8a `
  -CopyToAndroidProject
```

模拟器：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi x86_64 `
  -CopyToAndroidProject
```

全部 ABI：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi all `
  -CopyToAndroidProject
```

默认环境：

- NDK：`26.1.10909125`
- CMake：Android SDK 内的 CMake
- C++ 标准：C++17
- STL：项目当前构建参数定义的 Android STL
- 依赖：FFmpeg、OpenCV、Android NDK 系统库

## 构建 APK

推荐使用 Android Studio 打开本目录。

命令行示例：

```powershell
$env:JAVA_HOME='<Android Studio JBR路径>'
$env:ANDROID_HOME="$env:LOCALAPPDATA\Android\Sdk"

gradle -p android :app:assembleDebug `
  "-Pandroid.injected.build.abi=arm64-v8a"
```

模拟器：

```powershell
gradle -p android :app:assembleDebug `
  "-Pandroid.injected.build.abi=x86_64"
```

安装：

```powershell
adb -s <设备序列号> install -t -r <APK路径>
```

使用 `android.injected.build.abi` 生成的 APK 可能带 `testOnly`，因此安装命令包含 `-t`。

## 启动 smoke test

```powershell
adb -s <设备序列号> shell am force-stop com.honta.vision
adb -s <设备序列号> shell am start -n com.honta.vision/.MainActivity
```

`MainActivity` 会在启动约 500 毫秒后自动执行：

```text
init
  → setTopCoverModel
  → getOverlayRtspUrl
  → startDetect("entrance_hole")
  → confirmEntranceCandidate
  → startDetect("center_horn")
  → confirmCenterCandidate
  → calculateAllHorns
  → getHornCoordinate
  → release
```

切换检测类型时会先停止旧 worker，因此日志中出现：

```text
stop requested
worker stopped detect_type=entrance_hole
start detect_type=center_horn
```

属于正常行为。

## 查看日志

清空旧日志：

```powershell
adb -s <设备序列号> logcat -c
```

只看 native 和崩溃日志：

```powershell
adb -s <设备序列号> logcat -v time `
  "HontaNative:V" `
  "AndroidRuntime:E" `
  "*:S"
```

正常链路：

```text
DetectionService: start detect_type=entrance_hole
FfmpegRtspInput: opening input=rtsp://***@<摄像头IP>:554/11
FfmpegRtspInput: input opened
RtspPublisher: opening publisher=rtsp://<PC-IP>:8554/honta_overlay
RtspPublisher: publisher online
```

`publisher online` 表示 FFmpeg 已成功向 MediaMTX 完成 RTSP 发布握手。是否持续有视频仍应使用 FFprobe、FFplay 或 VLC 验证。

## 播放处理流

```powershell
ffprobe -rtsp_transport tcp `
  rtsp://<PC-IP>:8554/honta_overlay

ffplay -rtsp_transport tcp `
  rtsp://<PC-IP>:8554/honta_overlay
```

处理画面包含：

- 检测类型。
- 帧号。
- 候选数量。
- 检测服务状态。
- 图像中心十字。
- 检测到的候选圆和编号。

## 常见问题

### MediaMTX 提示没有流

```text
no stream is available on path 'honta_overlay'
```

表示播放器已经连接 MediaMTX，但 Android publisher 尚未注册。先查看是否出现：

```text
RtspPublisher: publisher online
```

### 摄像头输入结束

```text
input read failed: failed to read RTSP packet: End of file
```

表示摄像头结束当前 RTSP 会话。服务会自动重连并重新创建 publisher。重连期间视频会暂时中断。

### smoke test 显示 FAIL

固定候选编号在当前画面中不存在时，候选确认会失败。只要日志已经出现 `input opened` 和 `publisher online`，RTSP 链路仍可能正常。

### Media3 无法播放处理流

当 Android FFmpeg 没有可用的 H.264 YUV420P 编码器时，当前代码会回退到 MPEG-4 Part 2。FFplay 可以播放，但 Android Media3 可能不支持。使用 FFprobe 检查：

```powershell
ffprobe -v error -show_entries stream=codec_name `
  rtsp://<PC-IP>:8554/honta_overlay
```

正式要求 H.264 时，必须在 native 构建中提供兼容的 H.264 编码器，并确认输出 `codec_name=h264`。

### 模拟器没有网络

先检查：

```powershell
adb -s emulator-5554 shell ip -4 route
```

如果没有默认路由，属于 AVD 网络环境问题，不是 `.so` RTSP 逻辑问题。真实设备验收应优先使用 ARM64 真机。

## 集成规则

- JNI 只封装 `honta_api.h`，不要直接包含内部 C++ 头文件。
- 所有 native 生命周期调用必须串行化。
- 不要在 UI 线程执行耗时 native 调用。
- Activity、Service 或业务控制器销毁时必须调用 `release()`。
- 上层应同时记录 native 异常、MediaMTX 日志和最终播放验证结果。
- 不要在日志或仓库中保存摄像头明文密码。

## 进一步阅读

- `../readme.md`
- `../android_handoff/README.md`
- `../materials/docs/phase7_android_api_reference.md`
- `../materials/docs/phase7_android_engineer_handoff.md`
- `../materials/docs/phase7_android_real_native_handoff.md`
