# 第三阶段计划书：固定配置、顶盖数据和坐标系定义

## 1. 目标

第三阶段只解决一件事：把后续坐标计算需要的数据格式固定下来。

本阶段完成后，工程里应具备：

- 可读取的运行配置。
- 可读取的相机参数。
- 明确的相机内参标定流程。
- 可读取的顶盖数据。
- 明确的坐标系定义。
- 明确的高度定义。
- 可执行的配置检查和编号校验。

本阶段不做：

- 不做完整坐标解算。
- 不做入口和中心候选确认。
- 不做 C ABI。
- 不做 Android 联调。
- 不做真实 RTSP 闭环测试。

明确技术决策：

- 顶盖数据第一版只支持 JSON。
- 长度单位第一版统一使用 `mm`。
- 配置文件中的角度统一使用 `deg`。
- C++ 内部坐标计算后续可转换为 `rad`。
- Map 坐标输出单位为 `m`。
- AMR `yaw` 单位为 `rad`。
- 顶盖图纸原点为中心喇叭口。
- 中心喇叭口固定为 `horn_no = 1`。

## 2. 交付物

本阶段交付以下内容：

- `RuntimeConfig`：运行配置结构。
- `CameraConfig`：相机参数结构。
- 相机内参标定流程说明。
- `TopCoverModel`：顶盖数据结构。
- JSON 加载和校验逻辑。
- 不依赖 OpenCV 的配置/顶盖数据小库。
- 示例运行配置 JSON。
- 示例相机参数 JSON。
- 示例检测参数 JSON。
- 示例顶盖数据 JSON。
- `phase3_config_check` 配置检查工具。
- 坐标系定义文档。

建议新增文件：

```text
code/include/honta/config/runtime_config.h
code/include/honta/config/camera_config.h
code/include/honta/config/config_loader.h
code/include/honta/model/top_cover_model.h
code/src/config/runtime_config.cpp
code/src/config/camera_config.cpp
code/src/config/config_loader.cpp
code/src/model/top_cover_model.cpp
code/tools/phase3_config_check.cpp
materials/docs/coordinate_system_definition.md
materials/docs/camera_intrinsic_calibration.md
```

构建约束：

- `phase3_config_check` 只能依赖配置/顶盖数据小库。
- `phase3_config_check` 不应依赖 OpenCV、FFmpeg 或视觉识别模块。

示例配置建议放在：

```text
code/configs/runtime_example.json
code/configs/camera/top_rgb_camera.json
code/configs/top_cover/A001.json
```

## 3. 实现步骤

1. 定义 `RuntimeConfig`。

   核心字段：

   ```cpp
   struct RuntimeConfig {
       std::string input_rtsp_url;
       std::string overlay_bind_ip = "0.0.0.0";
       std::string overlay_public_host;
       int overlay_port = 8554;
       std::string overlay_path = "/honta_overlay";
       std::string camera_config_path;
       std::string top_cover_data_dir;
       std::map<std::string, DetectionProfileConfig> detection_profiles;
   };
   ```

   检测配置至少包含：

   - `entrance_hole`
   - `center_horn`

   每个检测配置至少支持：

   - `min_radius_px`
   - `max_radius_px`
   - `min_confidence`
   - `min_rim_edge_support`
   - `canny_high_threshold`
   - `hough_accumulator_threshold`
   - `enable_clahe`
   - `enable_hist_equalization`
   - `max_radius_image_ratio`
   - `max_results`

2. 定义 `CameraConfig`。

   核心字段：

   ```cpp
   struct CameraConfig {
       std::string camera_id;
       int image_width = 0;
       int image_height = 0;
       double fx = 0.0;
       double fy = 0.0;
       double cx = 0.0;
       double cy = 0.0;
       double k1 = 0.0;
       double k2 = 0.0;
       double p1 = 0.0;
       double p2 = 0.0;
       double k3 = 0.0;
       double mount_x_mm = 0.0;
       double mount_y_mm = 0.0;
       double mount_z_mm = 0.0;
       double mount_yaw_deg = 0.0;
       double mount_pitch_deg = 0.0;
       double mount_roll_deg = 0.0;
   };
   ```

   相机配置必须记录内参标定分辨率：

   - `image_width`
   - `image_height`

   相机配置必须记录畸变参数：

   - `k1`
   - `k2`
   - `p1`
   - `p2`
   - `k3`

3. 定义 `TopCoverModel`。

   核心字段：

   ```cpp
   struct TopCoverHole {
       int hole_no = 0;
       double x_mm = 0.0;
       double y_mm = 0.0;
   };

   struct TopCoverHorn {
       int horn_no = 0;
       double x_mm = 0.0;
       double y_mm = 0.0;
       double yaw_deg = 0.0;
   };

   struct TopCoverModel {
       std::string model_id;
       std::vector<TopCoverHole> holes;
       std::vector<TopCoverHorn> horns;
       std::map<int, TopCoverHole> holes_by_no;
       std::map<int, TopCoverHorn> horns_by_no;
   };
   ```

4. 实现 JSON 加载。

   使用现有 `code/third_party/nlohmann/json.hpp`。

   需要提供：

   - `loadRuntimeConfig(path)`
   - `loadCameraConfig(path)`
   - `loadTopCoverModel(data_dir, model_id)`

