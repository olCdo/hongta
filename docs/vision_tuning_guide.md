# 视觉识别现场调参文档

本文面向现场调试人员，说明 `detect_stream_demo` 的 JSON 配置、常用启动方式和调参顺序。

## 1. 启动方式

USB 摄像头调试：

```powershell
$env:Path = "D:\opencv\build\x64\vc16\bin;$env:Path"

.\build_nmake\detect_stream_demo.exe --config configs\center_horn_usb_debug.json
```

如果第 0 个摄像头打不开，可以临时覆盖摄像头编号：

```powershell
.\build_nmake\detect_stream_demo.exe --config configs\center_horn_usb_debug.json --camera 1
```

RTSP 调试时可以复用同一套识别参数，只覆盖输入源：

```powershell
.\build_nmake\detect_stream_demo.exe `
  --config configs\center_horn_usb_debug.json `
  --input "rtsp://admin:admin@192.168.0.120:554/11"
```

命令行参数优先级高于 JSON。优先级为：

```text
程序默认值 < JSON 配置 < 命令行参数
```

例如临时提高圆周边缘要求：

```powershell
.\build_nmake\detect_stream_demo.exe `
  --config configs\center_horn_usb_debug.json `
  --min-rim-edge-support 0.55
```

窗口快捷键：

- `q` 或 `ESC`：退出。
- `s`：保存当前标注画面为 `stream_debug_帧号.jpg`。

## 2. JSON 字段说明

示例配置文件：`configs/center_horn_usb_debug.json`。

### input

- `camera_index`：USB 摄像头编号。常见值为 `0`、`1`、`2`。
- `camera_width` / `camera_height`：请求 USB 摄像头输出分辨率。1080p 摄像头建议配置为 `1920` 和 `1080`。
- `camera_fps`：请求 USB 摄像头帧率，例如 `30`。实际值以程序启动时打印的摄像头信息为准。
- `url`：RTSP 地址或本地视频路径。

`camera_index` 和 `url` 只能配置一个。也可以在 JSON 中不写输入源，启动时用 `--camera` 或 `--input` 指定。

### detection

- `type`：识别目标类型。`center` 为中心喇叭口，`entrance` 为入口通孔，`generic` 为通用圆。
- `center_mode`：中心喇叭口识别模式。现场真实管口优先用 `rim`。
- `min_radius_px`：最小圆半径，单位为原始画面像素。调小可识别远处小目标，但误识别会增加。
- `max_radius_px`：最大圆半径，单位为原始画面像素。调小可过滤过大的背景圆。
- `enable_hist_equalization`：是否启用全局直方图均衡。空白墙误识别严重时建议先设为 `false`。
- `enable_clahe`：是否启用 CLAHE 局部对比度增强。它比全局均衡温和，但仍可能放大噪声，默认关闭。
- `clahe_clip_limit`：CLAHE 对比度限制，数值越大增强越强。
- `clahe_tile_grid_size`：CLAHE 分块大小，常用 `8`。
- `max_radius_image_ratio`：最大半径占画面短边的比例。实际最大半径为 `min(max_radius_px, 短边 * max_radius_image_ratio)`。
- `min_confidence`：最低置信度。调高可减少误识别，调低可减少漏检。
- `max_results`：最多显示几个结果。中心喇叭口通常设为 `1`。
- `min_rim_edge_support`：圆周边缘支撑度。数值越高越严格，是控制中心喇叭口误识别的关键参数。
- `canny_high_threshold`：边缘检测阈值。调低可发现弱边缘，调高可减少噪声边缘。
- `hough_accumulator_threshold`：霍夫圆检测阈值。调低更容易出圆，调高更严格。
- `roi`：识别区域，格式为 `[x, y, width, height]`，坐标基于原始画面。
- `require_circle_inside_roi`：为 `true` 时要求整个圆都在 ROI 内。
- `draw_roi`：是否显示蓝色 ROI 框。

### runtime

