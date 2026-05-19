# Honta 视觉动态库项目上下文恢复摘要

更新时间：2026-05-19  
工作目录：`D:\WorkProject\honta`

本文用于下次对话重新加载项目上下文，记录当前关键决策、已完成内容、待办事项、重要文件修改记录和整体架构思路。

## 1. 项目目标

本项目目标是实现一个用于 Android 端集成的 C++ 视觉识别动态库/模块，核心能力是识别顶盖相关圆形目标，并输出识别结果给 Android App 后续进行用户确认、坐标计算和 AMR 任务下发。

当前业务流程概括：

1. 用户在 App 中选择顶盖型号。
2. App 获取现场视频流，当前确认输入主要走 RTSP，调试阶段保留 USB 摄像头输入。
3. 视频解码使用 FFmpeg。
4. C++ 视觉模块处理视频帧，识别入口通孔、中心喇叭口等圆形目标。
5. 客户需要看到识别结果圈在画面上。
6. 推荐正式显示方式是：C++ 输出识别结果坐标，App 在视频画面上叠加画圈。
7. 调试阶段可保留 C++ 生成的调试图，例如边缘图、预处理图、圈注图。
8. 后续坐标计算模块根据入口通孔、中心喇叭口、AMR 位姿、顶盖型号等数据，计算全部喇叭口在 AMR 地图中的坐标。

## 2. 当前关键决策

### 2.1 输入与输出方向

- 最终 Android 集成方向：输入和输出都面向 RTSP 流方案，解码使用 FFmpeg。
- USB 摄像头输入暂时保留，用于 PC 阶段调试算法。
- C++ 算法核心不应该依赖 Android UI，也不应该长期绑定 PC 窗口显示。
- 客户要看圈注结果，但推荐由 App 根据 C++ 返回的圆心、半径、置信度自行在视频画面上画圈。
- C++ 返回整张 RGBA8888 圈注图只建议作为调试能力，不建议作为正式每帧主输出。

### 2.2 Android 动态库边界

重要结论：

- 正式 Android 版本由 C++ native 动态库负责 RTSP 接入、FFmpeg 解码、圈注和 RTSP 输出；Android App 负责配置、控制、显示、用户交互和业务流程。
- C++ 算法模块负责处理解码后的帧，输出识别结果。
- native RTSP/FFmpeg pipeline 作为下一阶段单独设计；当前 PC demo 的窗口逻辑不进入最终架构。

建议接口方向：

```cpp
int honta_detect_frame(
    const unsigned char* image_data,
    int width,
    int height,
    int stride,
    int pixel_format,
    const char* config_json,
    char* result_json,
    int result_json_size
);
```

## 11. 2026-05-19 收束后决策

- 当前工作分支：`codex/converge-vision-module`。
- 项目结构已按轻量工程化整理：原始资料放入 `references/`，参考图片放入 `assets/reference/`。
- PC 阶段只维护 `detect_stream_demo`，用于 USB 摄像头和 RTSP 实时调试。
- 中心喇叭口识别固定走外圆 rim 边缘路径，默认关闭全局直方图均衡和 CLAHE。
- 旧的单帧/首帧调试入口和中心喇叭口实验分支已从当前主路径移除。
- 正式 Android 方向为 native RTSP 输入/输出与 FFmpeg 解码/编码/推流。
- 本轮不实现完整 native FFmpeg 管线；下一阶段先设计 `honta_api.h`、native pipeline 生命周期接口和 Android NDK `arm64-v8a` 构建。

调试图按需输出，不建议每帧默认输出：

```cpp
int honta_get_debug_image(
    int debug_type,
    int output_format,
    unsigned char* output_data,
    int output_width,
    int output_height,
    int output_stride
);
```

### 2.3 识别算法方向

当前识别仍以传统 OpenCV 圆检测为主：

```text
输入帧
  -> 灰度化
  -> 高斯模糊
  -> 可选预处理增强
  -> Canny 边缘
  -> Hough 圆检测
  -> 轮廓 fallback
  -> ROI/半径/边缘支撑过滤
  -> 合并排序
  -> 输出候选圆
```

