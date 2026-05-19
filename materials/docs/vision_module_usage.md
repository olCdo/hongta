# 视觉模块使用说明

## 当前模块

- `CircleDetector`：统一圆形目标检测。入口通孔和中心喇叭口通过不同半径、置信度和圆周边缘支撑参数区分。
- `RtspReader`：PC 调试用 RTSP/视频读取封装，基于 OpenCV `VideoCapture`。
- `LatestFrameReader`：PC 调试用后台读取器，只保留最新帧，降低 RTSP 显示延迟。
- `detect_stream_demo`：当前唯一维护的 PC 实时调试工具，支持 USB 摄像头和 RTSP 输入。

正式 Android 方向是 native RTSP 输入/输出和 FFmpeg 解码/编码管线；本模块当前阶段先保留帧级算法和 PC 调试能力，不在 `detect_stream_demo` 中实现最终 Android 管线。

## 目录结构

```text
CMakeLists.txt
code/
  configs/
    center_horn_usb_debug.json
  include/
    honta/vision/
      circle_detector.h
      rtsp_reader.h
  src/
    vision/
      circle_detector.cpp
      rtsp_reader.cpp
  tools/
    detect_stream_demo.cpp
  third_party/
materials/
  docs/
  references/
  assets/reference/
```

## 构建

```powershell
cmd.exe /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S D:\WorkProject\honta -B D:\WorkProject\honta\build_nmake -G "NMake Makefiles" -DOpenCV_DIR=D:\opencv\build\x64\vc16\lib && "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build D:\WorkProject\honta\build_nmake'
```

构建目标：

- `honta_vision`
- `detect_stream_demo`

## 实时调试

USB 摄像头：

```powershell
$env:Path = "D:\opencv\build\x64\vc16\bin;$env:Path"
.\build_nmake\detect_stream_demo.exe --config code\configs\center_horn_usb_debug.json
```

RTSP 输入：

```powershell
$env:Path = "D:\opencv\build\x64\vc16\bin;$env:Path"
.\build_nmake\detect_stream_demo.exe `
  --config code\configs\center_horn_usb_debug.json `
  --input "rtsp://user:password@192.168.1.10:554/stream1"
```

按键：

- `q` 或 `ESC`：退出。
- `s`：保存当前圈注图；配置 `save_debug_stages=true` 时同时保存灰度、模糊、预处理和边缘图。

## 当前识别策略

当前统一使用圆形目标检测路径：

- 默认关闭全局直方图均衡。
- 默认关闭 CLAHE。
- 通过 `min_rim_edge_support`、半径范围和 Hough 阈值控制误识别。
- 已移除早期中心目标实验分支。

## 后续工作

下一阶段重点：

1. 设计 `honta_api.h` C ABI。
2. 设计 native FFmpeg RTSP 输入/输出管线，包含正式 RTSP 流和 debug RTSP 四宫格流。
3. 增加 Android NDK `arm64-v8a` 构建。
4. 基于真实补光样本重新标定默认参数。


