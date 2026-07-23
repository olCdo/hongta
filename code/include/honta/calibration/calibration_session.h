#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "honta/calibration/coordinate_solver.h"
#include "honta/model/top_cover_model.h"
#include "honta/vision/detection_types.h"

namespace honta::calibration {

struct ConfirmedEntrance {
    int candidate_id = 0;
    int entrance_hole_no = 0;
    honta::vision::DetectionCandidate candidate;
    double target_plane_vehicle_z_mm = 0.0;
    AmrPose amr_pose;
    MapPoint map_point;
};

struct ConfirmedCenter {
    int candidate_id = 0;
    honta::vision::DetectionCandidate candidate;
    double target_plane_vehicle_z_mm = 0.0;
    AmrPose amr_pose;
    MapPoint map_point;
};

struct HornMapCoordinate {
    int horn_no = 0;
    double x_m = 0.0;
    double y_m = 0.0;
};

struct CalibrationSnapshot {
    std::string model_id;
    std::optional<ConfirmedEntrance> entrance;
    std::optional<ConfirmedCenter> center;
    std::optional<double> cover_rotation_rad;
    std::vector<HornMapCoordinate> horn_coordinates;
};

class CalibrationSession {
public:
    explicit CalibrationSession(honta::model::TopCoverModel top_cover_model);

    void reset();
    void confirmEntranceCandidate(const honta::vision::DetectionCandidate& candidate,
                                  int entrance_hole_no,
                                  double target_plane_vehicle_z_mm,
                                  const AmrPose& amr_pose,
                                  const CoordinateSolver& solver);
    void confirmCenterCandidate(const honta::vision::DetectionCandidate& candidate,
                                double target_plane_vehicle_z_mm,
                                const AmrPose& amr_pose,
                                const CoordinateSolver& solver);
    void calculateAllHorns();
    std::optional<HornMapCoordinate> getHornCoordinate(int horn_no) const;
    CalibrationSnapshot snapshot() const;

private:
    honta::model::TopCoverModel top_cover_model_;
    std::optional<ConfirmedEntrance> entrance_;
    std::optional<ConfirmedCenter> center_;
    std::optional<double> cover_rotation_rad_;
    std::map<int, HornMapCoordinate> horn_coordinates_by_no_;
};

}  // namespace honta::calibration
