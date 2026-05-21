# 第二阶段计划书：Native RTSP/FFmpeg 输入与圈注输出管线

## 1. 阶段目标

第二阶段目标是让 C++ 动态库承担 Android 主链路中的视频输入、识别和圈注输出责任。动态库内部从唯一一路摄像头 RTSP 拉流，使用 FFmpeg 解码为算法可处理的图像帧，后台持续调用现有 `CircleDetector` 做圆形目标识别，并通过动态库内部 RTSP 输出服务发布带圈注和候选编号的正式播放流，供 APP 播放和用户选择。

本阶段只处理识别链路，不实现标定坐标解算和完整业务闭环。入口通孔与中心喇叭口不做两套算法，也不引入模型文件；统一使用同一个 `CircleDetector`，通过两套检测参数 Profile 和当前会话的 `detect_type` 区分。

## 2. 技术方案

### 2.1 总体管线

```text
camera RTSP input
  -> FFmpeg demux/decode
  -> frame queue / latest frame
  -> CircleDetector
  -> candidate cache
  -> overlay renderer
  -> FFmpeg encode
  -> internal RTSP output service
  -> APP plays overlay RTSP URL
```

Debug 模式可预留第二路输出：

```text
same decoded frame + same detection result
  -> debug compositor: overlay / gray-blur / preprocess / edges
  -> FFmpeg encode
  -> internal debug RTSP output service
```

正式输出流必须优先完成；Debug 输出可以作为可选项，不阻塞第二阶段主验收。

### 2.2 识别类型与 Profile 切换

第一版固定支持两个识别类型：

- `entrance_hole`：入口通孔识别。
- `center_horn`：中心喇叭口识别。

切换规则：

- `honta_start_detect("entrance_hole")` 创建入口通孔识别会话，使用入口通孔 Profile。
- `honta_start_detect("center_horn")` 创建中心喇叭口识别会话，使用中心喇叭口 Profile。
- 运行中的会话不支持热切换 `detect_type`。
- 从入口通孔切换到中心喇叭口时，必须先 `honta_stop_detect()`，再 `honta_start_detect("center_horn")`。
- 每次启动新会话后，`candidate_id` 从 1 重新编号，只在当前会话内有效。
- 确认候选时必须校验候选记录中的 `detect_type`，避免把入口通孔候选用于中心喇叭口确认，或反向误用。

推荐配置结构：

```json
{
  "detection_profiles": {
    "entrance_hole": {
      "min_radius_px": 40,
      "max_radius_px": 180,
      "min_confidence": 0.60,
      "min_rim_edge_support": 0.45,
      "enable_clahe": true
    },
    "center_horn": {
      "min_radius_px": 80,
      "max_radius_px": 260,
      "min_confidence": 0.55,
      "min_rim_edge_support": 0.35,
      "enable_clahe": false
    }
  }
}
```

未配置字段使用 `CircleDetectorConfig` 默认值。未知 `detect_type`、缺失 Profile、运行中重复启动识别会话都必须返回错误并写入 `last_error`。

### 2.3 RTSP 配置

推荐配置结构：

```json
{
  "rtsp": {
    "input_url": "rtsp://192.168.1.10:554/stream1",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "192.168.1.20",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay",
    "debug_enabled": false,
    "debug_path": "/honta_debug"
  }
}
```

说明：

- `input_url` 是摄像头原始 RTSP 地址。
- `overlay_bind_ip` 是动态库内部 RTSP 输出服务监听地址。
- `overlay_public_host` 是 APP 可访问到的设备 IP，多网卡环境必须由配置明确。
- `overlay_port` 和 `overlay_path` 组成正式圈注流地址。
- 动态库通过 `honta_get_overlay_rtsp_url` 返回播放地址，例如 `rtsp://192.168.1.20:8554/honta_overlay`。
- APP 不传完整输出 RTSP URL，只播放动态库返回的地址。

## 3. 模块设计

### 3.1 FFmpeg 输入模块

职责：

