#pragma once

#include <vector>

#include <opencv2/core.hpp>

namespace honta::vision {

struct CircleDetectorConfig {
    int min_radius_px = 12;
    int max_radius_px = 320;
    double min_confidence = 0.55;
    double dp = 1.2;
    double min_dist_px = 45.0;
    double canny_high_threshold = 120.0;
    double hough_accumulator_threshold = 28.0;
    int gaussian_kernel_size = 7;
    bool enable_hist_equalization = false;
    bool enable_clahe = false;
    double clahe_clip_limit = 2.0;
    int clahe_tile_grid_size = 8;
    bool enable_contour_fallback = true;
    int max_results = 20;
    double max_radius_image_ratio = 0.22;
    double min_rim_edge_support = 0.24;
};

struct CircleDetection {
    int id = 0;
    cv::Point2f center{};
    float radius = 0.0F;
    double confidence = 0.0;
};

struct PreprocessDebugImages {
    cv::Mat gray;
    cv::Mat blurred;
    cv::Mat preprocessed;
    cv::Mat edges;
};

class CircleDetector {
public:
    explicit CircleDetector(CircleDetectorConfig config = {});

    std::vector<CircleDetection> detect(const cv::Mat& frame) const;
    cv::Mat drawDetections(const cv::Mat& frame, const std::vector<CircleDetection>& detections) const;
    PreprocessDebugImages buildPreprocessDebugImages(const cv::Mat& frame) const;

private:
    CircleDetectorConfig config_;

    cv::Mat toGray(const cv::Mat& frame) const;
    cv::Mat preprocess(const cv::Mat& frame) const;
    std::vector<CircleDetection> detectByHough(const cv::Mat& gray) const;
    std::vector<CircleDetection> detectByContours(const cv::Mat& gray) const;
    std::vector<CircleDetection> mergeAndRank(std::vector<CircleDetection> detections) const;
};

}  // namespace honta::vision
