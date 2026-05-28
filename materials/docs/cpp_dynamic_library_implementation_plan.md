# C++ 动态库实现计划

## 1. 目标

C++ 动态库负责视觉识别、RTSP 视频接入、圈注 RTSP 输出、标定状态保存和坐标计算，并最终以 Android 可调用的动态库形式提供给 APP。正式 Android 集成方向为：C++ 动态库内部接入唯一一路摄像头输入 RTSP，使用 FFmpeg 解码，在后台持续识别入口通孔或中心喇叭口，并通过动态库内部 RTSP 输出服务提供带候选编号的圈注画面；APP 不逐帧传图，也不需要从动态库获取识别候选 JSON，只需要播放动态库提供的圈注 RTSP 地址，并把用户选择的候选编号和业务输入传回动态库。

核心原则：

- C++ 动态库内部负责 RTSP 拉流、FFmpeg 解码、圆形识别、候选编号管理、圈注 RTSP 输出和标定状态保存。
- APP 只通过 C ABI 调用动态库，不直接调用 C++ 类或传递复杂 C++ 对象。
- APP 负责交互、播放动态库提供的圈注 RTSP 地址、让用户选择候选编号、获取 AMR 位姿、下发 AMR 控制。
- C++ 动态库负责保存入口通孔确认结果、中心喇叭口确认结果、计算并缓存全部喇叭口 AMR 地图坐标。
- 标定完成后，APP 只传入目标喇叭口序号，C++ 动态库返回该序号对应的 AMR 地图 x/y 坐标，`yaw` 作为可选输出。

## 2. 总体阶段

### 2.1 阶段一：稳定圆形识别和 PC 调试工具

目标是在没有 APP 的情况下，继续稳定当前圆形识别算法和 PC 实时调试入口。

需要实现或保持：

- USB 摄像头和 RTSP 输入的 PC 调试能力。
- 入口通孔圆形识别参数。
- 中心喇叭口圆形识别参数。
- 识别候选编号生成。
- 识别圈注图输出，画面上显示圆、圆心、候选编号和置信度。
- 灰度、模糊、预处理、边缘图等调试图输出。
- 基于真实补光样本重新标定默认参数。

识别候选内部结构建议包含：

```json
{
  "id": 1,
  "center_x": 512.3,
  "center_y": 384.6,
  "radius": 42.8,
  "confidence": 0.92,
  "type": "entrance_hole"
}
```

该结构用于动态库内部保存和调试，不作为 Android 正式识别结果返回接口。

### 2.2 阶段二：实现 native RTSP/FFmpeg 输入与圈注输出管线

目标是让动态库正式承担 Android 主链路中的视频输入和圈注输出责任。输入 RTSP 是摄像头原始流，通常只有一路；输出 RTSP 是动态库内部生成和提供的圈注画面播放地址，不由 APP 传入。

需要实现：

- FFmpeg 打开 RTSP 输入。
- 视频解码为算法可处理的图像格式。
- 后台线程持续读取和识别。
- 动态库内部缓存当前最新候选列表，供用户确认接口按 `candidate_id` 查找。
- 动态库内部提供正式圈注 RTSP 输出服务，画面上显示候选编号，供 APP 播放和用户选择。
- 支持输出地址配置或查询，例如固定输出路径 `rtsp://<device-ip>:8554/honta_overlay`。
- 支持开始识别、停止识别。
- 支持 RTSP 断流后的错误记录和必要重连策略。

本阶段需要落地的内部状态：

- `DetectionSession`：保存当前识别会话状态，例如识别类型、输入 RTSP 地址、输出 RTSP 地址、运行状态、帧序号和最新候选列表。
- `DetectionCandidate`：保存单个识别候选，例如 `candidate_id`、圆心像素坐标、半径、置信度、识别类型、帧序号和时间戳。
- `latest_candidates`：只保存当前识别会话最新候选列表，供用户确认接口查找。
- `candidate_id`：只在当前识别会话内有效；每次重新开始识别后允许重新编号。

本阶段的主调用模型：

```text
APP 调用 honta_start_detect
  -> C++ 动态库后台拉 RTSP
  -> FFmpeg 解码
  -> CircleDetector 持续识别
  -> C++ 动态库维护候选编号和圈注画面
  -> C++ 动态库内部 RTSP 输出服务发布圈注流
  -> APP 播放动态库提供的圈注 RTSP 地址
  -> 用户在 APP 画面上选择候选编号
  -> APP 把 candidate_id 传回 C++ 动态库
```

