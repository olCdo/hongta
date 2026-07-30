# 第七阶段 Android 平板 USB 真机 RTSP 联调记录

日期：2026-06-01

## 设备信息

通过 USB ADB 识别到平板：

```text
serial: 8565SFCEDC00000885
model: P85T(P3M5)
Android: 14
SDK: 34
ABI: arm64-v8a, armeabi-v7a, armeabi
```

## 测试拓扑

```text
PC Python synthetic RTSP publisher
  -> PC MediaMTX :8564 /honta_test
  -> USB adb reverse tcp:8564 tcp:8564
  -> Android tablet app com.honta.vision
  -> arm64-v8a libhonta_native.so
  -> PC MediaMTX :8564 /honta_overlay
```

平板端配置：

```text
input_url = rtsp://127.0.0.1:8564/honta_test
overlay_url = rtsp://127.0.0.1:8564/honta_overlay
```

ADB reverse：

```text
UsbFfs tcp:8564 tcp:8564
```

## 安装结果

APK：

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

安装结果：

```text
Performing Streamed Install
Success
```

native 加载确认：

```text
library_path=.../lib/arm64:.../base.apk!/lib/arm64-v8a
```

## 运行结果

页面最终结果：

```text
confirmEntranceCandidate(candidate 1)... OK attempt=10
confirmCenterCandidate(candidate 1)... OK attempt=10
calculateAllHorns... OK
getHornCoordinate(1)... horn1=(1.6800118865966795, 1.1499890670776367)
release... OK
JNI smoke test: PASS
```

MediaMTX 日志确认：

```text
honta_test: session is reading from path 'honta_test', with TCP, 1 track (H264)
honta_overlay: stream is available and online, 1 track (MPEG-4 Video)
honta_overlay: session is publishing to path 'honta_overlay'
```

截图：

```text
.codex_tmp/phase7_tablet_test/honta_tablet_real_rtsp_pass.png
.codex_tmp/phase7_tablet_test/honta_tablet_real_rtsp_pass_bottom.png
```

## 结论

USB 连接的 Android 平板已通过真实 `arm64-v8a libhonta_native.so` 验证：

- JNI 加载成功。
- Android 真机可通过 `adb reverse` 读取 PC 上的 Python RTSP 测试流。
- OpenCV/FFmpeg 真实检测链路跑通。
- overlay RTSP 输出链路跑通。
- 标定确认、坐标计算、坐标读取链路跑通。

该结果可作为“Android 真机 native/JNI/RTSP 基础链路已打通”的验收记录。后续现场验收仍需将 `input_url` 切换为真实相机或现场 RTSP 地址，并验证 AMR/Modbus 业务链路。
