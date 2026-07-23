#include "honta/vision/detection_session.h"

#include <utility>

namespace honta::vision {

void DetectionSession::reset(const std::string& detect_type,
                             const std::string& input_rtsp_url,
                             const std::string& overlay_rtsp_url,
                             const std::string& debug_rtsp_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = {};
    snapshot_.detect_type = detect_type;
    snapshot_.input_rtsp_url = input_rtsp_url;
    snapshot_.overlay_rtsp_url = overlay_rtsp_url;
    snapshot_.debug_rtsp_url = debug_rtsp_url;
    snapshot_.state = DetectionSessionState::Starting;
}

void DetectionSession::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = {};
}

void DetectionSession::setState(DetectionSessionState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = state;
}

void DetectionSession::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.last_error = error;
}

void DetectionSession::updateCandidates(const std::string& detect_type,
                                        int frame_id,
                                        const std::vector<CircleDetection>& detections) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.frame_id = frame_id;
    snapshot_.latest_candidates.clear();
    snapshot_.latest_candidates.reserve(detections.size());

    const std::int64_t timestamp_ms = nowMs();
    for (std::size_t i = 0; i < detections.size(); ++i) {
        const CircleDetection& detection = detections[i];
        DetectionCandidate candidate;
        candidate.candidate_id = static_cast<int>(i + 1);
        candidate.detect_type = detect_type;
        candidate.center_x = detection.center.x;
        candidate.center_y = detection.center.y;
        candidate.radius = detection.radius;
        candidate.confidence = detection.confidence;
        candidate.frame_id = frame_id;
        candidate.timestamp_ms = timestamp_ms;
        snapshot_.latest_candidates.push_back(std::move(candidate));
    }
}

std::optional<DetectionCandidate> DetectionSession::findCandidate(
    int candidate_id,
    const std::string& expected_detect_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const DetectionCandidate& candidate : snapshot_.latest_candidates) {
        if (candidate.candidate_id == candidate_id && candidate.detect_type == expected_detect_type) {
            return candidate;
        }
    }
    return std::nullopt;
}

DetectionSessionSnapshot DetectionSession::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

std::int64_t DetectionSession::nowMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::string toString(DetectionSessionState state) {
    switch (state) {
        case DetectionSessionState::Idle:
            return "Idle";
        case DetectionSessionState::Starting:
            return "Starting";
        case DetectionSessionState::Running:
            return "Running";
        case DetectionSessionState::Reconnecting:
            return "Reconnecting";
        case DetectionSessionState::Error:
            return "Error";
        case DetectionSessionState::Stopping:
            return "Stopping";
    }
    return "Unknown";
}

}  // namespace honta::vision