### 2.3 阶段三：固定配置、顶盖数据和坐标系定义

目标是把坐标计算依赖的数据格式先固定下来，避免进入 Android 联调后反复改接口。

需要完成：

- 相机参数 JSON。
- 相机内参标定流程说明。
- 检测参数 JSON。
- 顶盖数据 JSON。
- 坐标系定义文档。
- 顶盖型号加载。
- 通孔编号和喇叭口编号校验。
- 高度定义说明。

本阶段需要落地的内部状态：

- `RuntimeConfig`：保存动态库运行配置，例如输入 RTSP 地址、圈注输出服务监听配置、播放地址生成参数、FFmpeg 参数、日志路径、顶盖数据目录和调试开关。
- `CameraConfig`：保存相机内参、畸变参数、内参标定分辨率和相机相对车体安装参数。
- `TopCoverModel`：保存当前顶盖型号以及图纸坐标数据。
- `TopCoverHole` / `TopCoverHorn`：分别保存通孔和喇叭口在顶盖图纸坐标系下的数据。
- `holes_by_no` / `horns_by_no`：按编号索引顶盖图纸数据，供标定和任务阶段快速查找。

需要明确的坐标系：

- Image 坐标系：像素原点、x/y 方向。
- Camera 坐标系：光轴方向和 x/y/z 方向。
- Vehicle 坐标系：车体前方、左方、上方定义。
- Map 坐标系：AMR 地图 x/y/yaw 定义。
- TopCover 坐标系：顶盖图纸原点、x/y 方向和角度正方向。

相机内参标定归属本阶段：

- 内参标定用于得到 `fx`、`fy`、`cx`、`cy` 和畸变参数 `k1`、`k2`、`p1`、`p2`、`k3`。
- 内参建议使用棋盘格或 Charuco 标定板，通过 OpenCV `calibrateCamera` 等方法完成。
- 内参必须记录对应图像分辨率，例如 `image_width` 和 `image_height`。
- 识别运行分辨率应尽量与标定分辨率一致；如果运行分辨率变化，`fx`、`fy`、`cx`、`cy` 需要按比例缩放，畸变参数通常不缩放。
- 阶段三负责定义内参配置格式和标定流程说明；阶段四负责在坐标解算中使用内参和畸变参数。

### 2.4 阶段四：实现标定状态和坐标解算

目标是实现动态库内部标定状态保存和坐标计算。

需要实现：

- 根据 `candidate_id` 从当前候选列表中找到用户确认的入口通孔候选。
- 保存入口通孔候选、入口通孔图纸编号、高度和入口识别时 AMR 位姿。
- 根据 `candidate_id` 从当前候选列表中找到用户确认的中心喇叭口候选。
- 保存中心喇叭口候选、高度和中心识别时 AMR 位姿。
- 根据像素坐标、高度、相机内参、畸变参数和相机安装参数计算目标相对车体坐标。
- 根据 AMR 当前位姿转换到 AMR 地图坐标。
- 根据入口通孔编号、入口通孔地图坐标和中心喇叭口地图坐标计算顶盖旋转关系。
- 根据顶盖图纸数据一次性计算全部喇叭口地图坐标，并缓存在动态库内部。
- 支持 APP 按喇叭口序号查询对应 AMR 地图坐标。

本阶段需要落地的内部状态：

- `CalibrationSession`：保存当前标定会话，包括顶盖型号、入口确认结果、中心确认结果和坐标缓存状态。
- `ConfirmedEntrance`：保存已确认入口通孔的 `candidate_id`、`entrance_hole_no`、候选圆完整信息、高度、AMR 位姿和入口地图坐标。
- `ConfirmedCenter`：保存已确认中心喇叭口的 `candidate_id`、候选圆完整信息、高度、AMR 位姿和中心地图坐标。
- `HornCoordinateCache`：保存标定完成后计算出的全部喇叭口 AMR 地图坐标。
- `HornMapCoordinate`：保存单个喇叭口的 `horn_no`、AMR 地图 `x/y` 和可选 `yaw`。

