#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

namespace honta::vision {

struct RtspReaderConfig {
    std::string url;
    int api_preference = cv::CAP_ANY;
    int open_timeout_ms = 5000;
    int read_timeout_ms = 3000;
    int reconnect_interval_ms = 1000;
    int max_reconnect_attempts = 3;
    bool low_latency = true;
    int buffer_size = 1;
};

class RtspReader {
public:
    explicit RtspReader(RtspReaderConfig config);
    ~RtspReader();

    RtspReader(const RtspReader&) = delete;
    RtspReader& operator=(const RtspReader&) = delete;

    bool open();
    void close();
    bool isOpened() const;
    bool read(cv::Mat& frame);

    const std::string& lastError() const;

private:
    RtspReaderConfig config_;
    cv::VideoCapture capture_;
    std::string last_error_;
    std::chrono::steady_clock::time_point last_reconnect_time_{};

    bool shouldReconnect() const;
    bool reopen();
};

class LatestFrameReader {
public:
    explicit LatestFrameReader(RtspReaderConfig config);
    ~LatestFrameReader();

    LatestFrameReader(const LatestFrameReader&) = delete;
    LatestFrameReader& operator=(const LatestFrameReader&) = delete;

    bool start();
    void stop();
    bool readLatest(cv::Mat& frame, int& frame_id);
    const std::string& lastError() const;

private:
    void loop();

    RtspReaderConfig config_;
    RtspReader reader_;
    std::thread worker_;
    mutable std::mutex mutex_;
    cv::Mat latest_frame_;
    std::string last_error_;
    int latest_frame_id_ = 0;
    bool running_ = false;
    bool has_frame_ = false;
};

}  // namespace honta::vision
