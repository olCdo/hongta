# Honta AMR Vision

Honta AMR Vision 是一个面向 AMR 顶盖标定和喇叭口坐标换算的原生视觉模块。当前仓库包含 C++17 核心库、PC 侧验证工具、Android JNI 集成骨架和阶段性联调文档。

## 业务背景

现场由基座和顶盖组成。基座俯视为环形或圆形，AMR 会用激光扫描形成地图；顶盖为空心半球，像碗一样倒扣在基座上。顶盖上有同高度安装、开口朝下的喇叭口，边缘分布一圈通孔。

每次任务前顶盖都会重新倒扣到基座上，喇叭口相对 AMR 地图的位置会发生变化，主要体现为绕圆心的旋转。因此系统需要先通过视觉标定把顶盖图纸坐标映射到 AMR 地图坐标，再把任务中的靶球序号转换为 AMR 可导航的目标点。

## 当前能力

- 读取运行配置、相机内参、顶盖模型配置。
- 从 RTSP 或 USB 相机输入中识别圆形目标。
- 支持入口通孔和中心喇叭口的人工确认链路。
- 根据入口点、中心点和顶盖模型计算全部喇叭口地图坐标。
- 提供 C ABI 动态库 `honta_native`，用于 Android 或其他上层系统集成。
- 提供 Android JNI smoke-test 工程，方便 Android 工程师先行接入。

## 仓库结构

```text
code/include/                 C++ 公共头文件和 C ABI
code/src/                     C++ 核心实现
code/tools/                   标定和 RTSP 测试辅助工具
code/configs/                 示例运行配置、相机配置、顶盖模型
android/                      Android JNI 集成工程
materials/docs/               当前接口、集成和交付文档
materials/assets/             需求和识别参考素材
tools/                        构建、数据生成、RTSP 测试脚本
vcpkg.json                    PC/Android 原生依赖清单
CMakeLists.txt                C++ 顶层构建入口
```

## Android 工程师入口

如果只是交付给 Android 工程师接入，优先使用：

- `android_handoff/`

该目录只包含 `.so` 放置位置、程序配置文件、C API 头文件、Java 封装示例和 API 说明。

优先阅读：

- `android/README.md`
- `materials/docs/phase7_android_api_reference.md`
- `materials/docs/phase7_android_engineer_handoff.md`
- `materials/docs/phase7_android_real_native_handoff.md`
- `materials/docs/phase7_android_tablet_usb_rtsp_test_log.md`

Android 工程在 `android/` 下，当前是一个最小 JNI smoke-test app。仓库不提交本地生成的 `.so`、APK、Gradle 缓存和 CMake 缓存；真实原生库请通过 `tools/build_android_real_native.ps1` 构建，或按交付文档复制到 `android/app/src/main/jniLibs/<abi>/`。

## PC 侧构建

依赖：

- CMake 3.16+
- C++17 编译器
- OpenCV
- FFmpeg development libraries
- vcpkg（推荐使用 manifest 模式）

示例：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>\scripts\buildsystems\vcpkg.cmake
cmake --build build
```

常用构建目标包括 `honta_native`、`honta_config_model`、`honta_calibration`、`honta_vision`、`calibrate_camera_tool` 和 `capture_calibration_images_tool`。

## Android 原生库构建

Windows 本地脚本入口：

```powershell
.\tools\build_android_real_native.ps1 -Abi arm64-v8a -CopyToAndroidProject
```

脚本默认读取本机 Android SDK/NDK 和 `.codex_tmp\vcpkg`。在其他机器上使用时，请传入对应参数：

```powershell
.\tools\build_android_real_native.ps1 `
  -Abi all `
  -AndroidNdkHome <ndk-path> `
  -CMakeExe <cmake.exe-path> `
  -VcpkgRoot <vcpkg-root> `
  -CopyToAndroidProject
```

## GitHub 发布注意事项

应提交源码、配置、文档和脚本；不提交以下本地产物：

- `.codex_tmp/`
- `local_archive/`
- `build*/`
- `output/`
- `render_tmp/`
- `vcpkg_installed/`
- `android/.gradle/`
- `android/app/.cxx/`
- `android/app/build/`
- `android/app/src/main/jniLibs/**/*.so`

发布前建议执行：

```powershell
git status --short
git status --ignored --short
```
