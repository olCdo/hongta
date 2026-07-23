#include "honta/vision/detection_service.h"

#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

#include <opencv2/imgproc.hpp>

#include "honta/logging/native_log.h"
#include "honta/vision/frame_crop.h"

namespace honta::vision {
namespace {

class WorkerLogGuard {
public:
    explicit WorkerLogGuard(std::string detect_type)
        : detect_type_(std::move(detect_type)) {}

    ~WorkerLogGuard() {
        honta::logging::info(
            "DetectionService",
            "worker stopped detect_type=" + detect_type_);
    }

private:
    std::string detect_type_;
};

}  // namespace

DetectionService::DetectionService() = default;

DetectionService::~DetectionService() {
    stopDetect();
}

void DetectionService::configure(DetectionServiceConfig config) {
    stopDetect();
    config_ = std::move(config);
}

bool DetectionService::startDetect(const std::string& detect_type) {
    const DetectionSessionSnapshot current = session_.snapshot();
    if (isRunningState(current.state)) {
        setError("detection session is already running");
        return false;
    }
    if (!isSupportedDetectType(detect_type)) {
        setError("unsupported detect_type: " + detect_type);
        return false;
    }

    const auto profile_iter = config_.profiles.find(detect_type);
    if (profile_iter == config_.profiles.end()) {
        setError("missing detection profile: " + detect_type);
        return false;
    }
    if (config_.input.url.empty()) {
        setError("empty input RTSP url");
        return false;
    }

    stopDetect();
    stop_requested_.store(false);
    session_.reset(detect_type,
                   config_.input.url,
                   overlayRtspUrl(),
                   config_.debug_output_enabled ? debugRtspUrl() : std::string{});
    honta::logging::info(
        "DetectionService",
        "start detect_type=" + detect_type);
    worker_ = std::thread(&DetectionService::workerLoop, this, detect_type, profile_iter->second);
    return true;
}

void DetectionService::stopDetect() {
    const bool worker_was_running = worker_.joinable();
    if (worker_was_running) {
        honta::logging::info("DetectionService", "stop requested");
    }
    stop_requested_.store(true);
    const DetectionSessionSnapshot current = session_.snapshot();
    if (isRunningState(current.state)) {
        session_.setState(DetectionSessionState::Stopping);
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    if (output_) {
        output_->close();
    }
    if (debug_output_) {
        debug_output_->close();
    }
    if (input_) {
        input_->close();
    }
    session_.clear();
    stop_requested_.store(false);
}

DetectionSessionSnapshot DetectionService::snapshot() const {
    return session_.snapshot();
}

std::optional<DetectionCandidate> DetectionService::findCandidate(
    int candidate_id,
    const std::string& expected_detect_type) const {
    return session_.findCandidate(candidate_id, expected_detect_type);
}

std::string DetectionService::overlayRtspUrl() const {
    InternalRtspOutputService output(config_.overlay);
    return output.playbackUrl();
}

std::string DetectionService::debugRtspUrl() const {
    InternalRtspOutputService output(config_.debug_overlay);
    return output.playbackUrl();
}

std::string DetectionService::lastError() const {
    return session_.snapshot().last_error;
}

void DetectionService::workerLoop(std::string detect_type, DetectionProfile profile) {
    WorkerLogGuard worker_log(detect_type);
    CircleDetector detector(profile.detector_config);
    int reconnect_attempts = 0;
    int frame_id = 0;

    while (!stop_requested_.load()) {
        session_.setState(reconnect_attempts == 0 ? DetectionSessionState::Starting
                                                  : DetectionSessionState::Reconnecting);
        if (!openInput()) {
            if (stop_requested_.load()) {
                break;
            }
            ++reconnect_attempts;
            if (reconnect_attempts > config_.max_reconnect_attempts) {
                session_.setState(DetectionSessionState::Error);
                return;
            }
            honta::logging::warning(
                "DetectionService",
                "reconnecting input attempt=" + std::to_string(reconnect_attempts));
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
            continue;
        }

        reconnect_attempts = 0;
        session_.setState(DetectionSessionState::Running);

        while (!stop_requested_.load()) {
            DecodedFrame decoded;
            if (!input_->read(decoded)) {
                if (stop_requested_.load()) {
                    break;
                }
                setError("input read failed: " + input_->lastError());
                session_.setState(DetectionSessionState::Reconnecting);
                input_->close();
                if (output_) {
                    output_->close();
                }
                if (debug_output_) {
                    debug_output_->close();
                }
                ++reconnect_attempts;
                break;
            }

            cv::Rect crop_rect;
            cv::Mat cropped_frame;
            try {
                cropped_frame = cropFrame(decoded.bgr, config_.crop, &crop_rect);
            } catch (const std::exception& error) {
                setError(std::string("invalid crop configuration: ") + error.what());
                session_.setState(DetectionSessionState::Error);
                return;
            }

            ++frame_id;
            std::vector<CircleDetection> local_detections = detector.detect(cropped_frame);
            std::vector<CircleDetection> full_frame_detections =
                offsetDetections(local_detections, crop_rect.tl());
            session_.updateCandidates(detect_type, frame_id, full_frame_detections);

            cv::Mat overlay = drawOverlay(cropped_frame,
                                          local_detections,
                                          frame_id,
                                          detect_type,
                                          session_.snapshot().state);
            if (!openOutputIfNeeded(overlay)) {
                session_.setState(DetectionSessionState::Error);
                return;
            }
            if (!output_->write(overlay)) {
                setError("publisher write failed: " + output_->lastError());
                session_.setState(DetectionSessionState::Error);
                return;
            }

            if (config_.debug_output_enabled) {
                PreprocessDebugImages debug_images = detector.buildPreprocessDebugImages(cropped_frame);
                cv::Mat debug_overlay = drawDebugOverlay(cropped_frame,
                                                         debug_images,
                                                         overlay,
                                                         frame_id,
                                                         detect_type,
                                                         session_.snapshot().state);
                if (!openDebugOutputIfNeeded(debug_overlay)) {
                    session_.setState(DetectionSessionState::Error);
                    return;
                }
                if (!debug_output_->write(debug_overlay)) {
                    setError("debug publisher write failed: " +
                             debug_output_->lastError());
                    session_.setState(DetectionSessionState::Error);
                    return;
                }
            }
        }

        if (reconnect_attempts > config_.max_reconnect_attempts) {
            setError("RTSP input exceeded max reconnect attempts");
            session_.setState(DetectionSessionState::Error);
            return;
        }
        if (!stop_requested_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_interval_ms));
        }
    }

