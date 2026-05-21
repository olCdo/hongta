#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "honta/vision/detection_session.h"
#include "honta/vision/ffmpeg_rtsp_input.h"
#include "honta/vision/rtsp_output_service.h"

namespace honta::vision {

struct DetectionServiceConfig {
    FfmpegRtspInputConfig input;
    OverlayRtspConfig overlay;
    OverlayRtspConfig debug_overlay = [] {
        OverlayRtspConfig config;
        config.path = "/honta_debug";
        return config;
    }();
    std::map<std::string, DetectionProfile> profiles;
    int reconnect_interval_ms = 1000;
    int max_reconnect_attempts = 60;
    bool debug_output_enabled = false;
};

class DetectionService {
public:
    DetectionService();
    ~DetectionService();

    DetectionService(const DetectionService&) = delete;
    DetectionService& operator=(const DetectionService&) = delete;

    void configure(DetectionServiceConfig config);
    bool startDetect(const std::string& detect_type);
    void stopDetect();

    DetectionSessionSnapshot snapshot() const;
    std::optional<DetectionCandidate> findCandidate(int candidate_id,
                                                    const std::string& expected_detect_type) const;
    std::string overlayRtspUrl() const;
    std::string debugRtspUrl() const;
    const std::string& lastError() const;

private:
    void workerLoop(std::string detect_type, DetectionProfile profile);
    bool openInput();
    bool openOutputIfNeeded(const cv::Mat& frame);
    bool openDebugOutputIfNeeded(const cv::Mat& frame);
    cv::Mat drawOverlay(const cv::Mat& frame,
                        const std::vector<CircleDetection>& detections,
                        int frame_id,
                        const std::string& detect_type,
                        DetectionSessionState state) const;
    cv::Mat drawDebugOverlay(const cv::Mat& frame,
                             const PreprocessDebugImages& debug_images,
                             const cv::Mat& overlay,
                             int frame_id,
                             const std::string& detect_type,
                             DetectionSessionState state) const;
    void setError(const std::string& error);
    bool isRunningState(DetectionSessionState state) const;

    DetectionServiceConfig config_;
    DetectionSession session_;
    std::unique_ptr<FfmpegRtspInput> input_;
    std::unique_ptr<InternalRtspOutputService> output_;
    std::unique_ptr<InternalRtspOutputService> debug_output_;
    std::thread worker_;
    std::atomic_bool stop_requested_{false};
    std::string last_error_;
};

}  // namespace honta::vision
