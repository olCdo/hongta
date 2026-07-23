# Native RTSP/FFmpeg 边界设计

## 当前结论

正式 Android 版本面向一路摄像头 RTSP 输入和动态库内部 RTSP 圈注输出，视频解码、编码和输出使用 FFmpeg。当前收束阶段不实现完整管线，只明确后续 native 模块边界，避免 PC 调试窗口逻辑进入正式架构。

算法层统一为圆形目标检测。入口通孔和中心喇叭口不再通过算法类型区分，而是通过半径、置信度、圆周边缘支撑等参数区分。

## 后续 native 管线

```text
RTSP input
  -> FFmpeg demux/decode
  -> latest-frame queue
  -> CircleDetector
  -> overlay renderer
  -> FFmpeg encode
  -> internal RTSP output service
  -> primary RTSP playback URL
  -> debug compositor
  -> FFmpeg encode
  -> internal debug RTSP output service
  -> debug RTSP playback URL
```

## 两路输出

- 正式 RTSP 输出：动态库内部发布的圈注流，内容为原始视频 + 识别圆圈 + 圆心 + 编号/置信度。
- Debug RTSP 输出：动态库内部发布的单独一路四宫格画面，仅在 debug 模式开启时输出。
- Debug 四宫格内容：`overlay`、`gray/blur`、`preprocess`、`edges`。
- Debug 流使用同一帧源和同一检测结果，避免与正式画面不同步。
- APP 播放动态库提供的 RTSP 地址，不需要传入输出 RTSP 地址。

## 与现有代码的关系

- `CircleDetector` 保持帧级算法能力，不依赖 RTSP、FFmpeg 或 Android UI。
- `detect_stream_demo` 只用于 PC 阶段 USB/RTSP 实时调试。
- `RtspReader` 是 PC 调试用 OpenCV 读取封装，不作为 Android 最终 RTSP 管线。
- 下一阶段通过 `honta_api.h` 暴露 C ABI，再接入 Android NDK `arm64-v8a` 构建。

## 下一阶段配置方向

```json
{
  "rtsp": {
    "input_url": "rtsp://192.168.1.10:554/stream1",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "192.168.1.20",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay",
    "debug_enabled": true,
    "debug_path": "/honta_debug",
    "debug_public_host": "192.168.1.20"
  },
  "detection": {
    "min_radius_px": 50,
    "max_radius_px": 220,
    "min_confidence": 0.6,
    "min_rim_edge_support": 0.45
  }
}
```

圈注 RTSP 播放地址由动态库根据 `overlay_public_host`、`overlay_port` 和 `overlay_path` 生成，并通过 `honta_get_overlay_rtsp_url` 返回给 APP。APP 不传完整输出 RTSP URL。

## 非目标

- 本轮不编译 Android FFmpeg。
- 本轮不完整实现库内 RTSP 输出服务。
- 本轮不实现 JNI 封装。


