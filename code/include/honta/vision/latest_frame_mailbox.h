#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

#include "honta/vision/ffmpeg_rtsp_input.h"

namespace honta::vision {

enum class LatestFrameWaitResult {
    Frame,
    Timeout,
    Stopped,
    Error,
};

class LatestFrameMailbox {
public:
    void reset();
    void publish(DecodedFrame frame);
    void fail(std::string error);
    void stop();

    LatestFrameWaitResult waitNext(DecodedFrame& output,
                                   std::uint64_t& cursor,
                                   std::uint64_t& skipped_frames,
                                   std::chrono::milliseconds timeout);
    std::string lastError() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    DecodedFrame latest_frame_;
    std::uint64_t sequence_ = 0;
    bool stopped_ = false;
    std::string last_error_;
};

}  // namespace honta::vision
