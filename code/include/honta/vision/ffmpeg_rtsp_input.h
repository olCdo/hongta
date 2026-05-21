#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include <opencv2/core.hpp>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace honta::vision {

struct FfmpegRtspInputConfig {
    std::string url;
    std::string rtsp_transport = "tcp";
    int open_timeout_ms = 5000;
    int read_timeout_ms = 3000;
    bool low_latency = true;
};

struct DecodedFrame {
    cv::Mat bgr;
    int width = 0;
    int height = 0;
    int64_t pts = 0;
};

class FfmpegRtspInput {
public:
    explicit FfmpegRtspInput(FfmpegRtspInputConfig config = {});
    ~FfmpegRtspInput();

    FfmpegRtspInput(const FfmpegRtspInput&) = delete;
    FfmpegRtspInput& operator=(const FfmpegRtspInput&) = delete;

    bool open(std::atomic_bool* stop_requested = nullptr);
    bool read(DecodedFrame& output);
    void close();
    bool isOpen() const;

    const std::string& lastError() const;

private:
    struct InterruptState {
        std::atomic_bool* stop_requested = nullptr;
        std::chrono::steady_clock::time_point operation_start{};
        int timeout_ms = 0;
    };

    static int interruptCallback(void* opaque);
    void beginBlockingOperation(int timeout_ms);
    bool openDecoder();
    bool convertFrame(AVFrame* frame, DecodedFrame& output);
    void setError(const std::string& prefix, int ffmpeg_error);

    FfmpegRtspInputConfig config_;
    AVFormatContext* format_context_ = nullptr;
    AVCodecContext* codec_context_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;
    SwsContext* sws_context_ = nullptr;
    int video_stream_index_ = -1;
    std::string last_error_;
    InterruptState interrupt_state_;
};

}  // namespace honta::vision
