# Android 真实 native 库交付说明

## 当前状态

Android 版 `libhonta_native.so` 已支持真实 OpenCV/FFmpeg 检测链路。目标 ABI：

- `arm64-v8a`：Android 真机。
- `x86_64`：Android Emulator。

真实检测构建应关闭 mock：

```text
HONTA_ENABLE_MOCK_DETECTION=OFF
```

CMake 找到 Android 版 OpenCV 和 FFmpeg 后，会启用真实检测宏：

```text
HONTA_HAS_REAL_DETECTION
```

## 交付产物

真实库默认输出目录：

```text
output/phase7_android_real_native/
  arm64-v8a/libhonta_native.so
  x86_64/libhonta_native.so
```

GitHub 仓库不提交 `output/` 和 `.so` 文件。需要交付二进制时，建议通过 GitHub Release 附件、网盘或现场构建脚本单独传递。

## 依赖形态

当前交付形态以 `libhonta_native.so` 统一承载 OpenCV/FFmpeg 相关 native 代码。运行时仍依赖 Android 系统库，例如：

```text
libcamera2ndk.so
libandroid.so
libmediandk.so
liblog.so
libdl.so
libm.so
libc.so
```

Android 工程师接入时，核心动作是把对应 ABI 的 `libhonta_native.so` 放到：

```text
android/app/src/main/jniLibs/arm64-v8a/libhonta_native.so
android/app/src/main/jniLibs/x86_64/libhonta_native.so
```

## 构建命令

仓库根目录提供脚本：

```text
tools/build_android_real_native.ps1
```

构建两个 ABI：

```powershell
tools\build_android_real_native.ps1 -Abi all
```

只构建真机 ABI：

```powershell
tools\build_android_real_native.ps1 -Abi arm64-v8a
```

只构建模拟器 ABI：

```powershell
tools\build_android_real_native.ps1 -Abi x86_64
```

构建完成后直接覆盖 Android 工程 `jniLibs`：

```powershell
tools\build_android_real_native.ps1 -Abi all -CopyToAndroidProject
```

## Android 接入说明

替换成真实库后，`startDetect(...)` 会真正打开 `runtime.json` 中配置的 RTSP 输入。如果没有可访问的视频流，JNI smoke test 失败是符合预期的；需要在现场网络、真机和 RTSP 源可达后继续验证。

## 已验证和待验证

已验证：

- `arm64-v8a` 和 `x86_64` 的 OpenCV/FFmpeg 依赖安装。
- 两个 ABI 的 CMake configure。
- 两个 ABI 的 `libhonta_native.so` 构建。
- `llvm-readelf -d` 动态依赖检查。

待现场验证：

- 真机安装运行。
- Android 真实 RTSP 输入检测。
- Android overlay RTSP 输出播放。