确认候选时不能只保存 `candidate_id`。动态库必须把用户选中的 `DetectionCandidate` 完整复制到 `CalibrationSession` 中，避免后续后台识别刷新 `latest_candidates` 后影响已确认的标定数据。

阶段四内部实现顺序建议：

1. 定义 `CalibrationSession`。
2. 定义 `ConfirmedEntrance` 和 `ConfirmedCenter`。
3. 实现入口候选确认逻辑。
4. 实现中心候选确认逻辑。
5. 定义 `HornCoordinateCache` 和 `HornMapCoordinate`。
6. 实现全部喇叭口坐标计算。
7. 实现按 `horn_no` 查询坐标。
8. 实现内部 `resetCalibration`，用于重新标定时清空入口、中心和坐标缓存。

建议增加纯数学黄金样例，不依赖图像：

```text
给定相机参数、高度、像素点、AMR 位姿、入口通孔编号和顶盖图纸数据
期望输出入口地图坐标、中心地图坐标、顶盖旋转角和全部喇叭口地图坐标
```

关于“按序号实时计算一个坐标”的路径：

- 第一版对 APP 暴露的主路径选择“标定完成后一次性计算全部坐标并缓存”。
- 任务阶段只做按 `horn_no` 查询，避免任务执行时发生复杂计算失败。
- C++ 内部可以保留 `calculateOneHorn(horn_no)` 私有能力，供测试或后续优化使用，但不作为第一版对 APP 的主接口。

### 2.5 阶段五：封装 C ABI 动态库

目标是将内部 C++ 实现封装为 Android 容易调用、二进制兼容性更稳定的 C 风格接口。

C ABI 的含义：

- 动态库内部仍然可以使用 C++ 类，例如 RTSP 管线、圆形检测器、坐标解算器、标定会话和顶盖数据仓库。
- 对 Android 只暴露 C 风格函数，不暴露 C++ 类、`std::string`、`std::vector`、异常或模板类型。
- 配置类复杂输入可以继续使用 JSON 字符串，例如 `honta_init` 的全局配置。
- 输入 RTSP 和圈注输出 RTSP 建议在 `honta_init` 的配置 JSON 中设置；`honta_start_detect` 只负责选择当前识别类型。
- 标定确认和坐标查询接口优先使用基础类型，例如 `int`、`double`、`double*`，减少 APP 解析 JSON 的负担。
- 错误信息通过 `honta_get_last_error` 获取，buffer 由 APP 申请和释放，动态库通过 `required_size` 告诉 APP 实际需要的大小。

阶段五不重新设计内部存储，只把阶段二、阶段三和阶段四已经实现的状态包装成 APP 可调用接口。第一版建议使用单个 `HontaContext` 管理运行配置、顶盖数据、识别会话、标定会话和喇叭口坐标缓存。

内部上下文建议：

```text
HontaContext
  ├─ RuntimeConfig
  ├─ CameraConfig
  ├─ TopCoverModel
  ├─ DetectionSession
  ├─ CalibrationSession
  └─ HornCoordinateCache
```

第一版约束：

- 单实例。
- 串行调用。
- 不支持多个 RTSP 同时识别。
- 不支持多个顶盖同时标定。
- 重新标定时必须清空旧的 `CalibrationSession` 和 `HornCoordinateCache`。

建议第一版接口：

```cpp
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int honta_init(const char* config_json);

int honta_set_top_cover_model(const char* model_id);

int honta_start_detect(const char* detect_type);

int honta_get_overlay_rtsp_url(
    char* output_text,
    int output_text_size,
    int* required_size
);

int honta_stop_detect();

int honta_confirm_entrance_candidate(
    int candidate_id,
    int entrance_hole_no,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw
);

int honta_confirm_center_candidate(
    int candidate_id,
    double height_mm,
    double amr_x,
    double amr_y,
    double amr_yaw
);

int honta_calculate_all_horns();

int honta_get_horn_coordinate(
    int horn_no,
    double* x,
    double* y,
    double* yaw
);

int honta_get_last_error(
    char* output_text,
    int output_text_size,
    int* required_size
);

void honta_release();

#ifdef __cplusplus
}
#endif
```

接口说明：