已验证的重要结论：

- 空白墙误识别严重的主要原因与全局直方图均衡有关。
- 关闭全局直方图均衡后，误识别明显减少或消失。
- CLAHE 对误识别影响比全局均衡小，但当前也不需要默认开启。
- 当前稳定策略应是：默认关闭全局直方图均衡和 CLAHE。
- 边缘图中圆不完整、只看到圆弧是正常现象；Hough 可以用局部圆弧推断圆。
- 影子可能被识别，后续计划通过补光解决。

### 2.4 已试验但不建议默认启用的能力

- `generic` 通用圆和 `entrance` 入口识别仍可用于调试/业务扩展，但当前调试重点是 `center` 中心喇叭口。

## 3. 当前技术栈与构建方式

### 3.1 技术栈

- 语言：C++17
- 构建：CMake
- 编译器：Visual Studio Build Tools 2026，MSVC x64
- OpenCV：官方预编译包，位于 `D:\opencv`
- JSON：`nlohmann/json` 单头文件，位于 `third_party/nlohmann/json.hpp`
- FFmpeg：本机已有，路径示例：
  - `D:\FFmpeg\ffmpeg-2025-05-19-git-c55d65ac0a-essentials_build\bin\ffmpeg.exe`
  - `D:\FFmpeg\ffmpeg-2025-05-19-git-c55d65ac0a-essentials_build\bin\ffprobe.exe`

### 3.2 当前构建命令

推荐 NMake 构建：

```powershell
cmd.exe /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S D:\WorkProject\honta -B D:\WorkProject\honta\build_nmake -G "NMake Makefiles" -DOpenCV_DIR=D:\opencv\build\x64\vc16\lib && "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build D:\WorkProject\honta\build_nmake'
```

增量编译：

```powershell
cmd.exe /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 && "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build D:\WorkProject\honta\build_nmake'
```

运行前设置 OpenCV DLL 路径：

```powershell
$env:Path = "D:\opencv\build\x64\vc16\bin;$env:Path"
```

## 4. 已完成内容

### 4.1 工程结构

已新增/维护 CMake 工程：

- `CMakeLists.txt`

当前构建目标：

- `honta_vision` 静态库
- `detect_stream_demo` 实时流测试工具


### 4.2 RTSP/视频读取模块

文件：

- `include/honta/vision/rtsp_reader.h`
- `src/vision/rtsp_reader.cpp`

当前能力：

- `RtspReader`：基于 OpenCV `VideoCapture` 读取 RTSP/视频流，支持重连、低延迟配置、buffer size。
- `LatestFrameReader`：后台线程持续读取，只保留最新帧，用于降低 RTSP 显示延迟。

后续方向：

- PC demo 可继续使用该模块调试。
- Android 正式环境如果决定 native 内部解码，应重新评估 FFmpeg 直接解码方案，而不是依赖 OpenCV `VideoCapture`。

### 4.3 圆形识别模块

文件：

- `include/honta/vision/circle_detector.h`
- `src/vision/circle_detector.cpp`

当前核心能力：

- 灰度化
- 高斯模糊
- 可选全局直方图均衡
- 可选 CLAHE
- Hough 圆检测
- 轮廓 fallback
- 暗色区域检测实验分支
- ROI 过滤
- 圆周边缘支撑度过滤
- 候选合并和排序
- 圈注图绘制
- 预处理调试图生成

重要配置结构：

```cpp
struct CircleDetectorConfig {
    int min_radius_px = 12;
    int max_radius_px = 320;
    double min_confidence = 0.55;
    double dp = 1.2;
    double min_dist_px = 45.0;
    double canny_high_threshold = 120.0;
    double hough_accumulator_threshold = 28.0;
    int gaussian_kernel_size = 7;
    bool enable_hist_equalization = true;
    bool enable_clahe = false;
    double clahe_clip_limit = 2.0;
    int clahe_tile_grid_size = 8;
    bool enable_contour_fallback = true;
    int max_results = 20;
    double min_circularity = 0.65;
    double min_fill_ratio = 0.55;
    double max_radius_image_ratio = 0.22;
    double min_rim_edge_support = 0.24;
    cv::Rect roi{};
    bool require_circle_inside_roi = false;
    bool draw_roi = true;
};
```


