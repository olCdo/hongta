#pragma once

#include "honta/config/camera_config.h"

namespace honta::calibration {

struct ImagePoint {
    double x_px = 0.0;
    double y_px = 0.0;
};

struct VehiclePoint {
    double x_mm = 0.0;
    double y_mm = 0.0;
    double z_mm = 0.0;
};

struct MapPoint {
    double x_m = 0.0;
    double y_m = 0.0;
};

struct AmrPose {
    double x_m = 0.0;
    double y_m = 0.0;
    double yaw_rad = 0.0;
};

struct CameraRay {
    double x = 0.0;
    double y = 0.0;
    double z = 1.0;
};

// Mount convention:
// - Camera coordinates use x right, y down, z along the optical axis.
// - Vehicle coordinates use x forward, y left, z up.
// - The base upward-facing mount maps camera +z to vehicle +z, camera +x to
//   vehicle +x, and camera +y to vehicle -y.
// - Extra mount rotation is applied after that base mapping as
//   Rz(yaw) * Ry(pitch) * Rx(roll).
// - target_plane_vehicle_z_mm is the target plane z value in vehicle
//   coordinates. It is already relative to the vehicle origin and does not add
//   camera mount height again.
class CoordinateSolver {
public:
    explicit CoordinateSolver(honta::config::CameraConfig camera_config);

    CameraRay pixelToUndistortedRay(const ImagePoint& image_point) const;
    VehiclePoint projectPixelToVehiclePlane(const ImagePoint& image_point,
                                            double target_plane_vehicle_z_mm) const;
    MapPoint vehicleToMap(const VehiclePoint& vehicle_point, const AmrPose& amr_pose) const;
    MapPoint pixelToMap(const ImagePoint& image_point,
                        double target_plane_vehicle_z_mm,
                        const AmrPose& amr_pose) const;

private:
    honta::config::CameraConfig camera_config_;
};

double degToRad(double degrees);
double normalizeRadians(double radians);

}  // namespace honta::calibration
