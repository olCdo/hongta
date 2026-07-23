#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "honta/vision/circle_detector.h"
#include "honta/vision/detection_types.h"

namespace honta::vision {

enum class DetectionSessionState {
    Idle,
    Starting,
    Running,
    Reconnecting,
    Error,
    Stopping,
};

struct DetectionProfile {
    std::string detect_type;
    CircleDetectorConfig detector_config;
};

struct DetectionSessionSnapshot {
    std::string detect_type;
    std::string input_rtsp_url;
    std::string overlay_rtsp_url;
    std::string debug_rtsp_url;
    DetectionSessionState state = DetectionSessionState::Idle;
    int frame_id = 0;
    std::vector<DetectionCandidate> latest_candidates;
    std::string last_error;
};

class DetectionSession {
public:
    void reset(const std::string& detect_type,
               const std::string& input_rtsp_url,
               const std::string& overlay_rtsp_url,
               const std::string& debug_rtsp_url);
    void clear();
    void setState(DetectionSessionState state);
    void setLastError(const std::string& error);
    void updateCandidates(const std::string& detect_type,
                          int frame_id,
                          const std::vector<CircleDetection>& detections);
    std::optional<DetectionCandidate> findCandidate(int candidate_id,
                                                    const std::string& expected_detect_type) const;
    DetectionSessionSnapshot snapshot() const;

private:
    static std::int64_t nowMs();

    mutable std::mutex mutex_;
    DetectionSessionSnapshot snapshot_;
};

std::string toString(DetectionSessionState state);

}  // namespace honta::vision
