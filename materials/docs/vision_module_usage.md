# 视觉模块使用说明

## 当前模块

- `CircleDetector`：统一圆形目标检测。入口通孔和中心喇叭口通过不同半径、置信度和圆周边缘支撑参数区分。
- `RtspReader`：PC 调试用 RTSP/视频读取封装，基于 OpenCV `VideoCapture`。
- `LatestFrameReader`：PC 调试用后台读取器，只保留最新帧，降低 RTSP 显示延迟。
- `detect_stream_demo`：当前唯一维护的 PC 实时调试工具，支持 USB 摄像头和 RTSP 输入。

正式 Android 方向是 C++ 动态库内部接入 RTSP，使用 FFmpeg 解码，并通过后台识别服务维护候选编号和圈注画面；APP 不走逐帧传图主链路，也不需要获取识别候选 JSON。APP 只把用户选择的候选编号传回动态库，由动态库保存标定状态并计算坐标。本模块当前阶段先保留 PC 调试能力，不在 `detect_stream_demo` 中实现最终 Android 管线。

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

1. 基于真实补光样本重新标定入口通孔和中心喇叭口默认参数。
2. 固定相机参数、顶盖数据和坐标系定义。
3. 设计 `honta_api.h` C ABI，包含开始识别、停止识别、入口候选确认、中心候选确认、全部喇叭口坐标计算和按喇叭口序号查询坐标。
4. 设计 native FFmpeg RTSP 输入和后台识别服务。
5. 增加 Android NDK `arm64-v8a` 构建。


