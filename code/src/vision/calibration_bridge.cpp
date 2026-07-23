#include "honta/vision/calibration_bridge.h"

#include <stdexcept>

namespace honta::vision {
namespace {

honta::vision::DetectionCandidate requireCandidate(
    const std::optional<honta::vision::DetectionCandidate>& candidate,
    int candidate_id,
    const std::string& detect_type) {
    if (!candidate) {
        throw std::invalid_argument("candidate_id " + std::to_string(candidate_id) +
                                    " was not found for detect_type " + detect_type);
    }
    return *candidate;
}

}  // namespace

void confirmEntranceCandidateFromSession(
    const DetectionSession& detection_session,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    int entrance_hole_no,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver) {
    const DetectionCandidate candidate = requireCandidate(
        detection_session.findCandidate(candidate_id, kDetectTypeEntranceHole),
        candidate_id,
        kDetectTypeEntranceHole);
    calibration_session.confirmEntranceCandidate(
        candidate, entrance_hole_no, target_plane_vehicle_z_mm, amr_pose, solver);
}

void confirmCenterCandidateFromSession(
    const DetectionSession& detection_session,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver) {
    const DetectionCandidate candidate = requireCandidate(
        detection_session.findCandidate(candidate_id, kDetectTypeCenterHorn),
        candidate_id,
        kDetectTypeCenterHorn);
    calibration_session.confirmCenterCandidate(
        candidate, target_plane_vehicle_z_mm, amr_pose, solver);
}

void confirmEntranceCandidateFromService(
    const DetectionService& detection_service,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    int entrance_hole_no,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver) {
    const DetectionCandidate candidate = requireCandidate(
        detection_service.findCandidate(candidate_id, kDetectTypeEntranceHole),
        candidate_id,
        kDetectTypeEntranceHole);
    calibration_session.confirmEntranceCandidate(
        candidate, entrance_hole_no, target_plane_vehicle_z_mm, amr_pose, solver);
}

void confirmCenterCandidateFromService(
    const DetectionService& detection_service,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver) {
    const DetectionCandidate candidate = requireCandidate(
        detection_service.findCandidate(candidate_id, kDetectTypeCenterHorn),
        candidate_id,
        kDetectTypeCenterHorn);
    calibration_session.confirmCenterCandidate(
        candidate, target_plane_vehicle_z_mm, amr_pose, solver);
}

}  // namespace honta::vision