### 4.4 实时调试工具

文件：

- `tools/detect_stream_demo.cpp`

当前能力：

- 支持 `--camera <index>` USB 摄像头输入。
- 支持 `--input <rtsp_url>` RTSP 输入。
- 支持 JSON 配置：`--config configs\center_horn_usb_debug.json`
- 支持降采样检测：`process_scale`
- 支持间隔帧检测：`detect_every`
- 支持显示宽度控制：`display_width`
- 支持 ROI 显示和过滤。
- 按 `q` 或 ESC 退出。
- 按 `s` 保存当前标注图。
- `save_debug_stages=true` 时保存：
  - `stream_debug_<frame>_gray.jpg`
  - `stream_debug_<frame>_blur.jpg`
  - `stream_debug_<frame>_preprocess.jpg`
  - `stream_debug_<frame>_edges.jpg`

当前 USB 调试配置：

- `configs/center_horn_usb_debug.json`

该配置已按最新实测结论设置为默认关闭全局直方图均衡和 CLAHE：

```json
"enable_hist_equalization": false,
"enable_clahe": false
```

### 4.5 单帧测试工具

文件：


当前能力：

- 读取图片、本地视频或 RTSP 第一帧。
- 输出识别摘要或 JSON。
- 保存圈注图。

后续方向：

- 用户已提出算法收束，输入只留 USB 和 RTSP，最终面向 Android 动态库。

### 4.6 文档

当前已有文档：

- `docs/android_collaboration_dev_doc.md`
- `docs/android_collaboration_dev_doc_word版.docx`
- `docs/cpp_dynamic_library_implementation_plan.md`
- `docs/vision_module_usage.md`
- `docs/vision_tuning_guide.md`
- `docs/project_context_reload_summary.md`

`docs/vision_tuning_guide.md` 已加入：

- USB/RTSP 调试说明
- JSON 字段说明
- 空白墙误识别分析
- ROI 调整方法
- 预处理调试图说明

该文档后续需要根据最新“输入/输出 RTSP、FFmpeg 解码、Android 动态库边界”重新收束。

## 5. 近期关键经验与问题定位

### 5.1 空白墙误识别原因

实测结论：

- 全局直方图均衡对误识别影响很大。
- CLAHE 对误识别影响较小，但当前也无需默认开启。
- 关闭全局直方图均衡和 CLAHE 后，误识别明显改善，识别比较稳定。
- 当前误识别更多来自预处理增强放大墙面噪声，而不是单纯 Hough 参数问题。

### 5.2 边缘图只有圆弧的原因

`edges` 图中圆不完整是正常的：

- 真实管口边缘受光照、阴影、反光、角度影响，不会 360 度都清晰。
- Canny 只保留梯度足够强的局部边缘。
- Hough 圆检测不要求完整闭合圆，足够多圆弧就能推断圆。
- 后续补光应能让圆弧更连续。

### 5.3 影子识别

当前还可能识别到影子。用户计划后续通过补光改善。算法层面可以后续考虑：

- 缩小 ROI。
- 限制半径范围。
- 提高 Hough 阈值。
- 提高圆周边缘支撑度。
- 结合目标真实安装位置做动态 ROI。

## 6. 待办事项

### 6.1 架构收束待办

- 明确 Android 端最终模块边界：
  - C++ native 动态库负责 RTSP 输入/输出和 FFmpeg 解码。
  - 帧级接口保留为算法内部边界和调试能力。
  - 下一阶段设计 C++ native FFmpeg 解码、编码和推流管线。
