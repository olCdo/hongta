#include "honta/vision/detection_service.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

honta::vision::DetectionServiceConfig makeConfig() {
    honta::vision::DetectionServiceConfig config;
    honta::vision::DetectionProfile profile;
    profile.detect_type = "entrance_hole";
    config.profiles.emplace(profile.detect_type, profile);
    return config;
}

bool testSynchronousErrorCallback() {
    honta::vision::DetectionServiceConfig config = makeConfig();

    std::string callback_error;
    config.on_error = [&](const std::string& value) {
        callback_error = value;
    };

    honta::vision::DetectionService service;
    service.configure(std::move(config));

    if (service.startDetect("entrance_hole")) {
        std::cerr << "startDetect unexpectedly succeeded\n";
        return false;
    }

    const std::string expected = "empty input RTSP url";
    if (callback_error != expected) {
        std::cerr << "callback error: expected '" << expected
                  << "', got '" << callback_error << "'\n";
        return false;
    }
    if (service.lastError() != expected) {
        std::cerr << "lastError: expected '" << expected
                  << "', got '" << service.lastError() << "'\n";
        return false;
    }

    return true;
}

bool testThrowingCallbackIsContained() {
    honta::vision::DetectionServiceConfig config = makeConfig();
    config.on_error = [](const std::string&) {
        throw std::runtime_error("test callback failure");
    };

    honta::vision::DetectionService service;
    service.configure(std::move(config));
    try {
        if (service.startDetect("entrance_hole")) {
            std::cerr << "throwing callback startDetect unexpectedly succeeded\n";
            return false;
        }
    } catch (const std::exception& error) {
        std::cerr << "error callback exception escaped: " << error.what() << '\n';
        return false;
    }
    return true;
}

bool testBackgroundErrorCallback() {
    honta::vision::DetectionServiceConfig config = makeConfig();
    config.input.url = "unsupported-protocol://source";
    config.max_reconnect_attempts = 0;

    std::mutex mutex;
    std::condition_variable condition;
    std::string callback_error;
    config.on_error = [&](const std::string& value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_error = value;
        }
        condition.notify_one();
    };

    honta::vision::DetectionService service;
    service.configure(std::move(config));
    if (!service.startDetect("entrance_hole")) {
        std::cerr << "background error test did not start\n";
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex);
    const bool received = condition.wait_for(
        lock,
        std::chrono::seconds(5),
        [&]() { return !callback_error.empty(); });
    const std::string error = callback_error;
    lock.unlock();
    service.stopDetect();

    if (!received) {
        std::cerr << "background error callback timed out\n";
        return false;
    }
    if (error.find("failed to open RTSP input") == std::string::npos) {
        std::cerr << "unexpected background error: " << error << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    return testSynchronousErrorCallback() &&
                   testThrowingCallbackIsContained() &&
                   testBackgroundErrorCallback()
               ? 0
               : 1;
}