- `process_scale`：检测前缩小画面的比例。`0.5` 表示用 50% 尺寸检测，速度更快。
- `detect_every`：每隔几帧检测一次。`2` 表示每 2 帧检测一次，中间帧复用上次结果。
- `display_width`：显示窗口最大宽度，只影响显示，不影响识别坐标。
- `latest_frame_mode`：RTSP 输入时只保留最新帧，降低延迟。USB 摄像头输入时不使用该模式。
- `save_debug_stages`：按 `s` 保存调试图时，是否额外保存预处理中间图。

## 3. 推荐初始配置

中心喇叭口 USB 调试建议从以下参数开始：

```json
{
  "type": "center",
  "center_mode": "rim",
  "min_radius_px": 6,
  "max_radius_px": 220,
  "max_results": 1,
  "max_radius_image_ratio": 0.22,
  "min_rim_edge_support": 0.45,
  "canny_high_threshold": 120,
  "hough_accumulator_threshold": 22,
  "process_scale": 0.5,
  "detect_every": 2
}
```

USB 摄像头第一次调试要先看程序启动时打印的实际分辨率，以及窗口左上角显示的 `size=宽x高`。1080p 摄像头建议在 JSON 中配置 `camera_width=1920`、`camera_height=1080`，但最终是否成功取决于摄像头和驱动。

如果实际画面不是 1920x1080，需要按实际尺寸重新设置 ROI。很多 USB 摄像头默认会以 `640x480` 打开，如果直接使用 1080p 的 ROI，例如 `[560, 160, 800, 800]`，ROI 会大幅超出画面，导致检测结果一直为 0。

确认目标大致位置后，再调整 ROI，让目标尽量落在 ROI 内，减少背景干扰。

## 4. 识别不到目标

按顺序处理：

1. 确认目标在画面中，且 ROI 覆盖目标可能出现区域。
2. 如果目标较远或较小，降低 `min_radius_px`，例如从 `6` 降到 `4`。
3. 降低 `hough_accumulator_threshold`，例如从 `22` 降到 `18`。
4. 降低 `min_rim_edge_support`，例如从 `0.45` 降到 `0.35`。
5. 降低 `canny_high_threshold`，例如从 `120` 降到 `80`。
6. 如果目标靠近摄像头且很大，增大 `max_radius_px` 或 `max_radius_image_ratio`。
7. 如果画面不卡，可以把 `process_scale` 改为 `1.0`，排除降采样导致的小目标丢失。

宽松识别配置示例：

```json
{
  "min_radius_px": 4,
  "hough_accumulator_threshold": 18,
  "min_rim_edge_support": 0.35,
  "canny_high_threshold": 80,
  "process_scale": 1.0
}
```

## 5. 误识别太多

按顺序处理：

1. 缩小 `roi`，只覆盖目标可能出现区域。
2. 设置 `require_circle_inside_roi` 为 `true`。
3. 提高 `min_rim_edge_support`，例如从 `0.45` 提高到 `0.55`。
4. 提高 `hough_accumulator_threshold`，例如从 `22` 提高到 `30`。
5. 提高 `min_confidence`，例如从 `0.55` 提高到 `0.65`。
6. 缩小 `max_radius_px` 或 `max_radius_image_ratio`，过滤过大的背景圆。

保守识别配置示例：

```json
{
  "min_confidence": 0.65,
  "hough_accumulator_threshold": 30,
  "min_rim_edge_support": 0.55,
  "require_circle_inside_roi": true,
  "max_results": 1
}
```

## 6. 画面卡顿或延迟高

按顺序处理：

1. 将 `process_scale` 从 `0.5` 降到 `0.35`。
2. 将 `detect_every` 从 `2` 提高到 `3`。
3. 将 `display_width` 从 `960` 降到 `720`。
4. RTSP 输入时保持 `latest_frame_mode` 为 `true`。

流畅优先配置示例：

```json
{
  "process_scale": 0.35,
  "detect_every": 3,
  "display_width": 720,
  "latest_frame_mode": true
}
```

## 7. ROI 调整方法

ROI 格式为：

```json
"roi": [x, y, width, height]
```

坐标原点在画面左上角：

- `x`：ROI 左上角横坐标。
- `y`：ROI 左上角纵坐标。
- `width`：ROI 宽度。
- `height`：ROI 高度。

调试时先设置较大的 ROI，确保目标能被框住；确认目标位置稳定后逐步缩小 ROI。若出现 ROI 边缘误识别，开启：

