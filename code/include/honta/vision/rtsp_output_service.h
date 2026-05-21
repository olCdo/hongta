#pragma once

#include <cstdint>
#include <string>

#include <opencv2/core.hpp>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct AVStream;
struct SwsContext;

namespace honta::vision {

struct OverlayRtspConfig {
    std::string bind_ip = "0.0.0.0";
    std::string public_host = "127.0.0.1";
    int port = 8554;
    std::string path = "/honta_overlay";
    int fps = 25;
    int bitrate = 2000000;
    std::string rtsp_transport = "tcp";
};

class InternalRtspOutputService {
public:
    explicit InternalRtspOutputService(OverlayRtspConfig config = {});
    ~InternalRtspOutputService();

    InternalRtspOutputService(const InternalRtspOutputService&) = delete;
    InternalRtspOutputService& operator=(const InternalRtspOutputService&) = delete;

    std::string playbackUrl() const;
    bool open(int width, int height);
    bool write(const cv::Mat& bgr_frame);
    void close();
    bool isOpen() const;
    const std::string& lastError() const;

private:
    bool openEncoder(int width, int height);
    bool writeEncodedFrame(AVFrame* frame);
    bool convertBgrToYuv(const cv::Mat& bgr_frame);
    void setError(const std::string& prefix, int ffmpeg_error);

    OverlayRtspConfig config_;
    AVFormatContext* format_context_ = nullptr;
    AVCodecContext* codec_context_ = nullptr;
    AVStream* stream_ = nullptr;
    AVFrame* yuv_frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* sws_context_ = nullptr;
    int64_t next_pts_ = 0;
    std::string last_error_;
};

}  // namespace honta::vision
