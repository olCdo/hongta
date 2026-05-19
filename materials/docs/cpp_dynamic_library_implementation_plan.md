# C++ 动态库实现计划

## 1. 目标

C++ 动态库负责视觉识别和坐标计算，并最终以 Android 可调用的动态库形式提供给 APP。由于前期不能直接与 APP 联调，因此开发节奏应先保证算法和坐标计算可以在本地独立运行、独立测试，再封装动态库接口，最后进入安卓联调。

核心原则：

- 先跑通算法，不依赖 APP。
- 先用本地图片、视频、JSON 配置完成内部测试。
- 动态库接口尽量稳定，避免后续反复改 JNI。
- APP 只负责交互、视频展示、数据暂存、目标序号转坐标和 AMR 控制。
- C++ 动态库在计算出全部喇叭口坐标后，一次性返回给 APP。

## 2. 总体阶段

### 2.1 阶段一：算法独立验证

目标是在没有 APP 的情况下，把入口通孔识别、中心喇叭口识别和基础坐标计算先跑通。

需要实现：

- 本地图片读取。
- 本地视频读取。
- 后续可扩展 RTSP 视频读取。
- 入口通孔圆形识别。
- 中心喇叭口圆形识别。
- 识别候选结果输出。
- 识别圈注图输出。
- 基础坐标计算模块。

识别结果建议包含：

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

建议先做命令行 demo：

```bash
```

输出内容：

- 识别结果 JSON。
- 带圆形圈注的图片或视频。
- 控制台调试信息。

### 2.2 阶段二：内部测试闭环

目标是在没有 APP 的情况下完整模拟一次地图校准流程。

建议实现一个校准测试程序：

```bash
calibration_demo.exe --config data/test_config.json
```

测试配置示例：

```json
{
  "top_cover_model": "A001",
  "height_mm": 1200,
  "entrance_hole_no": 3,
  "entrance_image": "data/entrance.jpg",
  "center_image": "data/center.jpg",
  "entrance_selected_detection_id": 1,
  "center_selected_detection_id": 1,
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

输出示例：

```json
{
  "success": true,
  "message": "calibration success",
  "entrance_hole_coordinate": {
    "x": 1.42,
    "y": 0.68
  },
  "center_horn_coordinate": {
    "x": 2.05,
    "y": 1.03
  },
  "horns": [
    {
      "horn_no": 1,
      "x": 1.23,
      "y": 2.34,
      "yaw": 0.0
    },
    {
      "horn_no": 2,
      "x": 1.45,
      "y": 2.56,
      "yaw": 0.0
    }
  ]
}
```

内部测试需要覆盖：

- 入口通孔识别成功。
- 入口通孔识别失败。
- 中心喇叭口识别成功。
- 中心喇叭口识别失败。
- 多个圆形候选时按用户选择的候选继续计算。
- 高度参数变化时坐标结果是否符合预期。
- AMR 位姿变化时坐标结果是否符合预期。
- 顶盖型号切换后是否加载正确图纸数据。
- 输出的全部喇叭口坐标是否能按序号查表。

### 2.3 阶段三：动态库接口封装

目标是将已经验证过的算法封装为 Android 可调用的动态库。

建议使用 C ABI 暴露接口，避免 C++ ABI 兼容问题。

头文件示例：

```cpp
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int honta_init(const char* config_json);

int honta_set_top_cover_model(const char* model_id);

int honta_detect_image(
    const unsigned char* image_data,
    int width,
    int height,
    int format,
    const char* detect_type,
    char* result_json,
    int result_json_size
);

int honta_confirm_entrance(
    const char* input_json,
    char* output_json,
    int output_json_size
);

int honta_confirm_center(
    const char* input_json,
    char* output_json,
    int output_json_size
);

int honta_calculate_all_horns(
    const char* input_json,
    char* output_json,
    int output_json_size
);

void honta_release();

