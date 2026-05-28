#pragma once

#include <string>

namespace honta::config {

struct CameraConfig {
    std::string camera_id;
    int image_width = 0;
    int image_height = 0;
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double k1 = 0.0;
    double k2 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
    double k3 = 0.0;
    double mount_x_mm = 0.0;
    double mount_y_mm = 0.0;
    double mount_z_mm = 0.0;
    double mount_yaw_deg = 0.0;
    double mount_pitch_deg = 0.0;
    double mount_roll_deg = 0.0;
};

void validateCameraConfig(const CameraConfig& config);

}  // namespace honta::config
