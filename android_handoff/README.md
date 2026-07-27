# Honta Android 动态库交付包

本目录是交付给 Android 工程师的最小集成包，包含两个 ABI 的真实 RTSP 检测库、C API、Java 封装示例、运行配置和接口文档。

## 目录内容

```text
libs/
  arm64-v8a/libhonta_native.so      ARM64 安卓真机动态库
  x86_64/libhonta_native.so         Android Emulator 动态库

configs/
  runtime.json                      运行配置
  camera/top_rgb_camera.json        相机内参和安装配置
  top_cover/A001.json               顶盖模型

api/
  honta_api.h                       C ABI 头文件

java/
  com/honta/vision/HontaNative.java
  com/honta/vision/HontaNativeController.java

docs/
  android_api_reference.md          Android API 参考
```

## 发布文件

| ABI | 文件大小 | SHA-256 |
| --- | ---: | --- |
| `arm64-v8a` | 29,076,264 字节 | `ABA8F07583A2A4754A8C8866899F45D7E8005B9EBDEE1A132F6E69C6A06A31A7` |
| `x86_64` | 29,285,840 字节 | `880737867685873864F4DEB770A32AE27F59FB8A407845CA4B3CE9AFEDEF0EF1` |

两个文件均满足：

- 启用真实 RTSP 检测。
- 包含 `HontaNative` Android 日志。
- 链接 Android `liblog.so`。
- 保留核心 C API 导出。
- 日志会脱敏 RTSP 用户名和密码。
- 发布文件已执行 `strip --strip-unneeded`。

## 复制到 Android 工程

ARM64 真机：

```text
app/src/main/jniLibs/arm64-v8a/libhonta_native.so
```

x86_64 模拟器：

```text
app/src/main/jniLibs/x86_64/libhonta_native.so
```

必须同时提供 JNI bridge。可以直接参考：

```text
../android/app/src/main/cpp/HontaNativeBridge.cpp
../android/app/src/main/cpp/CMakeLists.txt
```

## 配置文件部署

保持以下相对目录结构：

```text
runtime.json
camera/top_rgb_camera.json
top_cover/A001.json
```

将配置复制到 App 可读目录后，把 `runtime.json` 的绝对路径传给：

```java
HontaNative.init(configPath);
```

RTSP 配置示例：

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

不要在正式仓库或日志中写入真实密码。

## Java 调用示例

```java
HontaNative.init(configPath);
HontaNative.setTopCoverModel("A001");

String outputUrl = HontaNative.getOverlayRtspUrl();
HontaNative.startDetect("entrance_hole");

// 业务层确认入口候选后再切换检测类型。
HontaNative.confirmEntranceCandidate(
        candidateId,
        entranceHoleNo,
        heightMm,
        amrX,
        amrY,
        amrYaw);

HontaNative.startDetect("center_horn");

// 页面或业务任务结束时必须释放。
HontaNative.stopDetect();
HontaNative.release();
```

耗时调用不能在 Android UI 线程执行。推荐通过 `HontaNativeController` 或单线程 executor 串行管理 native 生命周期。

## 查看 native 日志

```powershell
adb -s <设备序列号> logcat -c
adb -s <设备序列号> logcat -v time `
  "HontaNative:V" `
  "AndroidRuntime:E" `
  "*:S"
```

正常发布日志：

```text
FfmpegRtspInput: opening input=rtsp://***@<摄像头IP>:554/11
FfmpegRtspInput: input opened
RtspPublisher: opening publisher=rtsp://<PC-IP>:8554/honta_overlay
RtspPublisher: publisher online
```

## MediaMTX 验证

MediaMTX 默认 RTSP 端口：

```text
8554
```

处理流：

```text
rtsp://<PC-IP>:8554/honta_overlay
```

验证：

```powershell
ffprobe -rtsp_transport tcp `
  rtsp://<PC-IP>:8554/honta_overlay

ffplay -rtsp_transport tcp `
  rtsp://<PC-IP>:8554/honta_overlay
```

## 错误解释

### `no stream is available on path`

MediaMTX 已收到播放器请求，但 publisher 尚未上线。检查 Android 日志中是否出现 `publisher online`。

### `failed to read RTSP packet: End of file`

摄像头结束当前 RTSP 会话。检测服务会自动重连摄像头并重新建立 publisher。

### 候选确认失败

当前画面中没有对应候选编号。候选确认失败不等于 RTSP 输入或 publisher 失败。

## 已知限制

1. 当前 Android FFmpeg 构建没有 `libx264` 时，publisher 会回退为 MPEG-4 Part 2。正式要求 H.264 时必须补齐 H.264 编码器。
2. `debug_rtsp` 尚未接入实际运行链路。
3. 模拟器网络可能受 AVD 配置影响，最终网络和性能验收应使用 ARM64 真机。
4. 本交付包不包含 Modbus TCP 通信实现。

## 2026-07-27 验证记录

- 真实摄像头输入：HEVC，`1920×1080`，20fps。
- x86_64 `.so` 在 Android Emulator 中成功加载。
- native 日志出现 `input opened` 和 `publisher online`。
- MediaMTX 成功接收 `/honta_overlay`。
- 输出为 `1280×720`、10fps。
- FFprobe、FFmpeg 和 FFplay 可以读取处理流。
- 输出画面包含检测状态和中心十字。
- 摄像头 EOF 后自动重连可以恢复 publisher。

完整工程说明请阅读：

- `../readme.md`
- `../android/README.md`
- `docs/android_api_reference.md`