5. 实现校验。

   必须校验：

   - `input_rtsp_url` 不能为空。
   - `overlay_bind_ip` 不能为空。
   - `overlay_public_host` 不能为空。
   - `overlay_port` 必须在 `1..65535`。
   - `overlay_path` 必须以 `/` 开头。
   - 必须存在 `entrance_hole` 检测配置。
   - 必须存在 `center_horn` 检测配置。
   - `camera_id` 不能为空。
   - `image_width` 和 `image_height` 必须大于 0。
   - `fx` 和 `fy` 必须大于 0。
   - `model_id` 不能为空。
   - `hole_no` 必须为正整数。
   - `horn_no` 必须为正整数。
   - 重复 `hole_no` 必须报错。
   - 重复 `horn_no` 必须报错。
   - 必须存在 `hole_no = 1`，用于定义顶盖 x 轴正方向。
   - 必须存在 `horn_no = 1`，作为顶盖图纸原点。

6. 补充相机内参标定流程说明。

   文档需要写清楚：

   - 内参用于得到 `fx`、`fy`、`cx`、`cy`。
   - 畸变参数用于得到 `k1`、`k2`、`p1`、`p2`、`k3`。
   - 标定建议使用棋盘格或 Charuco 标定板。
   - 标定可使用 OpenCV `calibrateCamera`。
   - 正式坐标解算前必须使用真实现场相机标定结果。
   - 识别运行分辨率应尽量和标定分辨率一致。
   - 如果运行分辨率变化，`fx`、`fy`、`cx`、`cy` 需要按比例缩放。
   - 畸变参数通常不随分辨率缩放。

7. 实现顶盖索引。

   加载顶盖数据后，建立：

   - `holes_by_no`
   - `horns_by_no`

   阶段四直接用这两个索引按编号查数据。

8. 新增配置检查工具。

   建议命令：

   ```bash
   phase3_config_check --config code/configs/runtime_example.json --model A001
   ```

   工具需要输出：

   - 运行配置是否加载成功。
   - 相机配置是否加载成功。
   - 顶盖型号是否加载成功。
   - 通孔数量。
   - 喇叭口数量。
   - 校验失败时的明确错误原因。

9. 新增坐标系定义文档。

   坐标系用表格写清楚：

   | 坐标系 | 原点 | 方向 | 单位 | 用途 |
   | --- | --- | --- | --- | --- |
   | Image | 图像左上角 | x 向右，y 向下 | px | 圆心像素坐标 |
   | Camera | 相机光心 | x 向右，y 向下，z 沿光轴向前 | mm | 像素反投影 |
   | Vehicle | 车体参考点 | x 向前，y 向左，z 向上 | mm | 相机安装和车体坐标 |
   | Map | AMR 地图原点 | 按 AMR 地图定义 | m | 目标导航坐标 |
   | TopCover | 中心喇叭口 | x 正方向指向 1 号入口通孔，y 轴与 x 轴垂直 | mm | 通孔和喇叭口图纸坐标 |

   高度定义必须写成一句固定口径：

   ```text
   height_mm 表示现场提供的识别口到地面的垂直距离，单位为 mm。
   ```

## 4. 验收标准

文档验收：

- 坐标系定义文档存在。
- 坐标系表格包含 Image、Camera、Vehicle、Map、TopCover。
- 高度 `height_mm` 定义清楚。
- 相机内参标定流程说明存在。
- 相机内参配置包含标定分辨率。
- 相机内参配置包含畸变参数。
- 顶盖数据格式明确为 JSON。
- 单位和角度规则写清楚。

代码验收：

- 工程能编译通过。
- `phase3_config_check` 能加载正常配置。
- `phase3_config_check` 能加载 `A001` 顶盖数据。
- 正常顶盖数据能生成 `holes_by_no`。
- 正常顶盖数据能生成 `horns_by_no`。
- 不存在的顶盖型号必须报错。
- 重复 `hole_no` 必须报错。
- 重复 `horn_no` 必须报错。
- 缺少 `hole_no = 1` 必须报错。
- 缺少 `horn_no = 1` 必须报错。
- 非法 `overlay_port` 必须报错。
- 非法 `overlay_path` 必须报错。
- 空 `overlay_bind_ip` 必须报错。
- 缺少 `entrance_hole` 检测配置必须报错。
- 缺少 `center_horn` 检测配置必须报错。
- 相机 `image_width <= 0` 或 `image_height <= 0` 必须报错。
- 相机 `fx <= 0` 或 `fy <= 0` 必须报错。
- 相机 `cx/cy` 超出图像范围必须报错。
- 相机内参、畸变参数和安装参数不是有限数时必须报错。

衔接验收：

- 阶段四可以直接按 `entrance_hole_no` 查通孔。
- 阶段四可以直接按 `horn_no` 查喇叭口。
- 阶段四不需要重新设计相机参数格式。
- 阶段四可以直接使用内参、畸变参数和标定分辨率。
- 阶段四负责在坐标解算中使用畸变矫正。
- 阶段五的 `honta_init(config_json)` 可以复用 `RuntimeConfig`。
- 阶段五的 `honta_set_top_cover_model(model_id)` 可以复用 `TopCoverModel` 加载逻辑。

## 5. 已确认事项

以下事项已确认，阶段四按这些结论实现：

- 顶盖图纸原点为中心喇叭口。
- 顶盖 x 轴为中心喇叭口指向 1 号入口通孔的方向。
- 顶盖 x 正方向指向 1 号入口通孔。
- 顶盖 y 轴与 x 轴垂直。
- 中心喇叭口固定为 `horn_no = 1`。
- AMR 地图坐标单位为 `m`。
- AMR `yaw` 单位为 `rad`。
- 现场提供的是识别口到地面的垂直距离。