```json
"require_circle_inside_roi": true
```

如果不想显示蓝色 ROI 框，将：

```json
"draw_roi": false
```

## 8. 圆环边缘验证

圆环边缘验证用于减少空白墙、弱纹理、视频噪声造成的假圆，但它对真实目标的光照、材质和边缘清晰度很敏感。当前阶段建议把它作为实验项，不建议默认开启；先保证真实喇叭口能稳定识别，再逐步尝试打开。

相关 JSON 参数：

```json
{
  "enable_ring_validation": false,
  "min_ring_contrast": 6.0,
  "min_ring_edge_strength": 15.0,
  "min_ring_gradient_alignment": 0.30
}
```

参数含义：

- `enable_ring_validation`：是否启用圆环验证。真实目标识别不稳定时保持 `false`。
- `min_ring_contrast`：圆环内外灰度差要求。数值越高，越能过滤空白墙；太高会漏掉低对比目标。
- `min_ring_edge_strength`：圆周附近 Sobel 边缘强度要求。数值越高，越要求目标边缘清晰。
- `min_ring_gradient_alignment`：边缘方向和圆半径方向的一致性要求。真实管口边缘通常更符合这个条件，墙面噪声通常不稳定。

如果真实目标已经能稳定识别，但空白墙仍然误识别，可以尝试开启圆环验证并按顺序调严：

```json
{
  "enable_ring_validation": true,
  "hough_accumulator_threshold": 40,
  "min_rim_edge_support": 0.65,
  "min_ring_contrast": 14.0,
  "min_ring_edge_strength": 35.0,
  "min_ring_gradient_alignment": 0.55,
  "max_radius_image_ratio": 0.12
}
```

如果真实目标识别不到，先关闭圆环验证，再按顺序放宽：

```json
{
  "enable_ring_validation": false,
  "min_ring_contrast": 6.0,
  "min_ring_edge_strength": 15.0,
  "min_ring_gradient_alignment": 0.30,
  "hough_accumulator_threshold": 25,
  "min_rim_edge_support": 0.45
}
```

调试建议：先以真实喇叭口稳定识别为第一目标。空白墙误识别优先通过缩小 ROI、限制半径范围、提高 `hough_accumulator_threshold` 和 `min_rim_edge_support` 处理；圆环验证只在这些手段仍不够时再开启。

## 9. 预处理调试图

当 `save_debug_stages` 为 `true` 时，实时窗口按 `s` 会保存：

```text
stream_debug_帧号.jpg
stream_debug_帧号_gray.jpg
stream_debug_帧号_blur.jpg
stream_debug_帧号_preprocess.jpg
stream_debug_帧号_edges.jpg
```

文件含义：

- `gray`：原始灰度图，用来看摄像头本身是否有明显噪声、条纹或光照不均。
- `blur`：高斯模糊后的灰度图，用来看模糊是否已经压掉细碎噪声。
- `preprocess`：最终送入 Hough/Canny 的图。如果开启全局均衡或 CLAHE，这张图会体现增强后的效果。
- `edges`：Canny 边缘图。Hough 圆检测主要依赖这些边缘。

判断方法：

- 如果 `gray` 和 `blur` 很干净，但 `preprocess` 出现大量墙面纹理，说明直方图均衡或 CLAHE 放大了噪声。
- 如果 `preprocess` 看起来正常，但 `edges` 在空白墙上有大量白色边缘，说明 Canny 阈值偏低或预处理过敏。
- 如果 `edges` 很干净但仍然出圆，说明 Hough 阈值、半径范围或 ROI 约束需要收紧。
- 如果目标在 `edges` 中没有清楚圆边，说明目标边缘本身太弱，应优先改善光照、焦距或降低 `canny_high_threshold`。

排查空白墙误识别时，建议先用：

```json
{
  "enable_hist_equalization": false,
  "enable_clahe": false,
  "save_debug_stages": true
}
```

如果真实目标边缘太弱，再尝试：

```json
{
  "enable_clahe": true,
  "clahe_clip_limit": 2.0,
  "clahe_tile_grid_size": 8
}
```
