#include "honta/vision/ffmpeg_rtsp_input.h"

#include <sstream>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace honta::vision {
namespace {

std::string ffmpegErrorText(int error) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return buffer;
}

}  // namespace

FfmpegRtspInput::FfmpegRtspInput(FfmpegRtspInputConfig config) : config_(std::move(config)) {}

FfmpegRtspInput::~FfmpegRtspInput() {
    close();
}

bool FfmpegRtspInput::open(std::atomic_bool* stop_requested) {
    close();
    last_error_.clear();

    if (config_.url.empty()) {
        last_error_ = "empty RTSP input url";
        return false;
    }

    interrupt_state_.stop_requested = stop_requested;
    beginBlockingOperation(config_.open_timeout_ms);

    format_context_ = avformat_alloc_context();
    if (format_context_ == nullptr) {
        last_error_ = "failed to allocate AVFormatContext";
        return false;
    }
    format_context_->interrupt_callback.callback = &FfmpegRtspInput::interruptCallback;
    format_context_->interrupt_callback.opaque = &interrupt_state_;

    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", config_.rtsp_transport.c_str(), 0);
    av_dict_set(&options, "stimeout", std::to_string(config_.open_timeout_ms * 1000).c_str(), 0);
    av_dict_set(&options, "rw_timeout", std::to_string(config_.read_timeout_ms * 1000).c_str(), 0);
    if (config_.low_latency) {
        av_dict_set(&options, "fflags", "nobuffer", 0);
        av_dict_set(&options, "flags", "low_delay", 0);
        av_dict_set(&options, "max_delay", "0", 0);
    }

    int result = avformat_open_input(&format_context_, config_.url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (result < 0) {
        setError("failed to open RTSP input", result);
        close();
        return false;
    }

    beginBlockingOperation(config_.open_timeout_ms);
    result = avformat_find_stream_info(format_context_, nullptr);
    if (result < 0) {
        setError("failed to find RTSP stream info", result);
        close();
        return false;
    }

    if (!openDecoder()) {
        close();
        return false;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (packet_ == nullptr || frame_ == nullptr) {
        last_error_ = "failed to allocate FFmpeg packet/frame";
        close();
        return false;
    }

    return true;
}

bool FfmpegRtspInput::read(DecodedFrame& output) {
    output = {};
    if (!isOpen()) {
        last_error_ = "RTSP input is not open";
        return false;
    }

    while (true) {
        beginBlockingOperation(config_.read_timeout_ms);
        int result = av_read_frame(format_context_, packet_);
        if (result < 0) {
            setError("failed to read RTSP packet", result);
            return false;
        }

        if (packet_->stream_index != video_stream_index_) {
            av_packet_unref(packet_);
            continue;
        }

        result = avcodec_send_packet(codec_context_, packet_);
        av_packet_unref(packet_);
        if (result < 0) {
            setError("failed to send packet to decoder", result);
            return false;
        }

        result = avcodec_receive_frame(codec_context_, frame_);
        if (result == AVERROR(EAGAIN)) {
            continue;
        }
        if (result == AVERROR_EOF) {
            last_error_ = "decoder reached EOF";
            return false;
        }
        if (result < 0) {
            setError("failed to receive decoded frame", result);
            return false;
        }

        const bool converted = convertFrame(frame_, output);
        av_frame_unref(frame_);
        return converted;
    }
}

void FfmpegRtspInput::close() {
    if (sws_context_ != nullptr) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
    if (frame_ != nullptr) {
        av_frame_free(&frame_);
    }
    if (packet_ != nullptr) {
        av_packet_free(&packet_);
    }
    if (codec_context_ != nullptr) {
        avcodec_free_context(&codec_context_);
    }
    if (format_context_ != nullptr) {
        avformat_close_input(&format_context_);
        format_context_ = nullptr;
    }
    video_stream_index_ = -1;
}

bool FfmpegRtspInput::isOpen() const {
    return format_context_ != nullptr && codec_context_ != nullptr && video_stream_index_ >= 0;
}

const std::string& FfmpegRtspInput::lastError() const {
    return last_error_;
}

int FfmpegRtspInput::interruptCallback(void* opaque) {
    auto* state = static_cast<InterruptState*>(opaque);
    if (state == nullptr) {
        return 0;
    }
    if (state->stop_requested != nullptr && state->stop_requested->load()) {
        return 1;
    }
    if (state->timeout_ms > 0) {
        const auto elapsed = std::chrono::steady_clock::now() - state->operation_start;
        if (elapsed >= std::chrono::milliseconds(state->timeout_ms)) {
            return 1;
        }
    }
    return 0;
}

void FfmpegRtspInput::beginBlockingOperation(int timeout_ms) {
    interrupt_state_.operation_start = std::chrono::steady_clock::now();
    interrupt_state_.timeout_ms = timeout_ms;
}

bool FfmpegRtspInput::openDecoder() {
    const AVCodec* decoder = nullptr;
    const int stream_index = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (stream_index < 0) {
        setError("failed to find video stream", stream_index);
        return false;
    }

    AVStream* stream = format_context_->streams[stream_index];
    codec_context_ = avcodec_alloc_context3(decoder);
    if (codec_context_ == nullptr) {
        last_error_ = "failed to allocate decoder context";
        return false;
    }

    int result = avcodec_parameters_to_context(codec_context_, stream->codecpar);
    if (result < 0) {
        setError("failed to copy decoder parameters", result);
        return false;
    }

    codec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    result = avcodec_open2(codec_context_, decoder, nullptr);
    if (result < 0) {
        setError("failed to open video decoder", result);
        return false;
    }

    video_stream_index_ = stream_index;
    return true;
}

bool FfmpegRtspInput::convertFrame(AVFrame* frame, DecodedFrame& output) {
    if (frame == nullptr || frame->width <= 0 || frame->height <= 0) {
        last_error_ = "invalid decoded frame";
        return false;
    }

    sws_context_ = sws_getCachedContext(sws_context_,
                                        frame->width,
                                        frame->height,
                                        static_cast<AVPixelFormat>(frame->format),
                                        frame->width,
                                        frame->height,
                                        AV_PIX_FMT_BGR24,
                                        SWS_BILINEAR,
                                        nullptr,
                                        nullptr,
                                        nullptr);
    if (sws_context_ == nullptr) {
        last_error_ = "failed to create swscale context";
        return false;
    }

    output.bgr.create(frame->height, frame->width, CV_8UC3);
    uint8_t* dst_data[4] = {output.bgr.data, nullptr, nullptr, nullptr};
    int dst_linesize[4] = {static_cast<int>(output.bgr.step), 0, 0, 0};
    const int scaled = sws_scale(sws_context_,
                                 frame->data,
                                 frame->linesize,
                                 0,
                                 frame->height,
                                 dst_data,
                                 dst_linesize);
    if (scaled != frame->height) {
        last_error_ = "failed to convert decoded frame to BGR";
        return false;
    }

    output.width = frame->width;
    output.height = frame->height;
    output.pts = frame->pts;
    return true;
}

void FfmpegRtspInput::setError(const std::string& prefix, int ffmpeg_error) {
    std::ostringstream message;
    message << prefix << ": " << ffmpegErrorText(ffmpeg_error);
    last_error_ = message.str();
}

}  // namespace honta::vision
