# Android API 接入说明

版本：2026-06-02

适用对象：Android 工程师。

适用范围：Android App 通过 JNI 或自有封装调用 `libhonta_native.so`，完成 RTSP 检测、入口确认、中心确认、坐标计算和坐标读取。

## 交付文件

最小交付包只需要：

```text
libs/<abi>/libhonta_native.so          Android native 动态库
configs/runtime.json                   运行配置
configs/camera/top_rgb_camera.json     相机内参配置
configs/top_cover/A001.json            顶盖模型数据
api/honta_api.h                        C ABI 头文件
docs/android_api_reference.md          本说明文档
```

如果使用当前仓库里的 Android 示例工程，还会用到：

```text
android/app/src/main/java/com/honta/vision/HontaNative.java
android/app/src/main/java/com/honta/vision/HontaNativeController.java
android/app/src/main/cpp/HontaNativeBridge.cpp
```

## so 放置规则

真机：

```text
app/src/main/jniLibs/arm64-v8a/libhonta_native.so
```

模拟器：

```text
app/src/main/jniLibs/x86_64/libhonta_native.so
```

当前示例 JNI bridge 会生成 `libhonta_jni.so`。Java 加载顺序固定为：

```java
System.loadLibrary("honta_native");
System.loadLibrary("honta_jni");
```

不要调换加载顺序。

## Java API

Java 入口类：

```java
com.honta.vision.HontaNative
```

失败时 Java 包装方法会抛出：

```java
HontaNative.HontaNativeException
```

异常字段：

```java
public final int status;
```

### init

```java
public static void init(String configPath);
```

初始化 native context，并加载 `runtime.json`。

要求：

- `configPath` 必须是 Android 设备上的真实绝对路径。
- `runtime.json`、相机配置、顶盖模型文件必须已复制到 App 可读目录。
- 重复初始化前应先 `release()`，或使用 `HontaNativeController.init()` 自动处理。

### setTopCoverModel

```java
public static void setTopCoverModel(String modelId);
```

选择顶盖型号，例如 `A001`。

要求：

- 必须在 `init()` 之后调用。
- `top_cover_data_dir` 下必须存在对应模型文件。

### startDetect

```java
public static void startDetect(String detectType);
```

启动检测线程。

取值：

```text
entrance_hole
center_horn
```

行为：

- 打开 `runtime.json` 中的 `rtsp.input_url`。
- 启动圆形检测。
- 将画圈后的 overlay 推送到 `getOverlayRtspUrl()` 返回的 RTSP 地址。

注意：`startDetect()` 返回成功只表示线程启动成功，不表示已经产生候选目标。

### getOverlayRtspUrl

```java
public static String getOverlayRtspUrl();
```

读取 overlay RTSP 播放地址，例如：

```text
rtsp://127.0.0.1:8564/honta_overlay
```

该地址由 `runtime.json` 中的 `overlay_public_host`、`overlay_port`、`overlay_path` 拼出。

### stopDetect

```java
public static void stopDetect();
```

停止当前检测线程，释放 RTSP 输入输出资源。

调用场景：

- 切换检测类型前。
- 页面退出前。
- App 进入后台且不需要继续检测时。
- `release()` 前。

### confirmEntranceCandidate

```java
public static void confirmEntranceCandidate(
        int candidateId,
        int entranceHoleNo,
        double heightMm,
        double amrX,
        double amrY,
        double amrYaw);
```

确认入口通孔候选目标，并绑定当前 AMR 位姿。

参数：

| 参数 | 单位 | 说明 |
|---|---:|---|
| `candidateId` | - | 检测候选 ID，例如 overlay 中显示的 `#1` |
| `entranceHoleNo` | - | 顶盖入口孔编号 |
| `heightMm` | mm | 当前确认点高度 |
| `amrX` | m | AMR 地图坐标 x |
| `amrY` | m | AMR 地图坐标 y |
| `amrYaw` | rad | AMR 航向角 |

要求：

- 当前检测类型必须是 `entrance_hole`。
- 必须已调用 `setTopCoverModel()`。
- AMR 位姿必须由 Android 业务层先校验有效性。

