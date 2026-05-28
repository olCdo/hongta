#include "honta/config/camera_config.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace honta::config {
namespace {

void requireFinite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

}  // namespace

void validateCameraConfig(const CameraConfig& config) {
    if (config.camera_id.empty()) {
        throw std::invalid_argument("camera_id is required");
    }
    if (config.image_width <= 0 || config.image_height <= 0) {
        throw std::invalid_argument("image_width and image_height must be greater than 0");
    }
    requireFinite(config.fx, "fx");
    requireFinite(config.fy, "fy");
    requireFinite(config.cx, "cx");
    requireFinite(config.cy, "cy");
    requireFinite(config.k1, "k1");
    requireFinite(config.k2, "k2");
    requireFinite(config.p1, "p1");
    requireFinite(config.p2, "p2");
    requireFinite(config.k3, "k3");
    requireFinite(config.mount_x_mm, "mount_x_mm");
    requireFinite(config.mount_y_mm, "mount_y_mm");
    requireFinite(config.mount_z_mm, "mount_z_mm");
    requireFinite(config.mount_yaw_deg, "mount_yaw_deg");
    requireFinite(config.mount_pitch_deg, "mount_pitch_deg");
    requireFinite(config.mount_roll_deg, "mount_roll_deg");
    if (config.fx <= 0.0 || config.fy <= 0.0) {
        throw std::invalid_argument("fx and fy must be greater than 0");
    }
    if (config.cx < 0.0 || config.cx >= static_cast<double>(config.image_width)) {
        throw std::invalid_argument("cx must be in [0, image_width)");
    }
    if (config.cy < 0.0 || config.cy >= static_cast<double>(config.image_height)) {
        throw std::invalid_argument("cy must be in [0, image_height)");
    }
}

}  // namespace honta::config
