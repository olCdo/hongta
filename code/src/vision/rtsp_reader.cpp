#include "honta/vision/rtsp_reader.h"

#include <cstdlib>
#include <thread>
#include <utility>

namespace honta::vision {

RtspReader::RtspReader(RtspReaderConfig config) : config_(std::move(config)) {}

RtspReader::~RtspReader() {
    close();
}

bool RtspReader::open() {
    close();
    if (config_.url.empty()) {
        last_error_ = "empty input url";
        return false;
    }

    if (config_.low_latency) {
        // Applies to OpenCV's FFmpeg backend. UDP avoids TCP retransmission latency;
        // nobuffer/low_delay keeps the capture close to the live frame.
        _putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS",
                  "rtsp_transport;udp|fflags;nobuffer|flags;low_delay|max_delay;0|stimeout;3000000");
        config_.api_preference = cv::CAP_FFMPEG;
    }

    capture_.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, config_.open_timeout_ms);
    capture_.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, config_.read_timeout_ms);
    capture_.set(cv::CAP_PROP_BUFFERSIZE, config_.buffer_size);

    if (!capture_.open(config_.url, config_.api_preference)) {
        last_error_ = "failed to open input: " + config_.url;
        return false;
    }

    capture_.set(cv::CAP_PROP_BUFFERSIZE, config_.buffer_size);
    last_error_.clear();
    return true;
}

void RtspReader::close() {
    if (capture_.isOpened()) {
        capture_.release();
    }
}

bool RtspReader::isOpened() const {
    return capture_.isOpened();
}

bool RtspReader::read(cv::Mat& frame) {
    frame.release();

    if (!capture_.isOpened() && !reopen()) {
        return false;
    }

    if (capture_.read(frame) && !frame.empty()) {
        return true;
    }

    last_error_ = "failed to read frame";
    close();
    return reopen() && capture_.read(frame) && !frame.empty();
}

const std::string& RtspReader::lastError() const {
    return last_error_;
}

bool RtspReader::shouldReconnect() const {
    if (last_reconnect_time_ == std::chrono::steady_clock::time_point{}) {
        return true;
    }
    const auto elapsed = std::chrono::steady_clock::now() - last_reconnect_time_;
    return elapsed >= std::chrono::milliseconds(config_.reconnect_interval_ms);
}

bool RtspReader::reopen() {
    if (!shouldReconnect()) {
        return false;
    }

    for (int attempt = 0; attempt < config_.max_reconnect_attempts; ++attempt) {
        last_reconnect_time_ = std::chrono::steady_clock::now();
        if (open()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
    }

    return false;
}

LatestFrameReader::LatestFrameReader(RtspReaderConfig config)
    : config_(std::move(config)), reader_(config_) {}

LatestFrameReader::~LatestFrameReader() {
    stop();
}

bool LatestFrameReader::start() {
    stop();
    running_ = true;
    worker_ = std::thread(&LatestFrameReader::loop, this);
    return true;
}

void LatestFrameReader::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool LatestFrameReader::readLatest(cv::Mat& frame, int& frame_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_frame_ || latest_frame_.empty()) {
        return false;
    }
    latest_frame_.copyTo(frame);
    frame_id = latest_frame_id_;
    return true;
}

const std::string& LatestFrameReader::lastError() const {
    return last_error_;
}

void LatestFrameReader::loop() {
    cv::Mat frame;
    while (running_) {
        if (!reader_.read(frame)) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_error_ = reader_.lastError();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            frame.copyTo(latest_frame_);
            ++latest_frame_id_;
            has_frame_ = true;
            last_error_.clear();
        }
    }
}

}  // namespace honta::vision