- 如果确定“输入输出都用 RTSP 流，解码用 FFmpeg”在 native 层实现，需要设计 FFmpeg pipeline：
  - RTSP 输入解码
  - 帧队列
  - 算法处理
  - 圈注叠加
  - RTSP 输出编码/推流
- Android App 不承担正式 RTSP 解码/推流主链路，只负责配置、状态展示和业务交互。

### 6.2 算法收束待办

- 默认关闭：
  - `enable_hist_equalization`
  - `enable_clahe`
- 考虑移除或归档以下实验分支：
  - 暗色区域检测
- 将中心喇叭口识别收束为 `rim` 外圆边缘模式。
- 保留预处理调试图能力，便于现场排查。
- 基于真实补光后的样本重新调整默认参数。

### 6.3 Android 动态库接口待办

- 设计 `honta_api.h`。
- 定义 C ABI，避免暴露 C++ ABI。
- 定义输入图像格式：
  - 调试阶段可支持 `RGBA8888`
  - 后续性能优化建议支持 `GRAY8`、`NV21` 或 `YUV_420_888`
- 定义输出：
  - 正式输出识别 JSON 或结构体。
  - 客户显示由 App 画 overlay 圆。
  - 调试图按需输出，不默认每帧输出整张 RGBA 图。
- 设计错误码、初始化、释放、配置加载接口。
- 实现 Android NDK `arm64-v8a` 构建。

### 6.4 RTSP/FFmpeg 待办

- 明确 RTSP 输出责任归属：
  - App 层输出
  - native C++ 输出
  - 外部服务输出
- 如果 native 输出 RTSP，需要确认 Android 上可用的 FFmpeg 构建和 RTSP 推流方式。
- 如需外部 RTSP server，评估 MediaMTX 或现场已有 RTSP server。
- 确认输出画面：
  - 客户主画面：原视频 + 识别圈注
  - 工程调试画面：edges 或 preprocess

### 6.5 测试样本待办

- 采集真实工况样本：
  - 黑色金属喇叭口
  - AMR 补光开启
  - 无环境光或弱环境光
  - 不同距离和角度
  - 有影子和无影子对比
- 保存对应调试图：
  - 原图
  - gray
  - blur
  - preprocess
  - edges
  - annotated
- 建立批量测试工具或测试集统计：
  - 成功率
  - 误检率
  - 漏检率
  - 定位偏差

## 7. 重要文件修改记录

### 7.1 工程文件

- `CMakeLists.txt`
  - 定义 C++17 项目。
  - 查找 OpenCV。
  - 为 `detect_stream_demo` 增加 `third_party` include，用于 `nlohmann/json`。

### 7.2 头文件

- `include/honta/vision/circle_detector.h`
  - 定义 `DetectionTarget`。
  - 定义 `CircleDetectorConfig`。
  - 定义 `CircleDetection`。
  - 新增 `PreprocessDebugImages`。
  - 新增 `buildPreprocessDebugImages` 接口。

- `include/honta/vision/rtsp_reader.h`
  - 定义 `RtspReaderConfig`。
  - 定义 `RtspReader`。
  - 定义 `LatestFrameReader`。

### 7.3 实现文件

- `src/vision/circle_detector.cpp`
  - 实现灰度化、模糊、可选均衡、可选 CLAHE。
  - 实现 Hough 圆检测。
  - 实现轮廓 fallback。
  - 实现暗色区域检测实验分支。
  - 实现 ROI 过滤。
  - 实现圆周边缘支撑过滤。
  - 实现调试图生成。

- `src/vision/rtsp_reader.cpp`
  - 实现 RTSP 打开、读取、重连。
  - 实现后台最新帧读取。

### 7.4 工具文件

- `tools/detect_stream_demo.cpp`
  - 支持 `--camera <index>`。
  - 支持 `--input <rtsp_url>`。
  - 支持 `--config <json>`。
  - 支持 JSON 参数覆盖。
  - 支持 USB 摄像头分辨率和 FPS 请求。
  - 支持窗口状态叠加显示。
  - 支持按 `s` 保存标注图和预处理阶段图。

  - 单帧/首帧测试工具。
  - 后续建议移除或归档。