- `honta_init`：初始化算法配置、输入 RTSP 地址、圈注输出服务监听配置、播放地址生成参数、FFmpeg/RTSP 配置、相机参数、顶盖数据目录、日志配置等。
- `honta_set_top_cover_model`：设置当前顶盖型号并加载对应顶盖数据。
- `honta_start_detect`：开始 RTSP 识别，`detect_type` 使用 `entrance_hole` 或 `center_horn`；输入 RTSP 和输出 RTSP 使用初始化配置。
- `honta_get_overlay_rtsp_url`：获取动态库提供给 APP 播放的圈注 RTSP 地址。
- `honta_stop_detect`：停止后台识别并释放本次 RTSP 资源。
- `honta_confirm_entrance_candidate`：APP 回传用户选择的入口通孔候选编号，同时传入入口通孔图纸编号、高度和 AMR 位姿；动态库内部计算并保存入口通孔地图坐标。
- `honta_confirm_center_candidate`：APP 回传用户选择的中心喇叭口候选编号，同时传入高度和 AMR 位姿；动态库内部计算并保存中心喇叭口地图坐标。
- `honta_calculate_all_horns`：基于已保存的入口和中心标定结果，一次性计算全部喇叭口坐标并缓存在动态库内部。
- `honta_get_horn_coordinate`：APP 传入目标喇叭口序号，动态库返回该序号对应的 AMR 地图坐标。
- `honta_get_last_error`：获取最近一次错误信息文本。
- `honta_release`：释放全局资源。

`honta_get_overlay_rtsp_url` 输入输出说明：

- `output_text`：APP 提供的字符缓冲区，用于接收动态库写回的圈注 RTSP 播放地址。
- `output_text_size`：`output_text` 的缓冲区大小，单位为字节。
- `required_size`：动态库写回实际所需字节数，包含字符串结尾的 `\0`。
- 返回 `0` 表示成功，`output_text` 中写入类似 `rtsp://192.168.1.20:8554/honta_overlay` 的播放地址。
- 如果缓冲区太小，返回 buffer 不足错误码，并通过 `required_size` 告诉 APP 需要的大小。
- 该接口建议在 `honta_init` 成功后即可调用；此时可先获得播放地址，真正画面在 `honta_start_detect` 后开始输出。
- APP 不传完整输出 RTSP URL。动态库根据 `overlay_bind_ip`、`overlay_port`、`overlay_path` 和可选 `overlay_public_host` 生成播放地址。

编号说明：

- `candidate_id` 是动态库识别画面上显示的候选编号，例如 `#1`、`#2`、`#3`。
- `entrance_hole_no` 是顶盖图纸中的入口通孔编号。
- `horn_no` 是顶盖图纸中的喇叭口或靶球编号。

### 2.6 阶段六：PC 侧完整闭环测试

目标是在进入 Android 前，先用 PC 工具模拟完整标定流程。

建议实现一个校准测试程序：

```bash
calibration_demo.exe --config data/test_config.json
```

测试流程：

1. 调用 `honta_init`。
2. 调用 `honta_set_top_cover_model`。
3. 调用 `honta_get_overlay_rtsp_url` 获取圈注 RTSP 播放地址。
4. 调用 `honta_start_detect` 启动入口通孔识别。
5. 模拟用户选择入口通孔候选编号。
6. 调用 `honta_confirm_entrance_candidate` 保存入口标定结果。
7. 调用 `honta_start_detect` 启动中心喇叭口识别。
8. 模拟用户选择中心喇叭口候选编号。
9. 调用 `honta_confirm_center_candidate` 保存中心标定结果。
10. 调用 `honta_calculate_all_horns` 计算并缓存全部喇叭口坐标。
11. 调用 `honta_get_horn_coordinate` 按多个 `horn_no` 查询坐标。
12. 调用 `honta_release`。

测试配置示例：

```json
{
  "top_cover_model": "A001",
  "height_mm": 1200,
  "entrance_hole_no": 3,
  "rtsp": {
    "input_url": "rtsp://user:password@192.168.1.10:554/stream1",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "192.168.1.20",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay"
  },
  "entrance_selected_candidate_id": 1,
  "center_selected_candidate_id": 1,
  "target_horn_no": 8,
  "entrance_amr_pose": {
    "x": 1.2,
    "y": 0.5,
    "yaw": 0.0
  },
  "center_amr_pose": {
    "x": 2.0,
    "y": 1.0,
    "yaw": 0.1
  }
}
```

坐标查询输出示例：

```json
{
  "success": true,
  "horn_no": 8,
  "x": 1.23,
  "y": 2.34,
  "yaw": 0.0
}
```