    session_.setState(DetectionSessionState::Idle);
}

bool DetectionService::openInput() {
    honta::logging::info(
        "FfmpegRtspInput",
        "opening input=" +
            honta::logging::redactRtspCredentials(config_.input.url));
    input_ = std::make_unique<FfmpegRtspInput>(config_.input);
    if (!input_->open(&stop_requested_)) {
        if (!stop_requested_.load()) {
            setError(input_->lastError());
        }
        return false;
    }
    honta::logging::info("FfmpegRtspInput", "input opened");
    return true;
}

bool DetectionService::openOutputIfNeeded(const cv::Mat& frame) {
    if (output_ && output_->isOpen()) {
        return true;
    }
    output_ = std::make_unique<InternalRtspOutputService>(config_.overlay);
    honta::logging::info(
        "RtspPublisher",
        "opening publisher=" +
            honta::logging::redactRtspCredentials(output_->playbackUrl()));
    if (!output_->open(frame.cols, frame.rows)) {
        setError("publisher open failed: " + output_->lastError());
        return false;
    }
    honta::logging::info("RtspPublisher", "publisher online");
    return true;
}

bool DetectionService::openDebugOutputIfNeeded(const cv::Mat& frame) {
    if (debug_output_ && debug_output_->isOpen()) {
        return true;
    }
    debug_output_ = std::make_unique<InternalRtspOutputService>(config_.debug_overlay);
    honta::logging::info(
        "RtspDebugPublisher",
        "opening publisher=" +
            honta::logging::redactRtspCredentials(debug_output_->playbackUrl()));
    if (!debug_output_->open(frame.cols, frame.rows)) {
        setError("debug publisher open failed: " +
                 debug_output_->lastError());
        return false;
    }
    honta::logging::info("RtspDebugPublisher", "publisher online");
    return true;
}

