#include "honta/vision/latest_frame_mailbox.h"

#include <utility>

namespace honta::vision {

void LatestFrameMailbox::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_frame_ = {};
    sequence_ = 0;
    stopped_ = false;
    last_error_.clear();
}

void LatestFrameMailbox::publish(DecodedFrame frame) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        latest_frame_ = std::move(frame);
        ++sequence_;
    }
    condition_.notify_all();
}

void LatestFrameMailbox::fail(std::string error) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = std::move(error);
    }
    condition_.notify_all();
}

void LatestFrameMailbox::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    condition_.notify_all();
}

LatestFrameWaitResult LatestFrameMailbox::waitNext(
    DecodedFrame& output,
    std::uint64_t& cursor,
    std::uint64_t& skipped_frames,
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool ready = condition_.wait_for(lock, timeout, [&]() {
        return sequence_ > cursor || !last_error_.empty() || stopped_;
    });
    if (!ready) {
        return LatestFrameWaitResult::Timeout;
    }
    if (sequence_ > cursor) {
        skipped_frames = sequence_ - cursor - 1;
        cursor = sequence_;
        output = latest_frame_;
        return LatestFrameWaitResult::Frame;
    }
    if (!last_error_.empty()) {
        return LatestFrameWaitResult::Error;
    }
    return LatestFrameWaitResult::Stopped;
}

std::string LatestFrameMailbox::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

}  // namespace honta::vision
