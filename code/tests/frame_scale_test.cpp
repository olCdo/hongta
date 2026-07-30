#include "honta/vision/frame_scale.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "honta/config/runtime_config.h"

namespace {

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 0.001;
}

bool testHalfScaleFrameAndDetectorConfig() {
    const cv::Size scaled_size =
        honta::vision::scaledFrameSize({1280, 720}, 0.5);
    if (scaled_size != cv::Size(640, 360)) {
        std::cerr << "expected 640x360, got "
                  << scaled_size.width << "x" << scaled_size.height << '\n';
        return false;
    }

    honta::vision::CircleDetectorConfig config;
    config.min_radius_px = 80;
    config.max_radius_px = 260;
    config.min_dist_px = 45.0;
    const auto scaled =
        honta::vision::scaleDetectorConfig(config, 0.5);
    if (scaled.min_radius_px != 40 ||
        scaled.max_radius_px != 130 ||
        !nearlyEqual(scaled.min_dist_px, 22.5)) {
        std::cerr << "detector geometry was not scaled by 0.5\n";
        return false;
    }
    return true;
}

bool testDetectionCoordinatesReturnToInputSpace() {
    honta::vision::CircleDetection detection;
    detection.center = {160.0F, 90.0F};
    detection.radius = 50.0F;
    detection.confidence = 0.8;

    const auto restored = honta::vision::restoreDetectionsToInput(
        std::vector<honta::vision::CircleDetection>{detection},
        0.5,
        {10, 20});
    if (restored.size() != 1 ||
        !nearlyEqual(restored[0].center.x, 330.0) ||
        !nearlyEqual(restored[0].center.y, 200.0) ||
        !nearlyEqual(restored[0].radius, 100.0)) {
        std::cerr << "detection coordinates were not restored to input space\n";
        return false;
    }
    return true;
}

bool testProcessingScaleValidation() {
    honta::config::RuntimeConfig config;
    config.input_rtsp_url = "rtsp://127.0.0.1:8554/input";
    config.overlay_public_host = "127.0.0.1";
    config.camera_config_path = "camera.json";
    config.top_cover_data_dir = "top_cover";
    config.detection_profiles.emplace(
        "entrance_hole",
        honta::config::DetectionProfileConfig{});
    config.detection_profiles.emplace(
        "center_horn",
        honta::config::DetectionProfileConfig{});

    config.processing_scale = 0.5;
    honta::config::validateRuntimeConfig(config);

    config.processing_scale = 0.0;
    try {
        honta::config::validateRuntimeConfig(config);
    } catch (const std::invalid_argument&) {
        return true;
    }
    std::cerr << "processing_scale=0 was not rejected\n";
    return false;
}

}  // namespace

int main() {
    return testHalfScaleFrameAndDetectorConfig() &&
                   testDetectionCoordinatesReturnToInputSpace() &&
                   testProcessingScaleValidation()
               ? 0
               : 1;
}
