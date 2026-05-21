#include "honta/vision/rtsp_output_service.h"

#include <sstream>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace honta::vision {
namespace {

std::string ffmpegErrorText(int error) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(error, buffer, sizeof(buffer));
    return buffer;
}

std::string normalizePath(const std::string& path) {
    if (path.empty()) {
        return "/honta_overlay";
    }
    return path.front() == '/' ? path : "/" + path;
}

bool supportsPixelFormat(const AVCodec* codec, AVPixelFormat pixel_format) {
    if (codec == nullptr || codec->pix_fmts == nullptr) {
        return true;
    }
    for (const AVPixelFormat* format = codec->pix_fmts; *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == pixel_format) {
            return true;
        }
    }
    return false;
}

const AVCodec* findEncoderWithPixelFormat(AVCodecID codec_id, AVPixelFormat pixel_format) {
    void* opaque = nullptr;
    const AVCodec* codec = nullptr;
    while ((codec = av_codec_iterate(&opaque)) != nullptr) {
        if (!av_codec_is_encoder(codec) || codec->type != AVMEDIA_TYPE_VIDEO || codec->id != codec_id) {
            continue;
        }
        if (supportsPixelFormat(codec, pixel_format)) {
            return codec;
        }
    }
    return nullptr;
}

}  // namespace

InternalRtspOutputService::InternalRtspOutputService(OverlayRtspConfig config)
    : config_(std::move(config)) {}

InternalRtspOutputService::~InternalRtspOutputService() {
    close();
}

std::string InternalRtspOutputService::playbackUrl() const {
    std::ostringstream url;
    url << "rtsp://" << config_.public_host << ":" << config_.port << normalizePath(config_.path);
    return url.str();
}

bool InternalRtspOutputService::open(int width, int height) {
    close();
    last_error_.clear();

    if (width <= 0 || height <= 0) {
        last_error_ = "invalid output frame size";
        return false;
    }

    avformat_network_init();
    const std::string url = playbackUrl();
    int result = avformat_alloc_output_context2(&format_context_, nullptr, "rtsp", url.c_str());
    if (result < 0 || format_context_ == nullptr) {
        setError("failed to allocate RTSP output context", result);
        return false;
    }

    if (!openEncoder(width, height)) {
        close();
        return false;
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtsp_transport", config_.rtsp_transport.c_str(), 0);
    result = avformat_write_header(format_context_, &options);
    av_dict_free(&options);
    if (result < 0) {
        setError("failed to write RTSP output header", result);
        close();
        return false;
    }

    return true;
}

bool InternalRtspOutputService::write(const cv::Mat& bgr_frame) {
    if (!isOpen()) {
        last_error_ = "RTSP output is not open";
        return false;
    }
    if (bgr_frame.empty()) {
        last_error_ = "empty overlay frame";
        return false;
    }
    if (!convertBgrToYuv(bgr_frame)) {
        return false;
    }
    yuv_frame_->pts = next_pts_++;
    return writeEncodedFrame(yuv_frame_);
}

void InternalRtspOutputService::close() {
    if (codec_context_ != nullptr && format_context_ != nullptr) {
        writeEncodedFrame(nullptr);
        av_write_trailer(format_context_);
    }
    if (sws_context_ != nullptr) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
    if (packet_ != nullptr) {
        av_packet_free(&packet_);
    }
    if (yuv_frame_ != nullptr) {
        av_frame_free(&yuv_frame_);
    }
    if (codec_context_ != nullptr) {
        avcodec_free_context(&codec_context_);
    }
    if (format_context_ != nullptr) {
        if ((format_context_->oformat->flags & AVFMT_NOFILE) == 0 && format_context_->pb != nullptr) {
            avio_closep(&format_context_->pb);
        }
        avformat_free_context(format_context_);
        format_context_ = nullptr;
    }
    stream_ = nullptr;
    next_pts_ = 0;
}

bool InternalRtspOutputService::isOpen() const {
    return format_context_ != nullptr && codec_context_ != nullptr && stream_ != nullptr;
}

const std::string& InternalRtspOutputService::lastError() const {
    return last_error_;
}

bool InternalRtspOutputService::openEncoder(int width, int height) {
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!supportsPixelFormat(codec, AV_PIX_FMT_YUV420P)) {
        codec = findEncoderWithPixelFormat(AV_CODEC_ID_H264, AV_PIX_FMT_YUV420P);
    }
    if (codec == nullptr) {
        codec = findEncoderWithPixelFormat(AV_CODEC_ID_MPEG4, AV_PIX_FMT_YUV420P);
    }
    if (codec == nullptr) {
        last_error_ = "failed to find a video encoder supporting yuv420p";
        return false;
    }

    stream_ = avformat_new_stream(format_context_, nullptr);
    if (stream_ == nullptr) {
        last_error_ = "failed to create output stream";
        return false;
    }

    codec_context_ = avcodec_alloc_context3(codec);
    if (codec_context_ == nullptr) {
        last_error_ = "failed to allocate encoder context";
        return false;
    }

    const int fps = config_.fps > 0 ? config_.fps : 25;
    codec_context_->codec_id = codec->id;
    codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context_->width = width;
    codec_context_->height = height;
    codec_context_->time_base = AVRational{1, fps};
    codec_context_->framerate = AVRational{fps, 1};
    codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context_->bit_rate = config_.bitrate;
    codec_context_->gop_size = fps;
    codec_context_->max_b_frames = 0;
    stream_->time_base = codec_context_->time_base;

    if ((format_context_->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        codec_context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(codec_context_->priv_data, "preset", "veryfast", 0);
        av_opt_set(codec_context_->priv_data, "tune", "zerolatency", 0);
    }

    int result = avcodec_open2(codec_context_, codec, nullptr);
    if (result < 0) {
        setError("failed to open H.264 encoder", result);
        return false;
    }

    result = avcodec_parameters_from_context(stream_->codecpar, codec_context_);
    if (result < 0) {
        setError("failed to copy encoder parameters", result);
        return false;
    }

    yuv_frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();
    if (yuv_frame_ == nullptr || packet_ == nullptr) {
        last_error_ = "failed to allocate encoder frame/packet";
        return false;
    }

    yuv_frame_->format = codec_context_->pix_fmt;
    yuv_frame_->width = codec_context_->width;
    yuv_frame_->height = codec_context_->height;
    result = av_frame_get_buffer(yuv_frame_, 32);
    if (result < 0) {
        setError("failed to allocate YUV frame buffer", result);
        return false;
    }

    return true;
}

bool InternalRtspOutputService::writeEncodedFrame(AVFrame* frame) {
    int result = avcodec_send_frame(codec_context_, frame);
    if (result < 0) {
        setError("failed to send frame to encoder", result);
        return false;
    }

    while (true) {
        result = avcodec_receive_packet(codec_context_, packet_);
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
            return true;
        }
        if (result < 0) {
            setError("failed to receive encoded packet", result);
            return false;
        }

        av_packet_rescale_ts(packet_, codec_context_->time_base, stream_->time_base);
        packet_->stream_index = stream_->index;
        result = av_interleaved_write_frame(format_context_, packet_);
        av_packet_unref(packet_);
        if (result < 0) {
            setError("failed to write encoded RTSP packet", result);
            return false;
        }
    }
}

