#include "honta/logging/native_log.h"

#include <iostream>
#include <sstream>
#include <string>

namespace {

int failures = 0;

void expectEqual(const std::string& actual,
                 const std::string& expected,
                 const char* test_name) {
    if (actual == expected) {
        return;
    }
    ++failures;
    std::cerr << test_name << ": expected '" << expected
              << "', got '" << actual << "'\n";
}

}  // namespace

int main() {
    using honta::logging::redactRtspCredentials;

    expectEqual(
        redactRtspCredentials("rtsp://viewer:test-secret@192.0.2.10:554/11"),
        "rtsp://***@192.0.2.10:554/11",
        "credentials are redacted");
    expectEqual(
        redactRtspCredentials("rtsp://192.168.0.122:8554/honta_overlay"),
        "rtsp://192.168.0.122:8554/honta_overlay",
        "URL without credentials is unchanged");
    expectEqual(
        redactRtspCredentials("rtsps://admin:secret@camera.example/secure"),
        "rtsps://***@camera.example/secure",
        "secure RTSP credentials are redacted");
    expectEqual(
        redactRtspCredentials("not-an-rtsp-url"),
        "not-an-rtsp-url",
        "non-RTSP text is unchanged");

    std::ostringstream captured;
    std::streambuf* original_stderr = std::cerr.rdbuf(captured.rdbuf());
    honta::logging::info("Test", "connected");
    honta::logging::warning("Test", "retrying");
    honta::logging::error("Test", "failed");
    std::cerr.rdbuf(original_stderr);
    expectEqual(
        captured.str(),
        "[HontaNative][INFO] Test: connected\n"
        "[HontaNative][WARN] Test: retrying\n"
        "[HontaNative][ERROR] Test: failed\n",
        "desktop log levels are forwarded");

    return failures == 0 ? 0 : 1;
}
