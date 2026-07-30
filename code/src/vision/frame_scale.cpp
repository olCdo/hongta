#include "honta/vision/frame_scale.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace honta::vision {
namespace {

void validateScale(double scale) {
    if (!std::isfinite(scale) || scale <= 0.0 || scale > 1.0) {
        throw std::invalid_argument("processing scale must be in (0, 1]");
    }
}

}  // namespace

cv::Size scaledFrameSize(const cv::Size& input_size, double scale) {
    validateScale(scale);
    return {
        std::max(1, static_cast<int>(std::lround(input_size.width * scale))),
        std::max(1, static_cast<int>(std::lround(input_size.height * scale))),
    };
}

CircleDetectorConfig scaleDetectorConfig(CircleDetectorConfig config,
                                         double scale) {
    validateScale(scale);
    config.min_radius_px =
        std::max(1, static_cast<int>(std::lround(config.min_radius_px * scale)));
    config.max_radius_px =
        std::max(config.min_radius_px,
                 static_cast<int>(std::lround(config.max_radius_px * scale)));
    config.min_dist_px = std::max(1.0, config.min_dist_px * scale);
    return config;
}

std::vector<CircleDetection> restoreDetectionsToInput(
    const std::vector<CircleDetection>& detections,
    double scale,
    const cv::Point& crop_offset) {
    validateScale(scale);
    std::vector<CircleDetection> restored = detections;
    const double inverse_scale = 1.0 / scale;
    for (CircleDetection& detection : restored) {
        detection.center.x = static_cast<float>(
            detection.center.x * inverse_scale + crop_offset.x);
        detection.center.y = static_cast<float>(
            detection.center.y * inverse_scale + crop_offset.y);
        detection.radius =
            static_cast<float>(detection.radius * inverse_scale);
    }
    return restored;
}

}  // namespace honta::vision