- 使用 FFmpeg C API 打开输入 RTSP。
- 配置打开超时、读超时、低延迟参数和传输协议。
- 查找视频流并初始化 decoder。
- 解码 `AVPacket` 到 `AVFrame`。
- 使用 `sws_scale` 转换为 OpenCV BGR `cv::Mat`。
- 向上层返回结构化错误，不能在底层无限阻塞。

建议内部类：

- `FfmpegRtspInput`
- `FfmpegRtspInputConfig`
- `DecodedFrame`

### 3.2 内部 RTSP 输出模块

职责：

- 接收 overlay 后的 BGR 帧。
- 使用 FFmpeg 编码为 H.264，第一版建议像素格式固定为 `yuv420p`。
- 发布正式圈注 RTSP 流。
- 管理监听地址、端口、路径、客户端连接和停止释放。
- 输出服务启动失败时返回明确错误，例如端口占用、地址无效、编码器初始化失败。

建议内部类：

- `InternalRtspOutputService`
- `FfmpegOverlayEncoder`
- `OverlayRtspConfig`

### 3.3 识别服务模块

职责：

- 管理单个后台识别线程。
- 保存当前 `DetectionSession`。
- 根据 `detect_type` 选择 `DetectionProfile` 并创建 `CircleDetector`。
- 循环执行：读取帧、识别、更新候选缓存、渲染圈注、推送输出帧。
- 支持开始、停止、错误状态和有限重连。

建议状态：

```text
Idle
Starting
Running
Reconnecting
Error
Stopping
```

### 3.4 候选缓存

建议结构：

```cpp
struct DetectionCandidate {
    int candidate_id;
    std::string detect_type;
    double center_x;
    double center_y;
    double radius;
    double confidence;
    int frame_id;
    int64_t timestamp_ms;
};

struct DetectionSession {
    std::string detect_type;
    std::string input_rtsp_url;
    std::string overlay_rtsp_url;
    int frame_id;
    std::vector<DetectionCandidate> latest_candidates;
    DetectionSessionState state;
    std::string last_error;
};
```

规则：

- `latest_candidates` 只保存当前会话最新一帧或最近一次有效识别结果。
- 更新候选缓存必须加锁。
- 对外确认候选时复制完整 `DetectionCandidate`，不能只保存 `candidate_id`。
- 重新启动会话后旧候选全部失效。

### 3.5 Overlay 渲染

正式输出画面内容：

- 原始视频画面。
- 识别圆圈。
- 圆心。
- 候选编号，例如 `#1`、`#2`。
- 置信度，建议保留两位小数。
- 可选状态文字：`detect_type`、帧号、候选数量、连接状态。

要求：

- 720p 和 1080p 下编号必须可读。
- 圆圈和文字不能遮挡严重，文字位置优先放在圆右侧或上方。
- 多候选时编号与 `latest_candidates` 中的 `candidate_id` 必须一致。

## 4. 错误处理与生命周期

### 4.1 开始识别

`startDetect(detect_type)` 行为：

- 校验当前状态必须是 `Idle` 或上一次已经完全停止。
- 校验 `detect_type` 合法且 Profile 存在。
- 初始化输入 RTSP。
- 初始化输出 RTSP 服务。
- 创建后台线程。
- 清空旧候选缓存。
- 本次会话 `candidate_id` 从 1 开始。

### 4.2 停止识别

`stopDetect()` 行为：

- 设置停止标志。
- 尽快打断 RTSP 读阻塞。
- 停止输出服务。
- join 后台线程。
- 释放 FFmpeg decoder、encoder、format context、sws context 等资源。
- 停止后状态回到 `Idle`，候选缓存不再可用于确认。

### 4.3 有限重连

建议策略：

- 输入打开失败或读取失败后进入 `Reconnecting`。
- 按配置 `reconnect_interval_ms` 和 `max_reconnect_attempts` 重试。
- 重连成功后继续原会话，帧号递增，候选缓存继续更新。
- 超过最大重试次数后进入 `Error`，记录 `last_error`。
- `Error` 状态下允许调用 `stopDetect()` 正常释放资源。

## 5. 验收标准