该 JSON 仅作为 PC demo 输出格式，不要求 Android 正式接口按 JSON 接收喇叭口坐标。

### 2.7 阶段七：Android 最小联调

目标是在完整 APP 业务接入前，先验证动态库能在 Android 设备上运行。

建议最小验证：

- 能加载 `arm64-v8a/libhonta.so`。
- 能调用 `honta_init`。
- 能调用 `honta_set_top_cover_model`。
- 能调用 `honta_get_overlay_rtsp_url` 获取 APP 播放地址。
- 能调用 `honta_start_detect` 启动入口通孔 RTSP 识别。
- 用户能在 APP 展示画面中看到候选编号。
- APP 能调用 `honta_confirm_entrance_candidate`。
- APP 能调用 `honta_start_detect` 启动中心喇叭口 RTSP 识别。
- APP 能调用 `honta_confirm_center_candidate`。
- APP 能调用 `honta_calculate_all_horns`。
- APP 能调用 `honta_get_horn_coordinate` 获取指定喇叭口 AMR 地图坐标。
- 能调用 `honta_stop_detect` 和 `honta_release` 正常释放资源。

APP 联调顺序建议：

1. APP 加载动态库成功。
2. APP 配置输入 RTSP 地址，或使用动态库初始化配置中的输入 RTSP 地址。
3. APP 调用 `honta_get_overlay_rtsp_url` 获取圈注 RTSP 播放地址。
4. APP 调用入口通孔开始识别，并播放动态库提供的圈注 RTSP。
5. 用户选择入口候选编号，APP 调用入口确认接口，传入 `candidate_id`、`entrance_hole_no`、高度和 AMR 位姿。
6. APP 调用中心喇叭口开始识别，继续播放同一个圈注 RTSP 地址。
7. 用户选择中心候选编号，APP 调用中心确认接口，传入 `candidate_id`、高度和 AMR 位姿。
8. APP 调用全部喇叭口坐标计算接口。
9. 上位机通过 Modbus TCP 写入目标点位序号。
10. APP 调用 `honta_get_horn_coordinate` 查询对应 AMR 坐标。
11. APP 调用 AMR `go_to` 接口。

## 3. 模块划分建议

### 3.1 RTSP/FFmpeg 输入模块

职责：

- 打开 RTSP 输入。
- 使用 FFmpeg 解码视频帧。
- 管理重连、超时、停止和资源释放。
- 将解码帧转换为算法内部格式。

### 3.2 内部 RTSP 输出模块

职责：

- 接收带圈注的视频帧。
- 使用 FFmpeg 编码为 H.264 或现场确认的编码格式。
- 在动态库内部提供 RTSP 输出服务，发布正式圈注流。
- 管理输出端口、路径、客户端连接、停止和资源释放。
- 向 APP 提供可播放的圈注 RTSP 地址。

### 3.3 圆形识别模块

职责：

- 图像预处理。
- 圆形候选检测。
- 候选过滤。
- 置信度评分。
- 输出候选列表。

需要重点考虑：

- 光照变化。
- 补光灯反光。
- 圆孔边缘缺失。
- 画面中出现多个圆形。
- 相机角度导致圆形透视变形。

### 3.4 识别服务模块

职责：

- 管理后台识别线程。
- 保存最新候选列表。
- 为候选分配稳定的 `candidate_id`。
- 区分入口通孔和中心喇叭口识别配置。
- 生成带候选编号的圈注画面。

### 3.5 标定会话模块

职责：

- 保存当前顶盖型号。
- 保存已确认的入口通孔候选、入口通孔图纸编号、高度和 AMR 位姿。
- 保存已确认的中心喇叭口候选、高度和 AMR 位姿。
- 判断是否满足计算全部喇叭口坐标的前置条件。
- 保存全部喇叭口坐标缓存。

### 3.6 坐标计算模块

职责：

- 根据像素坐标、高度、相机安装参数计算目标相对车体坐标。
- 根据 AMR 当前位姿转换到 AMR 地图坐标。
- 根据入口通孔和中心喇叭口计算顶盖旋转关系。
- 根据顶盖图纸数据计算全部喇叭口地图坐标。

### 3.7 顶盖数据模块

职责：

