#pragma once

#include <vector>

#include <opencv2/core.hpp>

#include "honta/config/runtime_config.h"
#include "honta/vision/circle_detector.h"

namespace honta::vision {

cv::Rect resolveCropRect(const honta::config::CropConfig& config, const cv::Size& frame_size);
cv::Mat cropFrame(const cv::Mat& frame,
                  const honta::config::CropConfig& config,
                  cv::Rect* crop_rect = nullptr);
std::vector<CircleDetection> offsetDetections(const std::vector<CircleDetection>& detections,
                                              const cv::Point& offset);

}  // namespace honta::vision