### 5.1 基础功能验收

- 能用 FFmpeg C API 打开摄像头 RTSP 输入并持续解码。
- 能将解码帧转换为 `CircleDetector` 可处理的 BGR `cv::Mat`。
- 能启动动态库内部正式 RTSP 输出服务。
- APP、VLC 或 ffplay 能播放 `honta_get_overlay_rtsp_url` 返回的圈注流。
- 圈注流画面包含圆圈、圆心、候选编号和置信度。
- `latest_candidates` 中候选编号与画面显示完全一致。

### 5.2 Profile 切换验收

- `honta_start_detect("entrance_hole")` 使用入口通孔 Profile。
- `honta_start_detect("center_horn")` 使用中心喇叭口 Profile。
- 两个 Profile 的半径范围、置信度阈值等配置确实不同并生效。
- 运行中再次调用 `honta_start_detect` 返回错误，不允许热切换。
- `stop -> start` 后候选编号从 1 重新开始。
- 未知 `detect_type` 返回错误，并能通过 `last_error` 看到原因。

### 5.3 断流与恢复验收

- 正常识别时断开输入 RTSP，状态进入 `Reconnecting`。
- 在最大重连次数内恢复输入，输出流继续更新。
- 超过最大重连次数后状态进入 `Error`。
- `Error` 状态下调用 `stopDetect` 能正常返回。
- 连续 start/stop 20 次不出现线程残留、卡死或明显资源泄漏。

### 5.4 输出服务验收

- 输出端口被占用时启动失败并返回明确错误。
- APP 或播放器断开后，识别线程不崩溃。
- 新播放器连接后能继续看到最新圈注画面。
- `honta_get_overlay_rtsp_url` 在 `init` 后即可返回地址，实际画面在 `startDetect` 后开始输出。

## 6. 非目标

- 不实现标定坐标计算。
- 不实现完整 Android JNI 封装。
- 不实现多个摄像头 RTSP 并发识别。
- 不支持多个识别会话同时运行。
- 不支持运行中热切换 `detect_type`。
- 不要求 APP 获取候选 JSON。
- Debug 四宫格 RTSP 输出可预留，不作为主验收必需项。

## 7. 交付物

- 第二阶段实现说明。
- Native RTSP/FFmpeg 输入模块。
- 内部 RTSP 圈注输出模块。
- 识别服务与 `DetectionSession`。
- `detection_profiles` 配置解析。
- PC 侧阶段二验证 demo。
- 示例配置 JSON。
- 验收记录：正常识别、Profile 切换、断流重连、start/stop 生命周期、播放器播放截图或日志。

## 8. 实施顺序

1. 固定 `detection_profiles` 配置结构和 `detect_type` 枚举。
2. 实现 Profile 解析与 `CircleDetectorConfig` 映射。
3. 实现 FFmpeg RTSP 输入解码，先在 PC demo 中验证 BGR 帧输出。
4. 实现 `DetectionSession`、候选缓存和后台识别线程。
5. 实现 overlay 渲染，确认候选编号与缓存一致。
6. 实现内部 RTSP 输出服务，先推送原始帧，再推送 overlay 帧。
7. 接入有限重连和 start/stop 生命周期控制。
8. 增加阶段二 PC demo，覆盖入口通孔与中心喇叭口两种 `detect_type`。
9. 完成验收测试并记录问题清单。

## 9. 风险与应对

- 库内 RTSP 输出服务复杂度高：先完成正式一路输出，Debug 输出延后。
- Android 设备多网卡导致播放地址不可达：必须配置 `overlay_public_host`，不要自动猜测。
- FFmpeg 阻塞导致 stop 卡死：输入模块必须支持超时和停止标志。
- 候选编号误选风险：每次新会话重新编号，并在画面和日志中显示当前 `detect_type`。
- 两套 Profile 参数不稳定：入口通孔和中心喇叭口需要分别用真实样本标定默认值。
- OpenCV `RtspReader` 与正式管线混用风险：`RtspReader` 只保留 PC 调试用途，正式管线直接使用 FFmpeg C API。