- 根据顶盖型号加载对应图纸数据。
- 保存通孔编号、喇叭口编号、图纸坐标等信息。
- 给坐标计算模块提供结构化数据。

图纸数据格式第一版建议固定为 JSON，后续如需支持 CAD/XML，可先转换为标准 JSON 后再进入动态库。

### 3.8 动态库接口层

职责：

- 对外提供稳定 C ABI。
- 管理算法实例、后台线程、标定会话和资源生命周期。
- 将 APP 传入的基础类型参数转换为内部结构。
- 统一错误码和最近错误信息。

## 4. 数据文件建议

建议目录结构：

```text
data/
  camera/
    camera_config.json
  top_cover/
    A001.json
    A002.json
  test/
    test_config.json
output/
  calibration_result.json
  debug_overlay.jpg
  debug_stages/
```

相机配置示例：

```json
{
  "camera_id": "top_rgb_camera",
  "image_width": 1920,
  "image_height": 1080,
  "intrinsic": {
    "fx": 1200.0,
    "fy": 1200.0,
    "cx": 640.0,
    "cy": 360.0
  },
  "distortion": {
    "k1": 0.0,
    "k2": 0.0,
    "p1": 0.0,
    "p2": 0.0,
    "k3": 0.0
  },
  "mount": {
    "x": 0.0,
    "y": 0.0,
    "z": 0.0,
    "yaw": 0.0,
    "pitch": 0.0,
    "roll": 0.0
  }
}
```

相机配置说明：

- `image_width` / `image_height`：内参标定时使用的图像分辨率。
- `intrinsic`：相机内参，包含 `fx`、`fy`、`cx`、`cy`。
- `distortion`：镜头畸变参数，包含 `k1`、`k2`、`p1`、`p2`、`k3`。
- `mount`：相机相对车体坐标系的安装位置和角度，属于相机外参。
- 正式坐标解算前必须使用真实标定内参；开发早期可用厂家参数或近似值跑通流程，但不能作为最终精度验收依据。

RTSP 配置示例：

```json
{
  "rtsp": {
    "input_url": "rtsp://192.168.1.10:554/stream1",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "192.168.1.20",
    "overlay_port": 8554,
    "overlay_path": "/honta_overlay"
  }
}
```

字段含义：

- `input_url`：摄像头原始视频流地址，动态库从这里拉流。
- `overlay_bind_ip`：动态库内部 RTSP 输出服务监听地址。
- `overlay_public_host`：APP 可访问到的本机 IP；多网卡环境建议由 APP 或配置明确传入。
- `overlay_port`：动态库内部 RTSP 输出服务监听端口。
- `overlay_path`：圈注输出流路径。
- 圈注 RTSP 播放地址不在配置中直接传入，由动态库生成并通过 `honta_get_overlay_rtsp_url` 返回给 APP。若 `overlay_public_host` 为 `192.168.1.20`，`overlay_port` 为 `8554`，`overlay_path` 为 `/honta_overlay`，则返回 `rtsp://192.168.1.20:8554/honta_overlay`。

顶盖数据示例：

```json
{
  "model_id": "A001",
  "holes": [
    {
      "hole_no": 1,
      "x": 1.0,
      "y": 0.0
    }
  ],
  "horns": [
    {
      "horn_no": 1,
      "x": 0.5,
      "y": 0.5,
      "yaw": 0.0
    }
  ]
}
```

## 5. 测试策略

### 5.1 单元测试

建议覆盖：

- JSON 配置解析。
- 顶盖数据加载。
- 相机参数加载。
- 相机内参分辨率校验。
- 像素点畸变矫正。
- `candidate_id` 查找和候选缓存更新。
- 入口确认状态保存。
- 中心确认状态保存。
- 像素坐标到车体坐标转换。
- 车体坐标到 AMR 地图坐标转换。
- 顶盖旋转矩阵计算。
- 全部喇叭口坐标缓存。
- `horn_no` 查询坐标。
- 未标定时查询坐标返回错误。
- 目标序号不存在时返回错误。

### 5.2 图像测试

建议建立测试样本集：

- 正常入口通孔样本。
- 正常中心喇叭口样本。
- 多个候选圆样本。
- 亮度偏暗样本。
- 反光样本。
- 圆形边缘不完整样本。
- 模糊样本。

每个样本保存期望结果：

