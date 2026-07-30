#pragma once

#include <map>
#include <string>

namespace honta::config {

struct DetectionProfileConfig {
    int min_radius_px = 12;
    int max_radius_px = 320;
    double min_confidence = 0.55;
    double min_rim_edge_support = 0.24;
    double canny_high_threshold = 120.0;
    double hough_accumulator_threshold = 28.0;
    bool enable_clahe = false;
    bool enable_hist_equalization = false;
    double max_radius_image_ratio = 0.22;
    int max_results = 20;
};

struct CropConfig {
    bool enabled = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct RuntimeConfig {
    std::string input_rtsp_url;
    std::string overlay_bind_ip = "0.0.0.0";
    std::string overlay_public_host;
    int overlay_port = 8554;
    std::string overlay_path = "/honta_overlay";
    std::string camera_config_path;
    std::string top_cover_data_dir;
    double processing_scale = 1.0;
    CropConfig crop;
    std::map<std::string, DetectionProfileConfig> detection_profiles;
};

void validateCropConfig(const CropConfig& config);
void validateRuntimeConfig(const RuntimeConfig& config);

}  // namespace honta::config
