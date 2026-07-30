# 第七阶段 Android 交付包说明

## 包内容

```text
android/                              Android 工程源码
apk/app-debug.apk                     已构建 debug APK，可直接安装验证
native/phase7_android_real_native.zip 真实 native 库压缩包
headers/honta_api.h                   C ABI 头文件
docs/                                 API、交接、构建、测试记录文档
tools/                                复现构建和 Python RTSP 测试脚本
```

如果以 GitHub 仓库形式交接，APK、native 压缩包和本地生成的 `.so` 默认不提交；需要通过 release 附件、网盘或现场构建脚本单独交付。

## 已验证

- Android 平板 `P85T(P3M5)`，Android 14，`arm64-v8a`。
- Python RTSP 测试流输入。
- `libhonta_native.so` 真实 OpenCV/FFmpeg 检测。
- JNI 调用链。
- overlay RTSP 输出。
- 入口确认、中心确认、坐标计算、坐标读取。

真机测试结果：

```text
JNI smoke test: PASS
```

## 调试运行方式

启动本机 MediaMTX 和 Python RTSP 测试流后，通过 USB 反向端口：

```powershell
adb reverse tcp:8564 tcp:8564
```

当前测试配置使用：

```text
rtsp://127.0.0.1:8564/honta_test
```

这是 USB 调试用法。现场环境需要改成真实相机或现场 RTSP 地址。

## Android 工程师优先阅读

```text
docs/phase7_android_api_reference.md
docs/phase7_android_real_native_handoff.md
docs/phase7_android_tablet_usb_rtsp_test_log.md
```

在 GitHub 仓库中，对应路径为：

```text
materials/docs/phase7_android_api_reference.md
materials/docs/phase7_android_real_native_handoff.md
materials/docs/phase7_android_tablet_usb_rtsp_test_log.md
```

## 当前限制

当前 Java API 还没有正式候选列表和检测快照接口。调试页通过重试固定 `candidateId=1` 验证链路；正式 UI 应在 native 补充候选列表 API 后再实现用户选择交互。
