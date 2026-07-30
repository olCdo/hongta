#pragma once

#include "honta/calibration/calibration_session.h"
#include "honta/vision/detection_service.h"
#include "honta/vision/detection_session.h"

namespace honta::vision {

void confirmEntranceCandidateFromSession(
    const DetectionSession& detection_session,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    int entrance_hole_no,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver);

void confirmCenterCandidateFromSession(
    const DetectionSession& detection_session,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver);

void confirmEntranceCandidateFromService(
    const DetectionService& detection_service,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    int entrance_hole_no,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver);

void confirmCenterCandidateFromService(
    const DetectionService& detection_service,
    honta::calibration::CalibrationSession& calibration_session,
    int candidate_id,
    double target_plane_vehicle_z_mm,
    const honta::calibration::AmrPose& amr_pose,
    const honta::calibration::CoordinateSolver& solver);

}  // namespace honta::vision
