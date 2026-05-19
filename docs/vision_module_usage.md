# 视觉模块使用说明

## 1. 当前内容

当前已实现两个基础模块：

- `RtspReader`：基于 OpenCV `VideoCapture` 的 RTSP、本地视频读取模块。
- `CircleDetector`：入口通孔、中心喇叭口的圆形识别模块。

同时提供一个命令行工具：

- `detect_demo`：读取图片、本地视频或 RTSP 第一帧，执行圆形识别，并输出识别 JSON 和圈注图片。

## 2. 目录结构

```text
CMakeLists.txt
include/
  honta/
    vision/
      circle_detector.h
      rtsp_reader.h
src/
  vision/
    circle_detector.cpp
    rtsp_reader.cpp
tools/
  detect_demo.cpp
```

## 3. 编译依赖

需要：

- CMake 3.16+
- C++17 编译器
- OpenCV，至少包含：
  - `core`
  - `imgproc`
  - `imgcodecs`
  - `videoio`
  - `highgui`

Windows 示例：

```bash
cmake -S . -B build -DOpenCV_DIR=<opencv_build_dir>
cmake --build build --config Release
```

Linux 示例：

```bash
cmake -S . -B build
cmake --build build -j
```

## 4. detect_demo 用法

识别入口通孔图片：

```bash
detect_demo --input data/entrance.jpg --type entrance --output output_entrance.jpg
```

识别中心喇叭口图片：

```bash
detect_demo --input data/center.jpg --type center --output output_center.jpg
```

读取 RTSP 第一帧并识别：

```bash
detect_demo --input rtsp://user:password@192.168.1.10:554/stream1 --type entrance --output output_rtsp.jpg
```

输出 JSON 示例：

```json
{
  "success": true,
  "detections": [
    {
      "id": 1,
      "center_x": 512.3,
      "center_y": 384.6,
      "radius": 42.8,
      "confidence": 0.92,
      "type": "entrance_hole"
    }
  ]
}
```

## 5. 后续工作

下一步建议：

1. 准备真实入口通孔、中心喇叭口图片。
2. 根据图片效果调整 `CircleDetectorConfig` 参数。
3. 增加连续视频识别 demo。
4. 增加结果 JSON 文件输出。
5. 开始实现相机参数、顶盖数据和坐标计算模块。
