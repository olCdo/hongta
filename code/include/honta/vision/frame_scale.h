#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "honta/vision/circle_detector.h"

namespace honta::vision {

cv::Size scaledFrameSize(const cv::Size& input_size, double scale);

CircleDetectorConfig scaleDetectorConfig(CircleDetectorConfig config,
                                         double scale);

std::vector<CircleDetection> restoreDetectionsToInput(
    const std::vector<CircleDetection>& detections,
    double scale,
    const cv::Point& crop_offset);

}  // namespace honta::vision