cv::Mat DetectionService::drawOverlay(const cv::Mat& frame,
                                      const std::vector<CircleDetection>& detections,
                                      int frame_id,
                                      const std::string& detect_type,
                                      DetectionSessionState state) const {
    cv::Mat output;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, output, cv::COLOR_GRAY2BGR);
    } else {
        output = frame.clone();
    }

    const double scale = std::max(0.7, static_cast<double>(output.cols) / 1280.0);
    const int thickness = std::max(2, static_cast<int>(std::round(scale * 2.0)));
    const int radius_center = std::max(3, static_cast<int>(std::round(scale * 4.0)));
    const cv::Scalar color{0, 255, 80};
    const cv::Point crop_center(output.cols / 2, output.rows / 2);
    const int center_arm = std::max(12, std::min(output.cols, output.rows) / 32);
    cv::line(output,
             {crop_center.x - center_arm, crop_center.y},
             {crop_center.x + center_arm, crop_center.y},
             {0, 0, 0},
             thickness + 2,
             cv::LINE_AA);
    cv::line(output,
             {crop_center.x, crop_center.y - center_arm},
             {crop_center.x, crop_center.y + center_arm},
             {0, 0, 0},
             thickness + 2,
             cv::LINE_AA);
    cv::line(output,
             {crop_center.x - center_arm, crop_center.y},
             {crop_center.x + center_arm, crop_center.y},
             {0, 255, 255},
             thickness,
             cv::LINE_AA);
    cv::line(output,
             {crop_center.x, crop_center.y - center_arm},
             {crop_center.x, crop_center.y + center_arm},
             {0, 255, 255},
             thickness,
             cv::LINE_AA);

    for (std::size_t i = 0; i < detections.size(); ++i) {
        const CircleDetection& detection = detections[i];
        const int candidate_id = static_cast<int>(i + 1);
        const cv::Point center(static_cast<int>(std::round(detection.center.x)),
                               static_cast<int>(std::round(detection.center.y)));
        cv::circle(output, center, static_cast<int>(std::round(detection.radius)), color, thickness, cv::LINE_AA);
        cv::circle(output, center, radius_center, {0, 0, 255}, -1, cv::LINE_AA);

        std::ostringstream label;
        label << "#" << candidate_id << " " << std::fixed << std::setprecision(2) << detection.confidence;
        cv::Point origin(static_cast<int>(detection.center.x + detection.radius + 8 * scale),
                         static_cast<int>(detection.center.y));
        origin.x = std::min(std::max(origin.x, 8), output.cols - 120);
        origin.y = std::min(std::max(origin.y, 24), output.rows - 8);
        cv::putText(output, label.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.6 * scale, {0, 0, 0}, thickness + 2, cv::LINE_AA);
        cv::putText(output, label.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.6 * scale, color, thickness, cv::LINE_AA);
    }

    std::ostringstream status;
    status << "type=" << detect_type
           << " frame=" << frame_id
           << " candidates=" << detections.size()
           << " state=" << toString(state);
    const cv::Point status_origin(12, std::max(28, static_cast<int>(32 * scale)));
    cv::putText(output, status.str(), status_origin, cv::FONT_HERSHEY_SIMPLEX, 0.65 * scale, {0, 0, 0}, thickness + 2, cv::LINE_AA);
    cv::putText(output, status.str(), status_origin, cv::FONT_HERSHEY_SIMPLEX, 0.65 * scale, {255, 255, 255}, thickness, cv::LINE_AA);
    return output;
}

cv::Mat DetectionService::drawDebugOverlay(const cv::Mat& frame,
                                           const PreprocessDebugImages& debug_images,
                                           const cv::Mat& overlay,
                                           int frame_id,
                                           const std::string& detect_type,
                                           DetectionSessionState state) const {
    const int cell_width = std::max(1, frame.cols / 2);
    const int cell_height = std::max(1, frame.rows / 2);
    cv::Mat canvas(cell_height * 2, cell_width * 2, CV_8UC3, cv::Scalar(20, 20, 20));

    const auto toTile = [](const cv::Mat& source, const cv::Size& size) {
        cv::Mat color;
        if (source.empty()) {
            color = cv::Mat(size, CV_8UC3, cv::Scalar(0, 0, 0));
        } else if (source.channels() == 1) {
            cv::cvtColor(source, color, cv::COLOR_GRAY2BGR);
        } else {
            color = source.clone();
        }

        cv::Mat resized;
        cv::resize(color, resized, size, 0.0, 0.0, cv::INTER_AREA);
        return resized;
    };

    const auto placeTile = [&](const cv::Mat& source, int col, int row, const std::string& label) {
        const cv::Rect roi(col * cell_width, row * cell_height, cell_width, cell_height);
        cv::Mat tile = toTile(source, roi.size());
        tile.copyTo(canvas(roi));
        cv::putText(canvas, label, {roi.x + 10, roi.y + 26}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 0, 0}, 4, cv::LINE_AA);
        cv::putText(canvas, label, {roi.x + 10, roi.y + 26}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {255, 255, 255}, 2, cv::LINE_AA);
    };

    cv::Mat edges_color;
    if (!debug_images.edges.empty()) {
        cv::applyColorMap(debug_images.edges, edges_color, cv::COLORMAP_JET);
    }

    placeTile(frame, 0, 0, "source");
    placeTile(debug_images.preprocessed, 1, 0, "preprocessed");
    placeTile(edges_color.empty() ? debug_images.edges : edges_color, 0, 1, "edges");
    placeTile(overlay, 1, 1, "final overlay");

    std::ostringstream status;
    status << "debug type=" << detect_type
           << " frame=" << frame_id
           << " state=" << toString(state);
    cv::putText(canvas, status.str(), {12, canvas.rows - 14}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 0, 0}, 4, cv::LINE_AA);
    cv::putText(canvas, status.str(), {12, canvas.rows - 14}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 2, cv::LINE_AA);
    return canvas;
}

void DetectionService::setError(const std::string& error) {
    session_.setLastError(error);
    honta::logging::error("DetectionService", error);
    if (config_.on_error) {
        try {
            config_.on_error(error);
        } catch (const std::exception& callback_error) {
            honta::logging::error(
                "DetectionService",
                std::string("error callback failed: ") + callback_error.what());
        } catch (...) {
            honta::logging::error(
                "DetectionService",
                "error callback failed with unknown exception");
        }
    }
}

bool DetectionService::isRunningState(DetectionSessionState state) const {
    return state == DetectionSessionState::Starting ||
           state == DetectionSessionState::Running ||
           state == DetectionSessionState::Reconnecting ||
           state == DetectionSessionState::Stopping;
}

}  // namespace honta::vision
