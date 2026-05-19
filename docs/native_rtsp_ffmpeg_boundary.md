# Native RTSP/FFmpeg 边界设计

## 当前结论

正式 Android 版本面向 RTSP 输入和 RTSP 输出，视频解码、编码和推流使用 FFmpeg。当前收束阶段不实现完整管线，只明确后续 native 模块边界，避免 PC 调试窗口逻辑进入正式架构。

算法层统一为圆形目标检测。入口通孔和中心喇叭口不再通过算法类型区分，而是通过半径、置信度、圆周边缘支撑等参数区分。

## 后续 native 管线

```text
RTSP input
  -> FFmpeg demux/decode
  -> latest-frame queue
  -> CircleDetector
  -> overlay renderer
  -> FFmpeg encode
  -> primary RTSP output
  -> debug compositor
  -> FFmpeg encode
  -> debug RTSP output
```

## 两路输出

- 正式 RTSP 输出：原始视频 + 识别圆圈 + 圆心 + 编号/置信度。
- Debug RTSP 输出：单独一路四宫格画面，仅在 debug 模式开启时输出。
- Debug 四宫格内容：`overlay`、`gray/blur`、`preprocess`、`edges`。
- Debug 流使用同一帧源和同一检测结果，避免与正式画面不同步。

## 与现有代码的关系

- `CircleDetector` 保持帧级算法能力，不依赖 RTSP、FFmpeg 或 Android UI。
- `detect_stream_demo` 只用于 PC 阶段 USB/RTSP 实时调试。
- `RtspReader` 是 PC 调试用 OpenCV 读取封装，不作为 Android 最终 RTSP 管线。
- 下一阶段通过 `honta_api.h` 暴露 C ABI，再接入 Android NDK `arm64-v8a` 构建。

## 下一阶段配置方向

```json
{
  "input_rtsp_url": "rtsp://...",
  "output_rtsp_url": "rtsp://...",
  "debug_rtsp_enabled": true,
  "debug_rtsp_url": "rtsp://...",
  "detection": {
    "min_radius_px": 50,
    "max_radius_px": 220,
    "min_confidence": 0.6,
    "min_rim_edge_support": 0.45
  }
}
```

## 非目标

- 本轮不编译 Android FFmpeg。
- 本轮不实现 RTSP 输出推流。
- 本轮不实现 JNI 封装。
