# Honta Android Integration

本目录是 Android 工程师的集成入口。当前工程是一个最小 JNI smoke-test app，用来验证 Java 层、JNI bridge 和 `honta_native` C ABI 的调用链，不是最终业务 UI。

## 当前范围

- `app/src/main/java/com/honta/vision/HontaNative.java`：Java 层 native API 封装。
- `HontaNativeController`：用单工作线程串行化 native 生命周期调用。
- `AmrPoseValidator`：AMR 位姿有效性校验。
- `ModbusCommandGate`：冷启动和重复指令保护。
- `CoordinateTableMetadata`：坐标表失效判定字段。
- `app/src/main/cpp/HontaNativeBridge.cpp`：JNI 到 `honta_api.h` 的薄封装。
- `app/src/main/assets/phase7_smoke/`：JNI smoke test 使用的示例配置。
- `app/src/main/jniLibs/<abi>/`：本地放置 `libhonta_native.so` 和依赖库的位置。

## 构建前置条件

安装 Android Studio 或等价命令行工具，并准备：

- Android SDK
- Android NDK
- Gradle 或 Android Studio Gradle 集成
- 对应 ABI 的 `libhonta_native.so`
- 真实 RTSP 检测链路所需的 OpenCV、FFmpeg、`c++_shared` 等 native 依赖

仓库默认不提交 `.so`。如果 `app/src/main/jniLibs/<abi>/libhonta_native.so` 不存在，JNI bridge 的 CMake 配置会直接失败，避免生成一个运行时才崩溃的 APK。

## 推荐 smoke test

1. 构建或复制 `libhonta_native.so` 到 `app/src/main/jniLibs/arm64-v8a/`。
2. 如需模拟器验证，也复制到 `app/src/main/jniLibs/x86_64/`。
3. 构建 debug APK。
4. 安装并启动 app。
5. Debug 页面会自动执行：

```text
init
  -> setTopCoverModel
  -> getOverlayRtspUrl
  -> start entrance mock
  -> confirm entrance
  -> start center mock
  -> confirm center
  -> calculate all horns
  -> get horn coordinate
  -> release
```

通过标志：

```text
JNI smoke test: PASS
```

## 构建命令

本机 Windows 示例：

```powershell
$env:JAVA_HOME='D:\Program Files\Android\Android Studio\jbr'
$env:ANDROID_HOME='C:\Users\admin\AppData\Local\Android\Sdk'
$env:ANDROID_SDK_ROOT=$env:ANDROID_HOME

gradle -p android :app:assembleDebug
```

如果本机没有全局 `gradle`，可用 Android Studio 打开 `android/` 后通过 IDE 构建，或自行安装 Gradle 8.x。

## 原生库生成

推荐从仓库根目录运行：

```powershell
.\tools\build_android_real_native.ps1 -Abi arm64-v8a -CopyToAndroidProject
```

脚本会构建 `honta_native` 并复制到 Android 工程的 `jniLibs` 目录。不同机器上需要按实际 SDK/NDK/vcpkg 路径传参。

## 集成规则

- JNI 层保持为 `honta_api.h` 的薄封装。
- Android JNI 代码不要包含内部 C++ 头文件。
- 不要在 UI 线程直接执行耗时 native 调用。
- `init/start/stop/release` 必须串行调用。
- 页面销毁或 app 退出时必须调用 `release()`。
- 冷启动时等待新的 Modbus 指令，不自动消费旧目标寄存器值。

## 进一步阅读

- `../materials/docs/phase7_android_api_reference.md`
- `../materials/docs/phase7_android_engineer_handoff.md`
- `../materials/docs/phase7_android_real_native_handoff.md`
- `../materials/docs/phase7_android_tablet_usb_rtsp_test_log.md`
