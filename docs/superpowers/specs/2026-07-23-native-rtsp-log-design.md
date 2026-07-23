# Android Native RTSP 日志设计

## 目标

为 `honta_native.so` 的 RTSP 输入、检测和发布链路增加可诊断日志。安卓设备连接 ADB 后，可在电脑上运行以下命令查看：

```powershell
adb logcat -s 'HontaNative:*' '*:S'
```

## 日志范围

统一使用 `HontaNative` 标签，记录：

- 检测任务开始、停止和后台线程退出；
- RTSP 输入连接开始、成功和重连次数；
- overlay publisher 打开开始、成功和写帧失败；
- FFmpeg 返回的具体错误；
- 后台错误状态及可供 `getLastError()` 查询的错误文本。

RTSP 地址在写入日志前必须隐藏用户名和密码。正常帧不逐帧打印，避免日志刷屏。

## 实现边界

- Android 使用系统 log 库输出到 logcat；
- 非 Android 平台输出到标准错误，便于本机测试；
- 日志封装位于 native 公共层，业务代码不直接依赖 Android 头文件；
- 后台检测错误通过线程安全的状态快照返回，避免直接并发读取字符串；
- 不开启 FFmpeg 全量调试日志。

## 测试

- 单元测试验证 RTSP 凭据脱敏；
- 单元测试验证日志级别和消息转发；
- 编译 Android arm64-v8a 的 `libhonta_native.so`；
- 编译 debug APK；
- 有可用安卓设备时安装 APK，运行后通过 `adb logcat` 验证日志标签和错误内容；
- 检查日志中不包含 RTSP 密码。

## GitHub 发布

继续使用 `codex/android-handoff-package` 分支。只提交构建所需源码、测试、Android 工程和文档，不提交 `.so`、APK、构建目录、证书、私钥或临时文件。推送后创建草稿 PR。
