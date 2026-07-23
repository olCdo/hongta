# Android Native RTSP 日志实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 `honta_native.so` 增加安全、可筛选的 RTSP 发布日志，并让后台 publisher 错误可通过 `getLastError()` 获取。

**Architecture:** 新增跨平台 `honta_logging` 小组件，Android 写入 `logcat`，Windows 写入标准错误。`DetectionService` 只记录连接、发布、重连和错误等边界事件，通过错误回调把后台失败同步到线程安全的 `HontaContext`。

**Tech Stack:** C++17、CMake/CTest、Android NDK log、Gradle、ADB、Git/GitHub CLI。

---

### Task 1: 日志脱敏测试

**Files:**
- Create: `code/tests/native_log_test.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: 写入失败测试**

创建一个无第三方测试框架的 CTest 可执行文件，断言：

```cpp
expectEqual(redactRtspCredentials(
                "rtsp://viewer:test-secret@192.0.2.10:554/11"),
            "rtsp://***@192.0.2.10:554/11");
expectEqual(redactRtspCredentials(
                "rtsp://192.168.0.122:8554/honta_overlay"),
            "rtsp://192.168.0.122:8554/honta_overlay");
expectEqual(redactRtspCredentials("not-an-rtsp-url"),
            "not-an-rtsp-url");
```

- [x] **Step 2: 运行测试并确认失败**

```powershell
cmake -S . -B .codex_tmp/native_log_test -DBUILD_TESTING=ON
cmake --build .codex_tmp/native_log_test --target honta_native_log_test
```

预期：因 `honta/logging/native_log.h` 或 `redactRtspCredentials` 尚不存在而失败。

### Task 2: 跨平台日志组件

**Files:**
- Create: `code/include/honta/logging/native_log.h`
- Create: `code/src/logging/native_log.cpp`
- Modify: `CMakeLists.txt`
- Test: `code/tests/native_log_test.cpp`

- [x] **Step 1: 定义日志接口**

```cpp
namespace honta::logging {

std::string redactRtspCredentials(std::string_view value);
void info(std::string_view component, std::string_view message);
void warning(std::string_view component, std::string_view message);
void error(std::string_view component, std::string_view message);

}  // namespace honta::logging
```

- [x] **Step 2: 实现平台输出**

Android 使用：

```cpp
__android_log_print(priority,
                    "HontaNative",
                    "%.*s: %.*s",
                    static_cast<int>(component.size()),
                    component.data(),
                    static_cast<int>(message.size()),
                    message.data());
```

非 Android 使用 `std::cerr`。脱敏函数只替换 RTSP authority 中 `@` 前的用户信息，不改变主机、端口和路径。

- [x] **Step 3: 接入 CMake**

新增 `honta_logging` 静态库；Android 下 `find_library(ANDROID_LOG_LIBRARY log)` 并链接系统 log 库。`honta_vision` 和 `honta_native` 私有链接该库。仅在 `BUILD_TESTING AND NOT ANDROID` 时构建并注册 `honta_native_log_test`。

- [x] **Step 4: 运行测试**

```powershell
cmake -S . -B .codex_tmp/native_log_test -DBUILD_TESTING=ON
cmake --build .codex_tmp/native_log_test --target honta_native_log_test
ctest --test-dir .codex_tmp/native_log_test --output-on-failure
```

预期：`honta_native_log_test` 通过，且日志测试不依赖 OpenCV 或 FFmpeg。

### Task 3: Publisher 生命周期日志和后台错误透传

**Files:**
- Create: `code/tests/detection_service_error_test.cpp`
- Modify: `code/include/honta/vision/detection_service.h`
- Modify: `code/src/vision/detection_service.cpp`
- Modify: `code/src/api/honta_context.h`
- Modify: `code/src/api/honta_context.cpp`
- Modify: `code/src/api/honta_api.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: 写入错误回调失败测试**

配置 `DetectionServiceConfig` 的空输入地址和有效检测 profile，设置：

```cpp
std::string callback_error;
config.on_error = [&](const std::string& value) {
    callback_error = value;
};
```

断言 `startDetect("entrance_hole")` 返回 `false`，并且回调与 `lastError()` 都得到 `empty input RTSP url`。预期首次编译失败，因为 `on_error` 尚不存在。

- [x] **Step 2: 实现错误回调和线程安全读取**

在 `DetectionServiceConfig` 增加：

```cpp
std::function<void(const std::string&)> on_error;
```

`DetectionService::setError()` 更新 `DetectionSession`、输出 `HontaNative` ERROR 日志并调用回调。`lastError()` 改为按值返回 `session_.snapshot().last_error`，删除并发访问的裸 `last_error_` 字符串。

`HontaContext` 使用互斥锁保护 `context_last_error_`，`lastError()` 按值返回。在 `configureDetectionService()` 中把 `setLastError()` 注册为错误回调。`honta_get_last_error()` 每次读取前从 context 刷新 `g_last_error`。

- [x] **Step 3: 增加边界日志**

只记录以下事件，不逐帧输出：

```text
start detect_type=...
opening input=rtsp://***@host/path
input opened
reconnecting attempt=N
opening publisher=rtsp://host:8554/honta_overlay
publisher online
publisher write failed
worker stopped
```

所有 RTSP 地址调用 `redactRtspCredentials()`。

- [x] **Step 4: 运行单元测试**

```powershell
cmake --build .codex_tmp/native_log_test
ctest --test-dir .codex_tmp/native_log_test --output-on-failure
```

预期：脱敏测试和 detection service 错误回调测试全部通过。

### Task 4: Android 构建、设备验证和 GitHub 发布

**Files:**
- Modify only files created or changed in Tasks 1-3
- Do not stage: `auto.key`, `auto.crt`, `.so`, APK、`.codex_tmp/` 或无关工作区修改

- [x] **Step 1: 构建 arm64 native 库**

```powershell
.\tools\build_android_real_native.ps1 -Abi arm64-v8a -CopyToAndroidProject
```

预期：生成 `libhonta_native.so` 并复制到 Android 工程的本地 `jniLibs`，该二进制不提交。

- [x] **Step 2: 构建 debug APK**

```powershell
$env:JAVA_HOME='D:\Program Files\Android\Android Studio\jbr'
$env:ANDROID_HOME='C:\Users\admin\AppData\Local\Android\Sdk'
gradle -p android :app:assembleDebug
```

预期：`BUILD SUCCESSFUL`。

- [ ] **Step 3: 验证设备日志**

```powershell
adb devices
adb logcat -c
adb logcat -s 'HontaNative:*' '*:S'
```

启动 App 并运行检测。预期可见输入打开和 publisher 结果；输出不得包含配置中的 RTSP 凭据。

当前 `adb devices -l` 未发现已连接设备，因此真机 logcat 验证待设备连接后执行。

- [x] **Step 4: 检查并提交精选文件**

```powershell
git diff --check
git status --short
git add -- <本计划明确列出的源码、测试、CMake 和文档文件>
git diff --cached --check
git commit -m "feat: add native RTSP diagnostics / 功能：新增原生RTSP诊断日志"
```

- [x] **Step 5: 推送并创建草稿 PR**

```powershell
git push -u origin codex/android-handoff-package
gh pr create --draft --fill
```

PR 说明必须包含修改内容、publisher 无日志的根因、Android 使用方式，以及实际执行的测试命令。