bool InternalRtspOutputService::convertBgrToYuv(const cv::Mat& bgr_frame) {
    if (bgr_frame.type() != CV_8UC3) {
        last_error_ = "overlay frame must be BGR CV_8UC3";
        return false;
    }
    if (bgr_frame.cols != codec_context_->width || bgr_frame.rows != codec_context_->height) {
        last_error_ = "overlay frame size does not match encoder size";
        return false;
    }

    int result = av_frame_make_writable(yuv_frame_);
    if (result < 0) {
        setError("failed to make YUV frame writable", result);
        return false;
    }

    sws_context_ = sws_getCachedContext(sws_context_,
                                        bgr_frame.cols,
                                        bgr_frame.rows,
                                        AV_PIX_FMT_BGR24,
                                        codec_context_->width,
                                        codec_context_->height,
                                        codec_context_->pix_fmt,
                                        SWS_BILINEAR,
                                        nullptr,
                                        nullptr,
                                        nullptr);
    if (sws_context_ == nullptr) {
        last_error_ = "failed to create output swscale context";
        return false;
    }

    const uint8_t* src_data[4] = {bgr_frame.data, nullptr, nullptr, nullptr};
    int src_linesize[4] = {static_cast<int>(bgr_frame.step), 0, 0, 0};
    const int scaled = sws_scale(sws_context_,
                                 src_data,
                                 src_linesize,
                                 0,
                                 bgr_frame.rows,
                                 yuv_frame_->data,
                                 yuv_frame_->linesize);
    if (scaled != bgr_frame.rows) {
        last_error_ = "failed to convert overlay frame to YUV";
        return false;
    }
    return true;
}

void InternalRtspOutputService::setError(const std::string& prefix, int ffmpeg_error) {
    std::ostringstream message;
    message << prefix << ": " << ffmpegErrorText(ffmpeg_error);
    last_error_ = message.str();
}

}  // namespace honta::vision
