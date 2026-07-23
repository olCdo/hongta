# 第七阶段 Android 工程交接说明

## 当前交接状态

Android 工程已经具备最小 JNI 联调能力：

- 工程目录：`android/`
- Debug APK 可构建。
- 模拟器可安装运行。
- JNI bridge 可加载 `honta_native`。
- JNI smoke test 已通过。

当前工程重点是验证 native 调用链和 Android 集成边界。完整业务 UI、真实 RTSP 播放页面、AMR 状态接入和 Modbus 联调仍需要继续开发。

## Android 工程入口

- Gradle 根目录：`android/`
- App module：`android/app`
- 主 Activity：`android/app/src/main/java/com/honta/vision/MainActivity.java`
- Java native 封装：`android/app/src/main/java/com/honta/vision/HontaNative.java`
- Native 调用控制器：`android/app/src/main/java/com/honta/vision/HontaNativeController.java`
- JNI C++ bridge：`android/app/src/main/cpp/HontaNativeBridge.cpp`
- Native so 目录：
  - `android/app/src/main/jniLibs/arm64-v8a/libhonta_native.so`
  - `android/app/src/main/jniLibs/x86_64/libhonta_native.so`

## 当前 JNI 接口

Java 层通过 `HontaNative` 调用 native：

- `init(String configPath)`
- `setTopCoverModel(String modelId)`
- `startDetect(String detectType)`
- `getOverlayRtspUrl()`
- `stopDetect()`
- `confirmEntranceCandidate(...)`
- `confirmCenterCandidate(...)`
- `calculateAllHorns()`
- `getHornCoordinate(int hornNo)`
- `getLastError()`
- `release()`

调用失败会抛出 `HontaNative.HontaNativeException`，异常中包含 native status 和错误文本。

## 生命周期规则

- `init/start/stop/release` 必须串行调用。
- 不要在 UI 线程直接执行耗时 native 调用。
- 页面销毁或 app 退出时必须调用 `release()`。
- 当前 native context 按单实例链路设计，同一时刻只支持一条标定链路。
- `HontaNativeController` 已提供单线程串行化调用骨架，后续业务页面可复用。

## Smoke Test

App 启动后会自动执行 JNI smoke test，也可以点击页面按钮重跑。

Smoke test 使用 assets 中的配置：

- `android/app/src/main/assets/phase7_smoke/runtime.json`
- `android/app/src/main/assets/phase7_smoke/camera/top_rgb_camera.json`
- `android/app/src/main/assets/phase7_smoke/top_cover/A001.json`

测试链路：

```text
init
  -> setTopCoverModel(A001)
  -> getOverlayRtspUrl
  -> startDetect(entrance_hole)
  -> confirmEntranceCandidate
  -> startDetect(center_horn)
  -> confirmCenterCandidate
  -> calculateAllHorns
  -> getHornCoordinate(1)
  -> release
```

通过标志：

```text
JNI smoke test: PASS
```

## 构建命令

本机 Windows 环境示例：

```powershell
$env:JAVA_HOME='D:\Program Files\Android\Android Studio\jbr'
$env:ANDROID_HOME='C:\Users\admin\AppData\Local\Android\Sdk'
$env:ANDROID_SDK_ROOT=$env:ANDROID_HOME

gradle -p android :app:assembleDebug
```

APK 输出：

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

仓库不提交 APK、Gradle 缓存、CMake 缓存和 `.so` 文件。需要先生成或复制 `libhonta_native.so` 到 `jniLibs` 后再构建 APK。

## 已验证

- Android Gradle 构建通过。
- JNI C++ bridge 编译通过。
- App 在 x86_64 模拟器安装和启动通过。
- JNI smoke test 通过。
- mock candidate 下入口确认、中心确认、坐标计算、坐标查询通过。

## 未验证或待继续

- arm64 真机安装。
- Android 真实 OpenCV/FFmpeg 依赖完整性。
- native RTSP 输入和画圈 RTSP 输出。
- 真实 candidate id 从视频画面选择。
- AMR/NEST 实时位姿接入。
- Modbus TCP 真实上位机联调。

## 后续 Android 开发建议

Android 工程师可先并行开发：

- 配置页面。
- 标定流程 UI。
- RTSP 播放容器。
- AMR 状态展示。
- Modbus 状态展示。
- 日志诊断页面。

算法/native 侧继续补齐：

- Android OpenCV/FFmpeg 依赖。
- 真实 RTSP detection 版 `libhonta_native.so`。
- 真机 `arm64-v8a` 安装验证。
