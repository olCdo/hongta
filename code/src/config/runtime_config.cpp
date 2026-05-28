#include "honta/config/runtime_config.h"

#include <cmath>
#include <stdexcept>

namespace honta::config {

void validateRuntimeConfig(const RuntimeConfig& config) {
    if (config.input_rtsp_url.empty()) {
        throw std::invalid_argument("input_rtsp_url is required");
    }
    if (config.overlay_public_host.empty()) {
        throw std::invalid_argument("overlay_public_host is required");
    }
    if (config.overlay_bind_ip.empty()) {
        throw std::invalid_argument("overlay_bind_ip is required");
    }
    if (config.overlay_port < 1 || config.overlay_port > 65535) {
        throw std::invalid_argument("overlay_port must be in 1..65535");
    }
    if (config.overlay_path.empty() || config.overlay_path.front() != '/') {
        throw std::invalid_argument("overlay_path must start with /");
    }
    if (config.camera_config_path.empty()) {
        throw std::invalid_argument("camera_config_path is required");
    }
    if (config.top_cover_data_dir.empty()) {
        throw std::invalid_argument("top_cover_data_dir is required");
    }
    if (config.detection_profiles.find("entrance_hole") == config.detection_profiles.end()) {
        throw std::invalid_argument("detection profile entrance_hole is required");
    }
    if (config.detection_profiles.find("center_horn") == config.detection_profiles.end()) {
        throw std::invalid_argument("detection profile center_horn is required");
    }
    for (const auto& item : config.detection_profiles) {
        const DetectionProfileConfig& profile = item.second;
        if (profile.min_radius_px <= 0 || profile.max_radius_px <= 0) {
            throw std::invalid_argument("detection radius must be greater than 0");
        }
        if (profile.min_radius_px > profile.max_radius_px) {
            throw std::invalid_argument("min_radius_px must not exceed max_radius_px");
        }
        if (!std::isfinite(profile.min_confidence) ||
            profile.min_confidence < 0.0 || profile.min_confidence > 1.0) {
            throw std::invalid_argument("min_confidence must be in 0..1");
        }
        if (!std::isfinite(profile.min_rim_edge_support) ||
            profile.min_rim_edge_support < 0.0 || profile.min_rim_edge_support > 1.0) {
            throw std::invalid_argument("min_rim_edge_support must be in 0..1");
        }
        if (!std::isfinite(profile.canny_high_threshold) ||
            profile.canny_high_threshold <= 0.0) {
            throw std::invalid_argument("canny_high_threshold must be greater than 0");
        }
        if (!std::isfinite(profile.hough_accumulator_threshold) ||
            profile.hough_accumulator_threshold <= 0.0) {
            throw std::invalid_argument("hough_accumulator_threshold must be greater than 0");
        }
        if (!std::isfinite(profile.max_radius_image_ratio) ||
            profile.max_radius_image_ratio <= 0.0 || profile.max_radius_image_ratio > 1.0) {
            throw std::invalid_argument("max_radius_image_ratio must be in 0..1");
        }
        if (profile.max_results <= 0) {
            throw std::invalid_argument("max_results must be greater than 0");
        }
    }
}

}  // namespace honta::config
