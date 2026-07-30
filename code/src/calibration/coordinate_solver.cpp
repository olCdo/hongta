#include "honta/calibration/coordinate_solver.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace honta::calibration {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1.0e-9;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3 mapCameraToVehicleBase(const Vec3& point) {
    return {
        point.x,
        -point.y,
        point.z,
    };
}

Vec3 rotateCameraToVehicle(const Vec3& point, const honta::config::CameraConfig& camera) {
    const Vec3 base = mapCameraToVehicleBase(point);
    const double roll = degToRad(camera.mount_roll_deg);
    const double pitch = degToRad(camera.mount_pitch_deg);
    const double yaw = degToRad(camera.mount_yaw_deg);

    const double cr = std::cos(roll);
    const double sr = std::sin(roll);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);

    const Vec3 after_roll{
        base.x,
        cr * base.y - sr * base.z,
        sr * base.y + cr * base.z,
    };
    const Vec3 after_pitch{
        cp * after_roll.x + sp * after_roll.z,
        after_roll.y,
        -sp * after_roll.x + cp * after_roll.z,
    };
    return {
        cy * after_pitch.x - sy * after_pitch.y,
        sy * after_pitch.x + cy * after_pitch.y,
        after_pitch.z,
    };
}

}  // namespace

CoordinateSolver::CoordinateSolver(honta::config::CameraConfig camera_config)
    : camera_config_(std::move(camera_config)) {
    honta::config::validateCameraConfig(camera_config_);
}

CameraRay CoordinateSolver::pixelToUndistortedRay(const ImagePoint& image_point) const {
    const double distorted_x = (image_point.x_px - camera_config_.cx) / camera_config_.fx;
    const double distorted_y = (image_point.y_px - camera_config_.cy) / camera_config_.fy;

    double x = distorted_x;
    double y = distorted_y;
    for (int i = 0; i < 8; ++i) {
        const double r2 = x * x + y * y;
        const double r4 = r2 * r2;
        const double r6 = r4 * r2;
        const double radial = 1.0 + camera_config_.k1 * r2 + camera_config_.k2 * r4 +
                              camera_config_.k3 * r6;
        if (std::abs(radial) < kEpsilon) {
            throw std::runtime_error("invalid distortion parameters: radial factor is zero");
        }

        const double delta_x = 2.0 * camera_config_.p1 * x * y +
                               camera_config_.p2 * (r2 + 2.0 * x * x);
        const double delta_y = camera_config_.p1 * (r2 + 2.0 * y * y) +
                               2.0 * camera_config_.p2 * x * y;
        x = (distorted_x - delta_x) / radial;
        y = (distorted_y - delta_y) / radial;
    }

    return {x, y, 1.0};
}

VehiclePoint CoordinateSolver::projectPixelToVehiclePlane(
    const ImagePoint& image_point,
    double target_plane_vehicle_z_mm) const {
    if (!std::isfinite(target_plane_vehicle_z_mm)) {
        throw std::invalid_argument("target_plane_vehicle_z_mm must be finite");
    }

    const CameraRay ray = pixelToUndistortedRay(image_point);
    const Vec3 ray_vehicle = rotateCameraToVehicle({ray.x, ray.y, ray.z}, camera_config_);
    if (std::abs(ray_vehicle.z) < kEpsilon) {
        throw std::runtime_error("camera ray is parallel to target vehicle z plane");
    }

    const Vec3 camera_origin{
        camera_config_.mount_x_mm,
        camera_config_.mount_y_mm,
        camera_config_.mount_z_mm,
    };
    const double t = (target_plane_vehicle_z_mm - camera_origin.z) / ray_vehicle.z;
    if (t < 0.0) {
        throw std::runtime_error("target vehicle z plane is behind the camera ray");
    }

    return {
        camera_origin.x + t * ray_vehicle.x,
        camera_origin.y + t * ray_vehicle.y,
        target_plane_vehicle_z_mm,
    };
}

MapPoint CoordinateSolver::vehicleToMap(const VehiclePoint& vehicle_point,
                                        const AmrPose& amr_pose) const {
    const double vehicle_x_m = vehicle_point.x_mm / 1000.0;
    const double vehicle_y_m = vehicle_point.y_mm / 1000.0;
    const double cy = std::cos(amr_pose.yaw_rad);
    const double sy = std::sin(amr_pose.yaw_rad);
    return {
        amr_pose.x_m + cy * vehicle_x_m - sy * vehicle_y_m,
        amr_pose.y_m + sy * vehicle_x_m + cy * vehicle_y_m,
    };
}

MapPoint CoordinateSolver::pixelToMap(const ImagePoint& image_point,
                                      double target_plane_vehicle_z_mm,
                                      const AmrPose& amr_pose) const {
    return vehicleToMap(projectPixelToVehiclePlane(image_point, target_plane_vehicle_z_mm), amr_pose);
}

double degToRad(double degrees) {
    return degrees * kPi / 180.0;
}

double normalizeRadians(double radians) {
    while (radians <= -kPi) {
        radians += 2.0 * kPi;
    }
    while (radians > kPi) {
        radians -= 2.0 * kPi;
    }
    return radians;
}

}  // namespace honta::calibration