```json
{
  "sample": "entrance_001",
  "expected": {
    "has_detection": true,
    "center_x": 512.0,
    "center_y": 384.0,
    "tolerance_px": 10.0
  }
}
```

### 5.3 RTSP 和流程测试

至少覆盖：

- RTSP 正常打开和持续识别。
- 内部 RTSP 输出服务能启动，APP/VLC/ffplay 能播放圈注流。
- RTSP 断流和停止识别。
- 入口识别成功并确认候选。
- 中心识别成功并确认候选。
- 用户选择不存在的 `candidate_id`。
- 未确认入口就确认中心。
- 未完成标定就计算全部喇叭口。
- 完整校准成功流程。
- 标定完成后按多个 `horn_no` 查询坐标。
- 查询不存在的 `horn_no`。

## 6. 交付物

C++ 侧建议交付：

- `libhonta.so`
- `honta_api.h`
- PC 端识别 demo。
- PC 端校准 demo。
- PC 端动态库测试程序。
- Android NDK 编译说明。
- 测试样本、测试配置。
- 示例配置 JSON。
- 动态库接口说明文档。
- 坐标计算说明文档。
- RTSP/FFmpeg 输入与内部 RTSP 输出管线说明文档。

## 7. 开发顺序建议

1. 稳定当前 `CircleDetector` 和 `detect_stream_demo`。
2. 基于真实样本调整入口通孔和中心喇叭口默认参数。
3. 固定相机参数 JSON、顶盖 JSON 和坐标系定义。
4. 完成相机内参标定流程说明或标定工具，输出 `CameraConfig`。
5. 完成顶盖数据加载。
6. 完成相机参数加载和内参分辨率校验。
7. 完成候选缓存和 `candidate_id` 管理。
8. 完成入口确认和中心确认状态保存。
9. 完成单点地图坐标计算。
10. 完成全部喇叭口坐标计算和内部缓存。
11. 完成 `horn_no` 坐标查询。
12. 完成内部 `resetCalibration`。
13. 完成坐标解算黄金样例测试。
14. 设计并实现 `honta_api.h` C ABI。
15. 实现 native RTSP/FFmpeg 输入、内部 RTSP 输出和后台识别服务。
16. 完成 PC 端动态库测试。
17. 完成 PC 端完整校准 demo。
18. 完成 Android NDK `arm64-v8a` 构建。
19. 完成 Android 最小调用测试。
20. 进入 APP 联调。

## 8. 风险点

- 圆形识别受现场光照和反光影响较大，需要尽早采集真实图片和真实 RTSP 样本。
- APP 不获取候选 JSON 后，圈注画面上的候选编号必须清晰、稳定，避免用户选错。
- `candidate_id` 的生命周期必须明确：每次重新开始识别后候选编号可能变化，APP 只能确认当前识别会话内的编号。
- 高度定义必须统一，否则坐标会系统性偏差。
- 相机内参必须使用现场相机和固定分辨率标定，不能长期依赖厂家参数或近似值。
- 识别分辨率与内参标定分辨率不一致时，需要正确缩放 `fx`、`fy`、`cx`、`cy`。
- 相机安装参数必须可配置，不能写死。
- 坐标系定义必须尽早固定，尤其是 Camera、Vehicle、Map 和 TopCover 坐标系。
- 顶盖图纸数据格式需要尽早确定，第一版建议固定为 JSON。
- FFmpeg RTSP 拉流、断流重连和资源释放需要单独验证。
- 动态库内部 RTSP 输出服务会增加实现复杂度，需要重点验证端口占用、客户端断开、停止释放和 Android 播放兼容性。
- 后台识别线程生命周期需要明确，避免重复启动、停止卡死或释放后访问。
- 标定状态缓存需要支持重新标定时清空或覆盖。
- Android ABI 需要优先支持 `arm64-v8a`。
- Android 上日志、错误信息和现场诊断方式需要提前设计。
- 多线程调用时需要明确是否允许并发，建议第一版按单实例串行调用处理。

## 9. 当前建议

第一版先稳定圆形识别、配置格式、候选编号显示、标定状态保存和坐标解算；随后实现 native RTSP/FFmpeg 管线和 C ABI。正式业务主路径采用“标定完成后一次性计算并缓存全部喇叭口坐标，任务阶段按喇叭口序号查询坐标”，不建议第一版让任务阶段每次按序号重新计算坐标。