### 7.5 配置与第三方

- `configs/center_horn_usb_debug.json`
  - USB 摄像头调试配置。
  - 当前已设置 `enable_hist_equalization=false`、`enable_clahe=false`。

- `third_party/nlohmann/json.hpp`
  - 用于解析 JSON 配置。

### 7.6 调试产物

当前工作区存在调试输出：

- `stream_debug_*.jpg`
- `stream_debug_*_gray.jpg`
- `stream_debug_*_blur.jpg`
- `stream_debug_*_preprocess.jpg`
- `stream_debug_*_edges.jpg`

这些是调试产物，后续应加入 `.gitignore` 或清理。

## 8. 整体架构思路

### 8.1 当前 PC 调试架构

```text
USB 摄像头 / RTSP 输入
  |
  v
detect_stream_demo
  |
  +-- OpenCV VideoCapture / RtspReader
  |
  v
cv::Mat frame
  |
  v
CircleDetector
  |
  +-- 灰度化/模糊/边缘
  +-- Hough/轮廓候选
  +-- ROI/半径/边缘支撑过滤
  |
  v
识别结果 + 本地窗口显示 + 调试图保存
```

### 8.2 推荐 Android 分层架构

```text
Android App
  |
  +-- RTSP 输入/USB 输入
  +-- FFmpeg 解码
  +-- 视频预览
  +-- 用户交互
  |
  v
解码后的视频帧
  |
  v
C++ 动态库 / honta vision
  |
  +-- 单帧图像识别
  +-- 输出圆心、半径、置信度、候选列表
  +-- 可选输出调试图
  |
  v
Android App
  |
  +-- 在视频画面上画圈
  +-- 显示候选/确认结果
  +-- 可选 RTSP 输出带圈画面
  +-- 后续坐标计算和 AMR 任务下发
```

### 8.3 如果 native 层负责 RTSP 输入输出

如果后续确认 C++ 动态库必须直接处理 RTSP 输入输出，架构会变为：

```text
C++ native module
  |
  +-- FFmpeg RTSP 解码
  +-- 帧缓存/最新帧读取
  +-- CircleDetector 识别
  +-- 圈注叠加
  +-- FFmpeg 编码/RTSP 输出
  |
  v
Android App 控制启动、停止、配置和显示状态
```

这条路线复杂度更高，需要重点处理：

- Android NDK 下 FFmpeg 编译和授权。
- RTSP 网络异常和重连。
- 编码延迟。
- 线程生命周期。
- App 与 native 的控制接口。

## 9. 下次接续建议

下次继续开发时，建议按以下顺序：

1. 最终架构已确认：RTSP 输入输出和 FFmpeg 解码/编码/推流由 C++ native 动态库内部负责。
2. 收束算法默认路径：
   - 中心喇叭口使用 `rim`
   - 关闭全局均衡
   - 关闭 CLAHE
4. 设计 `honta_api.h` 动态库 C ABI。
5. 明确 App overlay 画圈协议，即 C++ 输出 JSON 字段格式。
6. 下一阶段单独设计 native RTSP 输出推流方案。
7. 使用真实补光样本重新标定默认参数。

下次对话可直接说：

```text
读取 docs/project_context_reload_summary.md，继续 Honta 视觉动态库项目。
```

## 10. 当前 Git 状态提醒

当前仓库存在大量未跟踪文件和部分已修改文件：

- `readme.md` 已修改。
- `CMakeLists.txt`、`docs/`、`include/`、`src/`、`tools/`、`configs/`、`third_party/` 等多为新增内容。
- `build/`、`build_nmake/` 是构建产物。
- `stream_debug_*.jpg`、`output_*.jpg` 是调试产物。
- `render_tmp/...` 存在权限警告，可忽略或后续清理。

后续应补充 `.gitignore`，至少忽略：

```text
build/
build_nmake/
stream_debug_*.jpg
output_*.jpg
.codex_tmp/
render_tmp/
*.obj
```

