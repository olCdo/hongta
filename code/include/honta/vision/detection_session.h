#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "honta/vision/circle_detector.h"

namespace honta::vision {

constexpr const char* kDetectTypeEntranceHole = "entrance_hole";
constexpr const char* kDetectTypeCenterHorn = "center_horn";

enum class DetectionSessionState {
    Idle,
    Starting,
    Running,
    Reconnecting,
    Error,
    Stopping,
};

struct DetectionCandidate {
    int candidate_id = 0;
    std::string detect_type;
    double center_x = 0.0;
    double center_y = 0.0;
    double radius = 0.0;
    double confidence = 0.0;
    int frame_id = 0;
    std::int64_t timestamp_ms = 0;
};

struct DetectionProfile {
    std::string detect_type;
    CircleDetectorConfig detector_config;
};

struct DetectionSessionSnapshot {
    std::string detect_type;
    std::string input_rtsp_url;
    std::string overlay_rtsp_url;
    DetectionSessionState state = DetectionSessionState::Idle;
    int frame_id = 0;
    std::vector<DetectionCandidate> latest_candidates;
    std::string last_error;
};

class DetectionSession {
public:
    void reset(const std::string& detect_type,
               const std::string& input_rtsp_url,
               const std::string& overlay_rtsp_url);
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

bool isSupportedDetectType(const std::string& detect_type);
std::string toString(DetectionSessionState state);

}  // namespace honta::vision
