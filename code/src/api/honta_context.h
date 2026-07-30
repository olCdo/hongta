#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "honta/api/honta_api.h"
#include "honta/calibration/calibration_session.h"
#include "honta/calibration/coordinate_solver.h"
#include "honta/config/camera_config.h"
#include "honta/config/runtime_config.h"
#include "honta/model/top_cover_model.h"
#include "honta/vision/detection_types.h"

#ifdef HONTA_HAS_REAL_DETECTION
namespace honta::vision {
class DetectionService;
}  // namespace honta::vision
#endif

namespace honta::api {

class HontaContext {
public:
    explicit HontaContext(const char* config_path);
    ~HontaContext();

    HontaContext(const HontaContext&) = delete;
    HontaContext& operator=(const HontaContext&) = delete;

    int setTopCoverModel(const char* model_id);
    int startDetect(const char* detect_type);
    int stopDetect();
    int confirmEntranceCandidate(int candidate_id,
                                 int entrance_hole_no,
                                 double height_mm,
                                 double amr_x,
                                 double amr_y,
                                 double amr_yaw);
    int confirmCenterCandidate(int candidate_id,
                               double height_mm,
                               double amr_x,
                               double amr_y,
                               double amr_yaw);
    int calculateAllHorns();
    int getHornCoordinate(int horn_no, double* x, double* y);

    const std::string& overlayRtspUrl() const;
    std::string lastError() const;
    void setLastError(std::string error);

private:
    std::string resolveConfigPath(const std::string& path) const;
    bool ensureTopCoverLoaded();
    std::optional<honta::vision::DetectionCandidate> findCandidate(
        int candidate_id,
        const std::string& expected_detect_type) const;
    void addMockCandidateForDetectType(const std::string& detect_type);
    void clearCalibrationState();
#ifdef HONTA_HAS_REAL_DETECTION
    void configureDetectionService();
#endif

    honta::config::RuntimeConfig runtime_config_;
    honta::config::CameraConfig camera_config_;
    std::unique_ptr<honta::calibration::CoordinateSolver> solver_;
    std::optional<honta::model::TopCoverModel> top_cover_model_;
    std::unique_ptr<honta::calibration::CalibrationSession> calibration_session_;
    std::vector<honta::vision::DetectionCandidate> candidates_;
#ifdef HONTA_HAS_REAL_DETECTION
    std::unique_ptr<honta::vision::DetectionService> detection_service_;
#endif
    std::string config_dir_;
    std::string top_cover_data_dir_;
    std::string current_detect_type_;
    std::string overlay_rtsp_url_;
    mutable std::mutex context_error_mutex_;
    std::string context_last_error_;
};

}  // namespace honta::api
