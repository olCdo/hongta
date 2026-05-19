# 视觉识别现场调参文档

本文面向现场调试人员，说明 `detect_stream_demo` 的 JSON 配置、常用启动方式和调参顺序。

当前算法已经收束为统一圆检测。入口通孔和中心喇叭口都按圆处理，差异通过不同半径范围、置信度和圆周边缘支撑参数体现。

## 启动方式

USB 摄像头调试：

```powershell
$env:Path = "D:\opencv\build\x64\vc16\bin;$env:Path"
.\build_nmake\detect_stream_demo.exe --config configs\center_horn_usb_debug.json
```

临时覆盖摄像头编号：

```powershell
.\build_nmake\detect_stream_demo.exe --config configs\center_horn_usb_debug.json --camera 1
```

RTSP 调试：

```powershell
.\build_nmake\detect_stream_demo.exe `
  --config configs\center_horn_usb_debug.json `
  --input "rtsp://admin:admin@192.168.0.120:554/11"
```

命令行参数优先级高于 JSON：

```text
程序默认值 < JSON 配置 < 命令行参数
```

常用按键：

- `q` 或 `ESC`：退出。
- `s`：保存当前标注画面为 `stream_debug_帧号.jpg`。

## JSON 字段

`input`：

- `camera_index`：USB 摄像头编号。
- `camera_width` / `camera_height`：请求 USB 摄像头输出分辨率。
- `camera_fps`：请求 USB 摄像头帧率。
- `url`：RTSP 地址或本地视频路径；`camera_index` 和 `url` 只能配置一个。

`detection`：

- `min_radius_px` / `max_radius_px`：圆半径范围，单位为原始画面像素。
- `min_confidence`：最低置信度。
- `max_results`：最多显示结果数量。
- `enable_hist_equalization`：全局直方图均衡，默认关闭。
- `enable_clahe`：CLAHE 局部对比度增强，默认关闭。
- `clahe_clip_limit` / `clahe_tile_grid_size`：CLAHE 参数，仅在 `enable_clahe=true` 时生效。
- `max_radius_image_ratio`：最大半径占画面短边比例。
- `min_rim_edge_support`：圆周边缘支撑度，用于过滤墙面纹理、阴影、噪声凑出的假圆。
- `canny_high_threshold`：Canny 边缘检测高阈值。
- `hough_accumulator_threshold`：Hough 圆检测累加器阈值。

`runtime`：

- `process_scale`：检测前缩小画面的比例。
- `detect_every`：每隔几帧检测一次。
- `display_width`：显示窗口最大宽度。
- `latest_frame_mode`：RTSP 输入时只保留最新帧，降低延迟。
- `save_debug_stages`：按 `s` 时是否额外保存预处理中间图。

## 推荐初始配置

中心喇叭口 USB 调试建议从当前配置文件开始：

```json
{
  "min_radius_px": 50,
  "max_radius_px": 220,
  "max_results": 3,
  "max_radius_image_ratio": 0.22,
  "min_rim_edge_support": 0.45,
  "enable_hist_equalization": false,
  "enable_clahe": false,
  "canny_high_threshold": 120,
  "hough_accumulator_threshold": 25,
  "process_scale": 0.5,
  "detect_every": 2
}
```

入口通孔如果半径不同，应新建或切换另一套配置，优先调整 `min_radius_px`、`max_radius_px`、`min_confidence` 和 `min_rim_edge_support`。

## 圆周边缘支撑

算法猜出圆心和半径后，会沿圆周采样多个点，并检查这些点附近是否存在边缘。`min_rim_edge_support=0.45` 表示圆周上至少约 45% 的采样点要能找到边缘。

数值越高越严格，误识别更少，但真实目标边缘不完整时可能漏检。数值越低越宽松，更容易检出目标，也更容易把墙面纹理或阴影凑成假圆。

## 识别不到目标

按顺序处理：

1. 确认目标在画面中。
2. 目标较远或较小时，降低 `min_radius_px`。
3. 降低 `hough_accumulator_threshold`。
4. 降低 `min_rim_edge_support`。
5. 降低 `canny_high_threshold`。
6. 目标靠近摄像头且很大时，增大 `max_radius_px` 或 `max_radius_image_ratio`。
7. 如画面不卡，可把 `process_scale` 改为 `1.0` 排除降采样影响。

## 误识别太多

按顺序处理：

1. 提高 `min_rim_edge_support`。
2. 提高 `hough_accumulator_threshold`。
3. 提高 `min_confidence`。
4. 缩小 `max_radius_px` 或 `max_radius_image_ratio`。
5. 根据目标真实尺寸收紧 `min_radius_px` 和 `max_radius_px`。

## 延迟或卡顿

按顺序处理：

1. 将 `process_scale` 从 `0.5` 降到 `0.35`。
2. 将 `detect_every` 从 `2` 提高到 `3`。
3. 将 `display_width` 从 `960` 降到 `720`。
4. RTSP 输入时保持 `latest_frame_mode=true`。

## 调试图与后续 Debug RTSP

PC 调试阶段，当 `save_debug_stages=true` 时，实时窗口按 `s` 会保存：

```text
stream_debug_帧号.jpg
stream_debug_帧号_gray.jpg
stream_debug_帧号_blur.jpg
stream_debug_帧号_preprocess.jpg
stream_debug_帧号_edges.jpg
```

后续 native RTSP/FFmpeg 阶段会增加单独一路 debug RTSP，四宫格输出 `overlay`、`gray/blur`、`preprocess`、`edges`，不替换正式 RTSP 输出。