### confirmCenterCandidate

```java
public static void confirmCenterCandidate(
        int candidateId,
        double heightMm,
        double amrX,
        double amrY,
        double amrYaw);
```

确认中心喇叭口候选目标，并绑定当前 AMR 位姿。

要求：

- 当前检测类型必须是 `center_horn`。
- 必须已完成入口确认。

### calculateAllHorns

```java
public static void calculateAllHorns();
```

根据入口确认、中心确认、顶盖模型和相机参数计算全部喇叭口坐标。

### getHornCoordinate

```java
public static HornCoordinate getHornCoordinate(int hornNo);
```

返回类型：

```java
public static final class HornCoordinate {
    public final int hornNo;
    public final double x;
    public final double y;
}
```

`x`、`y` 单位为米，对应 AMR 地图坐标系。

### getLastError

```java
public static String getLastError();
```

读取最近一次 native 错误文本。通常异常 message 中已经包含该文本，诊断页面可单独调用。

### release

```java
public static void release();
```

释放 native context 和检测资源。

调用场景：

- Activity/Fragment 销毁。
- App 退出。
- 重新加载配置前。
- 发生不可恢复错误后重建 native context。

## 推荐调用顺序

```text
init(runtime.json)
  -> setTopCoverModel("A001")
  -> getOverlayRtspUrl()
  -> startDetect("entrance_hole")
  -> 等待入口候选出现
  -> confirmEntranceCandidate(...)
  -> startDetect("center_horn")
  -> 等待中心候选出现
  -> confirmCenterCandidate(...)
  -> calculateAllHorns()
  -> getHornCoordinate(hornNo)
  -> stopDetect()
  -> release()
```

## 线程和生命周期

当前 native context 是进程内单实例模型。

要求：

- 不要从多个线程并发调用 `HontaNative`。
- 不要在 UI 线程直接执行可能阻塞的 native 调用。
- 使用 `HontaNativeController` 或业务层单线程 executor 串行化调用。
- 页面销毁时必须调用 `release()`。

## 配置文件要求

`runtime.json` 中关键字段：

```json
{
  "rtsp": {
    "input_url": "rtsp://127.0.0.1:8564/honta_test",
    "overlay_bind_ip": "0.0.0.0",
    "overlay_public_host": "127.0.0.1",
    "overlay_port": 8564,
    "overlay_path": "/honta_overlay"
  },
  "runtime": {
    "camera_config_path": "camera/top_rgb_camera.json",
    "top_cover_data_dir": "top_cover"
  }
}
```

路径规则：

- `configPath` 是 `runtime.json` 的绝对路径。
- `camera_config_path` 相对 `runtime.json` 所在目录解析。
- `top_cover_data_dir` 相对 `runtime.json` 所在目录解析。

调试环境可配合：

```powershell
adb reverse tcp:8564 tcp:8564
```

现场环境不要使用 `127.0.0.1`，应改成真实相机或现场 RTSP 地址。

## 错误码

| 状态码 | 名称 | 含义 |
|---:|---|---|
| `0` | `HONTA_OK` | 成功 |
| `1` | `HONTA_ERROR_INVALID_ARGUMENT` | 参数非法 |
| `2` | `HONTA_ERROR_NOT_INITIALIZED` | 未初始化 |
| `3` | `HONTA_ERROR_BAD_STATE` | 状态不允许 |
| `4` | `HONTA_ERROR_CONFIG` | 配置错误 |
| `5` | `HONTA_ERROR_DETECTION` | 检测错误 |
| `6` | `HONTA_ERROR_CALIBRATION` | 标定或坐标计算错误 |
| `7` | `HONTA_ERROR_BUFFER_TOO_SMALL` | 输出 buffer 太小 |
| `100` | `HONTA_ERROR_INTERNAL` | 内部错误 |

## 当前限制

当前 Java API 还没有正式候选列表/检测快照接口。调试页面可以通过 overlay 上显示的候选编号确认目标；正式 UI 建议后续补充候选列表 API 后再做用户选择交互。