#ifdef __cplusplus
}
#endif
```

接口说明：

- `honta_init`：初始化算法配置、相机参数、顶盖数据目录等。
- `honta_set_top_cover_model`：设置当前顶盖型号。
- `honta_detect_image`：对单帧图片进行识别，返回识别候选 JSON。
- `honta_confirm_entrance`：提交 APP 选择的入口通孔结果，返回入口通孔 AMR 坐标。
- `honta_confirm_center`：提交 APP 选择的中心喇叭口结果。
- `honta_calculate_all_horns`：计算并返回全部喇叭口 AMR 坐标。
- `honta_release`：释放资源。

当前收束后的正式方向是 C++ native 层直接接入 RTSP，使用 FFmpeg 解码，并在后续阶段补齐 RTSP 输出管线。本轮只保留帧级算法和 PC 调试工具，不实现完整 native 管线。

### 2.4 阶段四：动态库本地测试

目标是在进入安卓联调前，验证动态库接口本身稳定。

需要实现：

- PC 端动态库测试程序。
- Android NDK 编译脚本。
- Android 端最小 JNI 测试。

PC 测试程序模拟 APP 调用流程：

```bash
honta_lib_test.exe --config data/test_config.json
```

测试步骤：

1. 调用 `honta_init`。
2. 调用 `honta_set_top_cover_model`。
3. 读取入口图片并调用 `honta_detect_image`。
4. 模拟用户选择入口通孔候选。
5. 调用 `honta_confirm_entrance`。
6. 读取中心图片并调用 `honta_detect_image`。
7. 模拟用户选择中心喇叭口候选。
8. 调用 `honta_confirm_center`。
9. 调用 `honta_calculate_all_horns`。
10. 校验返回 JSON 是否符合 APP 使用要求。
11. 调用 `honta_release`。

Android 编译目标：

```text
arm64-v8a/libhonta.so
```

建议至少完成一次 Android 设备上的最小测试：

- 能加载 `libhonta.so`。
- 能调用 `honta_init`。
- 能传入一张测试图片。
- 能返回识别 JSON。
- 能返回全部喇叭口坐标 JSON。

### 2.5 阶段五：APP 联调

联调顺序建议：

1. APP 加载动态库成功。
2. APP 传单张图片，动态库返回识别结果。
3. APP 传视频帧，动态库持续返回识别结果。
4. APP 展示识别圈注。
5. 用户选择入口通孔候选，APP 调用入口确认接口。
6. 用户选择中心喇叭口候选，APP 调用中心确认接口。
7. APP 调用全部喇叭口坐标计算接口。
8. APP 保存全部喇叭口坐标表。
9. 上位机通过 Modbus TCP 写入目标点位序号。
10. APP 根据目标点位序号查找对应坐标。
11. APP 调用 AMR `go_to` 接口。
12. APP 通过 Modbus TCP 只读寄存器反馈 AMR 状态。

## 3. 模块划分建议

### 3.1 图像输入模块

职责：

- 读取图片。
- 读取本地视频。
- 将输入统一转换为算法使用的图像格式。

建议内部统一格式：

- BGR 或 RGB。
- 8 bit。
- 宽、高、步长明确。

### 3.2 圆形识别模块

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

### 3.3 坐标计算模块

职责：

- 根据像素坐标、高度、相机安装参数计算目标相对车体坐标。
- 根据 AMR 当前位姿转换到 AMR 地图坐标。
- 根据入口通孔和中心喇叭口计算顶盖旋转关系。
- 根据顶盖图纸数据计算全部喇叭口地图坐标。

### 3.4 顶盖数据模块

职责：

- 根据顶盖型号加载对应图纸数据。
- 保存通孔编号、喇叭口编号、图纸坐标等信息。
- 给坐标计算模块提供结构化数据。

图纸数据格式暂时可用 JSON 或 XML，建议优先使用 JSON，便于测试。

### 3.5 动态库接口层

职责：

- 对外提供稳定 C ABI。
- 将 APP 输入 JSON 解析为内部结构。
- 将内部结果序列化为 JSON 返回 APP。
- 管理算法实例和资源生命周期。

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
    entrance.jpg
    center.jpg
    entrance.mp4
    test_config.json
output/
  detection_result.json
  calibration_result.json
  debug_entrance.jpg
  debug_center.jpg
```

相机配置示例：

```json
{
  "camera_id": "top_rgb_camera",
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

- JSON 解析。
- 顶盖数据加载。
- 相机参数加载。
- 像素坐标到车体坐标转换。
- 车体坐标到 AMR 地图坐标转换。
- 顶盖旋转矩阵计算。
- 靶球序号与喇叭口坐标表一致性。

### 5.2 图像测试

建议建立测试样本集：

- 正常入口通孔图片。
- 正常中心喇叭口图片。
- 多个候选圆图片。
- 亮度偏暗图片。
- 反光图片。
- 圆形边缘不完整图片。
- 模糊图片。

每张图片保存期望结果：

```json
{
  "image": "entrance_001.jpg",
  "expected": {
    "has_detection": true,
    "center_x": 512.0,
    "center_y": 384.0,
    "tolerance_px": 10.0
  }
}
```

### 5.3 流程测试

至少覆盖：

- 完整校准成功流程。
- 入口识别失败流程。
- 中心识别失败流程。
- 用户选择不同候选流程。
- 顶盖型号不存在流程。
- 目标序号不存在流程。

## 6. 交付物

C++ 侧建议交付：

- `libhonta.so`
- `honta_api.h`
- PC 端识别 demo。
- PC 端校准 demo。
- PC 端动态库测试程序。
- Android NDK 编译说明。
- 测试图片、测试视频、测试配置。
- 示例输入 JSON。
- 示例输出 JSON。
- 动态库接口说明文档。
- 坐标计算说明文档。

## 7. 开发顺序建议

1. 建立工程结构。
2. 完成本地图片读取和调试图片输出。
3. 完成入口通孔圆形识别。
4. 完成中心喇叭口圆形识别。
5. 完成识别 demo。
6. 完成顶盖数据加载。
7. 完成相机参数加载。
8. 完成入口通孔坐标计算。
9. 完成中心喇叭口坐标计算。
10. 完成全部喇叭口坐标计算。
11. 完成校准 demo。
12. 完成动态库 C ABI 封装。
13. 完成 PC 动态库测试。
14. 完成 Android NDK 编译。
15. 完成 Android 最小调用测试。
16. 进入 APP 联调。

## 8. 风险点

- 圆形识别受现场光照和反光影响较大，需要尽早采集真实图片。
- 高度定义必须统一，否则坐标会系统性偏差。
- 相机安装参数必须可配置，不能写死。
- 顶盖图纸数据格式需要尽早确定。
- APP 与动态库之间的图片格式需要明确，例如 RGBA、RGB、BGR、YUV。
- 动态库接口返回 JSON 时需要考虑缓冲区大小。
- Android ABI 需要优先支持 `arm64-v8a`。
- 多线程调用时需要明确是否允许并发，建议前期按单实例串行调用处理。

## 9. 当前建议

第一版收束先稳定算法、配置和 PC 实时调试入口；下一阶段再设计 `honta_api.h`、native FFmpeg RTSP 输入/输出管线和 Android NDK `arm64-v8a` 构建。



