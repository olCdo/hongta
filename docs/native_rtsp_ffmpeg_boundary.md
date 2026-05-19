# Native RTSP/FFmpeg 边界设计

## 当前结论

正式 Android 版本面向 RTSP 输入和 RTSP 输出，视频解码、编码和推流使用 FFmpeg。当前收束阶段不实现完整管线，只明确后续 native 模块边界，避免 PC 调试窗口逻辑进入正式架构。

## 后续 native 管线

```text
RTSP input
  -> FFmpeg demux/decode
  -> latest-frame queue
  -> CircleDetector
  -> overlay renderer
  -> FFmpeg encode
  -> RTSP output
```

## 与现有代码的关系

- `CircleDetector` 保持帧级算法能力，不依赖 RTSP、FFmpeg 或 Android UI。
- `detect_stream_demo` 只用于 PC 阶段 USB/RTSP 实时调试。
- `RtspReader` 是 PC 调试用 OpenCV 读取封装，不作为 Android 最终 RTSP 管线。
- 下一阶段通过 `honta_api.h` 暴露 C ABI，再接入 Android NDK `arm64-v8a` 构建。

## 下一阶段接口方向

- 初始化和释放 native pipeline。
- 设置输入 RTSP URL、输出 RTSP URL 和算法配置。
- 启动、停止、查询状态。
- 按需获取最新识别结果 JSON。
- 按需获取调试图，不作为正式每帧主输出。

## 非目标

- 本轮不编译 Android FFmpeg。
- 本轮不实现 RTSP 输出推流。
- 本轮不实现 JNI 封装。
