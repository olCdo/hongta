#include "honta_context.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "honta/config/config_loader.h"

#ifdef HONTA_HAS_REAL_DETECTION
#include "honta/vision/detection_service.h"
#endif

namespace honta::api {
namespace {

std::string buildOverlayUrl(const honta::config::RuntimeConfig& config) {
    std::ostringstream output;
    output << "rtsp://" << config.overlay_public_host << ":" << config.overlay_port;
    if (!config.overlay_path.empty() && config.overlay_path.front() != '/') {
        output << "/";
    }
    output << config.overlay_path;
    return output.str();
}

honta::vision::DetectionCandidate makeMockCandidate(const std::string& detect_type,
                                                    double center_x,
                                                    double center_y) {
    honta::vision::DetectionCandidate candidate;
    candidate.candidate_id = 1;
    candidate.detect_type = detect_type;
    candidate.center_x = center_x;
    candidate.center_y = center_y;
    candidate.radius = 80.0;
    candidate.confidence = 0.95;
    candidate.frame_id = 1;
    candidate.timestamp_ms = 1;
    return candidate;
}

#ifdef HONTA_HAS_REAL_DETECTION
honta::vision::CircleDetectorConfig toDetectorConfig(
    const honta::config::DetectionProfileConfig& input) {
    honta::vision::CircleDetectorConfig output;
    output.min_radius_px = input.min_radius_px;
    output.max_radius_px = input.max_radius_px;
    output.min_confidence = input.min_confidence;
    output.min_rim_edge_support = input.min_rim_edge_support;
    output.canny_high_threshold = input.canny_high_threshold;
    output.hough_accumulator_threshold = input.hough_accumulator_threshold;
    output.enable_clahe = input.enable_clahe;
    output.enable_hist_equalization = input.enable_hist_equalization;
    output.max_radius_image_ratio = input.max_radius_image_ratio;
    output.max_results = input.max_results;
    return output;
}
#endif

}  // namespace

HontaContext::HontaContext(const char* config_path) {
    if (config_path == nullptr || config_path[0] == '\0') {
        throw std::invalid_argument("config_path is required");
    }

    const std::filesystem::path config_file(config_path);
    config_dir_ = std::filesystem::absolute(config_file).parent_path().string();
    runtime_config_ = honta::config::loadRuntimeConfig(config_path);
    const std::string camera_path = resolveConfigPath(runtime_config_.camera_config_path);
    top_cover_data_dir_ = resolveConfigPath(runtime_config_.top_cover_data_dir);
    camera_config_ = honta::config::loadCameraConfig(camera_path);
    solver_ = std::make_unique<honta::calibration::CoordinateSolver>(camera_config_);
    overlay_rtsp_url_ = buildOverlayUrl(runtime_config_);
#ifdef HONTA_HAS_REAL_DETECTION
    configureDetectionService();
#endif
}

HontaContext::~HontaContext() {
    stopDetect();
}

int HontaContext::setTopCoverModel(const char* model_id) {
    if (model_id == nullptr || model_id[0] == '\0') {
        setLastError("model_id is required");
        return HONTA_ERROR_INVALID_ARGUMENT;
    }

    try {
        top_cover_model_ = honta::config::loadTopCoverModel(top_cover_data_dir_, model_id);
        clearCalibrationState();
        return HONTA_OK;
    } catch (const std::exception& error) {
        setLastError(std::string("failed to load top cover model: ") + error.what());
        return HONTA_ERROR_CONFIG;
    }
}

int HontaContext::startDetect(const char* detect_type) {
    if (detect_type == nullptr || detect_type[0] == '\0') {
        setLastError("detect_type is required");
        return HONTA_ERROR_INVALID_ARGUMENT;
    }
    const std::string type(detect_type);
    if (!honta::vision::isSupportedDetectType(type)) {
        setLastError("unsupported detect_type: " + type);
        return HONTA_ERROR_INVALID_ARGUMENT;
    }

    stopDetect();
    current_detect_type_ = type;
#ifdef HONTA_ENABLE_MOCK_DETECTION
    addMockCandidateForDetectType(type);
    return HONTA_OK;
#elif defined(HONTA_HAS_REAL_DETECTION)
    if (!detection_service_) {
        setLastError("real RTSP detection service is not configured");
        return HONTA_ERROR_DETECTION;
    }
    setLastError({});
    if (!detection_service_->startDetect(type)) {
        setLastError(detection_service_->lastError());
        return HONTA_ERROR_DETECTION;
    }
    return HONTA_OK;
#else
    setLastError("real RTSP detection is not enabled in this honta_native build");
    return HONTA_ERROR_DETECTION;
#endif
}

int HontaContext::stopDetect() {
#ifdef HONTA_HAS_REAL_DETECTION
    if (detection_service_) {
        detection_service_->stopDetect();
    }
#endif
    current_detect_type_.clear();
    candidates_.clear();
    return HONTA_OK;
}

int HontaContext::confirmEntranceCandidate(int candidate_id,
                                           int entrance_hole_no,
                                           double height_mm,
                                           double amr_x,
                                           double amr_y,
                                           double amr_yaw) {
    if (!ensureTopCoverLoaded()) {
        return HONTA_ERROR_BAD_STATE;
    }
    if (current_detect_type_ != honta::vision::kDetectTypeEntranceHole) {
        setLastError("entrance candidate can only be confirmed during entrance_hole detection");
        return HONTA_ERROR_BAD_STATE;
    }
    const auto candidate = findCandidate(candidate_id, honta::vision::kDetectTypeEntranceHole);
    if (!candidate) {
        setLastError("entrance candidate not found: " + std::to_string(candidate_id));
        return HONTA_ERROR_DETECTION;
    }
    if (top_cover_model_->holes_by_no.find(entrance_hole_no) == top_cover_model_->holes_by_no.end()) {
        setLastError("entrance_hole_no not found: " + std::to_string(entrance_hole_no));
        return HONTA_ERROR_INVALID_ARGUMENT;
    }

    try {
        calibration_session_->confirmEntranceCandidate(
            *candidate,
            entrance_hole_no,
            height_mm,
            {amr_x, amr_y, amr_yaw},
            *solver_);
        return HONTA_OK;
    } catch (const std::exception& error) {
        setLastError(std::string("failed to confirm entrance candidate: ") + error.what());
        return HONTA_ERROR_CALIBRATION;
    }
}

int HontaContext::confirmCenterCandidate(int candidate_id,
                                         double height_mm,
                                         double amr_x,
                                         double amr_y,
                                         double amr_yaw) {
    if (!ensureTopCoverLoaded()) {
        return HONTA_ERROR_BAD_STATE;
    }
    if (current_detect_type_ != honta::vision::kDetectTypeCenterHorn) {
        setLastError("center candidate can only be confirmed during center_horn detection");
        return HONTA_ERROR_BAD_STATE;
    }
    const auto candidate = findCandidate(candidate_id, honta::vision::kDetectTypeCenterHorn);
    if (!candidate) {
        setLastError("center candidate not found: " + std::to_string(candidate_id));
        return HONTA_ERROR_DETECTION;
    }

    try {
        calibration_session_->confirmCenterCandidate(
            *candidate,
            height_mm,
            {amr_x, amr_y, amr_yaw},
            *solver_);
        return HONTA_OK;
    } catch (const std::exception& error) {
        setLastError(std::string("failed to confirm center candidate: ") + error.what());
        return HONTA_ERROR_CALIBRATION;
    }
}

int HontaContext::calculateAllHorns() {
    if (!ensureTopCoverLoaded()) {
        return HONTA_ERROR_BAD_STATE;
    }
    try {
        calibration_session_->calculateAllHorns();
        return HONTA_OK;
    } catch (const std::exception& error) {
        setLastError(std::string("failed to calculate horn coordinates: ") + error.what());
        return HONTA_ERROR_BAD_STATE;
    }
}

int HontaContext::getHornCoordinate(int horn_no, double* x, double* y) {
    if (x == nullptr || y == nullptr) {
        setLastError("x and y output pointers are required");
        return HONTA_ERROR_INVALID_ARGUMENT;
    }
    if (!ensureTopCoverLoaded()) {
        return HONTA_ERROR_BAD_STATE;
    }

    const auto coordinate = calibration_session_->getHornCoordinate(horn_no);
    if (!coordinate) {
        setLastError("horn coordinate not found: " + std::to_string(horn_no));
        return HONTA_ERROR_BAD_STATE;
    }
    *x = coordinate->x_m;
    *y = coordinate->y_m;
    return HONTA_OK;
}

const std::string& HontaContext::overlayRtspUrl() const {
    return overlay_rtsp_url_;
}

std::string HontaContext::lastError() const {
    std::lock_guard<std::mutex> lock(context_error_mutex_);
    return context_last_error_;
}

void HontaContext::setLastError(std::string error) {
    std::lock_guard<std::mutex> lock(context_error_mutex_);
    context_last_error_ = std::move(error);
}

std::string HontaContext::resolveConfigPath(const std::string& path) const {
    const std::filesystem::path input(path);
    if (input.is_absolute()) {
        return input.string();
    }
    return (std::filesystem::path(config_dir_) / input).lexically_normal().string();
}

bool HontaContext::ensureTopCoverLoaded() {
    if (!top_cover_model_ || !calibration_session_) {
        setLastError("top cover model has not been set");
        return false;
    }
    return true;
}

std::optional<honta::vision::DetectionCandidate> HontaContext::findCandidate(
    int candidate_id,
    const std::string& expected_detect_type) const {
#if defined(HONTA_HAS_REAL_DETECTION) && !defined(HONTA_ENABLE_MOCK_DETECTION)
    if (detection_service_) {
        return detection_service_->findCandidate(candidate_id, expected_detect_type);
    }
#endif
    for (const auto& candidate : candidates_) {
        if (candidate.candidate_id == candidate_id && candidate.detect_type == expected_detect_type) {
            return candidate;
        }
    }
    return std::nullopt;
}

void HontaContext::addMockCandidateForDetectType(const std::string& detect_type) {
    candidates_.clear();
#ifdef HONTA_ENABLE_MOCK_DETECTION
    if (detect_type == honta::vision::kDetectTypeEntranceHole) {
        candidates_.push_back(makeMockCandidate(detect_type, 1080.0, 540.0));
    } else if (detect_type == honta::vision::kDetectTypeCenterHorn) {
        candidates_.push_back(makeMockCandidate(detect_type, 960.0, 540.0));
    }
#endif
}

void HontaContext::clearCalibrationState() {
    if (top_cover_model_) {
        calibration_session_ =
            std::make_unique<honta::calibration::CalibrationSession>(*top_cover_model_);
    } else {
        calibration_session_.reset();
    }
}

#ifdef HONTA_HAS_REAL_DETECTION
void HontaContext::configureDetectionService() {
    honta::vision::DetectionServiceConfig config;
    config.input.url = runtime_config_.input_rtsp_url;
    config.overlay.bind_ip = runtime_config_.overlay_bind_ip;
    config.overlay.public_host = runtime_config_.overlay_public_host;
    config.overlay.port = runtime_config_.overlay_port;
    config.overlay.path = runtime_config_.overlay_path;
    config.processing_scale = runtime_config_.processing_scale;
    config.crop = runtime_config_.crop;
    config.on_error = [this](const std::string& error) {
        setLastError(error);
    };

    for (const auto& item : runtime_config_.detection_profiles) {
        honta::vision::DetectionProfile profile;
        profile.detect_type = item.first;
        profile.detector_config = toDetectorConfig(item.second);
        config.profiles.emplace(item.first, profile);
    }

    detection_service_ = std::make_unique<honta::vision::DetectionService>();
    detection_service_->configure(std::move(config));
    overlay_rtsp_url_ = detection_service_->overlayRtspUrl();
}
#endif

}  // namespace honta::api
