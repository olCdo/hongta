#include "honta/api/honta_api.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string getLastError() {
    int required_size = 0;
    const int size_status =
        honta_get_last_error(nullptr, 0, &required_size);
    if (size_status != HONTA_ERROR_BUFFER_TOO_SMALL || required_size <= 0) {
        return {};
    }

    std::vector<char> buffer(static_cast<std::size_t>(required_size));
    if (honta_get_last_error(
            buffer.data(),
            static_cast<int>(buffer.size()),
            &required_size) != HONTA_OK) {
        return {};
    }
    return buffer.data();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "runtime config path is required\n";
        return 1;
    }

    if (honta_init(argv[1]) != HONTA_OK) {
        std::cerr << "honta_init failed: " << getLastError() << '\n';
        honta_release();
        return 1;
    }
    if (honta_start_detect("entrance_hole") != HONTA_OK) {
        std::cerr << "honta_start_detect failed: " << getLastError() << '\n';
        honta_release();
        return 1;
    }

    std::string error;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        error = getLastError();
        if (!error.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const int stop_status = honta_stop_detect();
    honta_release();

    if (stop_status != HONTA_OK) {
        std::cerr << "honta_stop_detect failed\n";
        return 1;
    }
    if (error.find("failed to open RTSP input") == std::string::npos) {
        std::cerr << "background error was not propagated: " << error << '\n';
        return 1;
    }
    return 0;
}
