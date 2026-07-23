#include "honta/vision/frame_crop.h"

#include <stdexcept>
#include <string>

namespace honta::vision {

cv::Rect resolveCropRect(const honta::config::CropConfig& config, const cv::Size& frame_size) {
    if (frame_size.width <= 0 || frame_size.height <= 0) {
        throw std::invalid_argument("invalid frame size for crop");
    }
    if (!config.enabled) {
        return {0, 0, frame_size.width, frame_size.height};
    }
    honta::config::validateCropConfig(config);
    const long long crop_width =
        static_cast<long long>(frame_size.width) - config.left - config.right;
    const long long crop_height =
        static_cast<long long>(frame_size.height) - config.top - config.bottom;
    if (crop_width <= 0 || crop_height <= 0) {
        throw std::invalid_argument(
            "crop edges leave no valid frame area for frame size " + std::to_string(frame_size.width) + "x" +
            std::to_string(frame_size.height));
    }
    return {config.left, config.top, static_cast<int>(crop_width), static_cast<int>(crop_height)};
}

cv::Mat cropFrame(const cv::Mat& frame,
                  const honta::config::CropConfig& config,
                  cv::Rect* crop_rect) {
    if (frame.empty()) {
        throw std::invalid_argument("cannot crop an empty frame");
    }
    const cv::Rect rect = resolveCropRect(config, frame.size());
    if (crop_rect != nullptr) {
        *crop_rect = rect;
    }
    return frame(rect);
}

std::vector<CircleDetection> offsetDetections(const std::vector<CircleDetection>& detections,
                                              const cv::Point& offset) {
    std::vector<CircleDetection> output = detections;
    for (CircleDetection& detection : output) {
        detection.center.x += static_cast<float>(offset.x);
        detection.center.y += static_cast<float>(offset.y);
    }
    return output;
}

}  // namespace honta::vision
