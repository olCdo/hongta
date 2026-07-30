#include "honta/calibration/calibration_session.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace honta::calibration {
namespace {

void validateFinite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

ImagePoint candidateCenter(const honta::vision::DetectionCandidate& candidate) {
    validateFinite(candidate.center_x, "candidate.center_x");
    validateFinite(candidate.center_y, "candidate.center_y");
    return {candidate.center_x, candidate.center_y};
}

}  // namespace

CalibrationSession::CalibrationSession(honta::model::TopCoverModel top_cover_model)
    : top_cover_model_(std::move(top_cover_model)) {
    honta::model::validateTopCoverModel(top_cover_model_);
    honta::model::buildTopCoverIndexes(top_cover_model_);
}

void CalibrationSession::reset() {
    entrance_.reset();
    center_.reset();
    cover_rotation_rad_.reset();
    horn_coordinates_by_no_.clear();
}

void CalibrationSession::confirmEntranceCandidate(
    const honta::vision::DetectionCandidate& candidate,
    int entrance_hole_no,
    double target_plane_vehicle_z_mm,
    const AmrPose& amr_pose,
    const CoordinateSolver& solver) {
    if (candidate.detect_type != honta::vision::kDetectTypeEntranceHole) {
        throw std::invalid_argument("entrance confirmation requires an entrance_hole candidate");
    }
    if (top_cover_model_.holes_by_no.find(entrance_hole_no) ==
        top_cover_model_.holes_by_no.end()) {
        throw std::invalid_argument("entrance_hole_no does not exist in top cover model");
    }

    ConfirmedEntrance confirmed;
    confirmed.candidate_id = candidate.candidate_id;
    confirmed.entrance_hole_no = entrance_hole_no;
    confirmed.candidate = candidate;
    confirmed.target_plane_vehicle_z_mm = target_plane_vehicle_z_mm;
    confirmed.amr_pose = amr_pose;
    confirmed.map_point = solver.pixelToMap(candidateCenter(candidate), target_plane_vehicle_z_mm, amr_pose);
    entrance_ = confirmed;
    cover_rotation_rad_.reset();
    horn_coordinates_by_no_.clear();
}

void CalibrationSession::confirmCenterCandidate(
    const honta::vision::DetectionCandidate& candidate,
    double target_plane_vehicle_z_mm,
    const AmrPose& amr_pose,
    const CoordinateSolver& solver) {
    if (candidate.detect_type != honta::vision::kDetectTypeCenterHorn) {
        throw std::invalid_argument("center confirmation requires a center_horn candidate");
    }

    ConfirmedCenter confirmed;
    confirmed.candidate_id = candidate.candidate_id;
    confirmed.candidate = candidate;
    confirmed.target_plane_vehicle_z_mm = target_plane_vehicle_z_mm;
    confirmed.amr_pose = amr_pose;
    confirmed.map_point = solver.pixelToMap(candidateCenter(candidate), target_plane_vehicle_z_mm, amr_pose);
    center_ = confirmed;
    cover_rotation_rad_.reset();
    horn_coordinates_by_no_.clear();
}

void CalibrationSession::calculateAllHorns() {
    if (!entrance_) {
        throw std::runtime_error("entrance candidate has not been confirmed");
    }
    if (!center_) {
        throw std::runtime_error("center candidate has not been confirmed");
    }

    const auto center_horn_it = top_cover_model_.horns_by_no.find(1);
    if (center_horn_it == top_cover_model_.horns_by_no.end()) {
        throw std::runtime_error("top cover model must contain center horn_no = 1");
    }
    const auto entrance_hole_it = top_cover_model_.holes_by_no.find(entrance_->entrance_hole_no);
    if (entrance_hole_it == top_cover_model_.holes_by_no.end()) {
        throw std::runtime_error("confirmed entrance_hole_no no longer exists");
    }

    const honta::model::TopCoverHorn& center_horn = center_horn_it->second;
    const honta::model::TopCoverHole& entrance_hole = entrance_hole_it->second;
    const double top_dx_m = (entrance_hole.x_mm - center_horn.x_mm) / 1000.0;
    const double top_dy_m = (entrance_hole.y_mm - center_horn.y_mm) / 1000.0;
    const double map_dx_m = entrance_->map_point.x_m - center_->map_point.x_m;
    const double map_dy_m = entrance_->map_point.y_m - center_->map_point.y_m;

    if (std::hypot(top_dx_m, top_dy_m) <= 1.0e-9) {
        throw std::runtime_error("entrance hole overlaps top cover center");
    }
    if (std::hypot(map_dx_m, map_dy_m) <= 1.0e-9) {
        throw std::runtime_error("confirmed entrance and center map points overlap");
    }

    const double top_angle = std::atan2(top_dy_m, top_dx_m);
    const double map_angle = std::atan2(map_dy_m, map_dx_m);
    cover_rotation_rad_ = normalizeRadians(map_angle - top_angle);

    horn_coordinates_by_no_.clear();
    const double cr = std::cos(*cover_rotation_rad_);
    const double sr = std::sin(*cover_rotation_rad_);
    for (const honta::model::TopCoverHorn& horn : top_cover_model_.horns) {
        const double local_x_m = (horn.x_mm - center_horn.x_mm) / 1000.0;
        const double local_y_m = (horn.y_mm - center_horn.y_mm) / 1000.0;
        HornMapCoordinate coordinate;
        coordinate.horn_no = horn.horn_no;
        coordinate.x_m = center_->map_point.x_m + cr * local_x_m - sr * local_y_m;
        coordinate.y_m = center_->map_point.y_m + sr * local_x_m + cr * local_y_m;
        horn_coordinates_by_no_.emplace(coordinate.horn_no, coordinate);
    }
}

std::optional<HornMapCoordinate> CalibrationSession::getHornCoordinate(int horn_no) const {
    const auto it = horn_coordinates_by_no_.find(horn_no);
    if (it == horn_coordinates_by_no_.end()) {
        return std::nullopt;
    }
    return it->second;
}

CalibrationSnapshot CalibrationSession::snapshot() const {
    CalibrationSnapshot snapshot;
    snapshot.model_id = top_cover_model_.model_id;
    snapshot.entrance = entrance_;
    snapshot.center = center_;
    snapshot.cover_rotation_rad = cover_rotation_rad_;
    snapshot.horn_coordinates.reserve(horn_coordinates_by_no_.size());
    for (const auto& item : horn_coordinates_by_no_) {
        snapshot.horn_coordinates.push_back(item.second);
    }
    return snapshot;
}

}  // namespace honta::calibration
