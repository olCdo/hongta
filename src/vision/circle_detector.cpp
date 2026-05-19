#include "honta/vision/circle_detector.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <opencv2/imgproc.hpp>

namespace honta::vision {
namespace {

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

double distance(const cv::Point2f& a, const cv::Point2f& b) {
    const double dx = static_cast<double>(a.x - b.x);
    const double dy = static_cast<double>(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}

cv::Scalar colorForTarget(DetectionTarget target) {
    switch (target) {
        case DetectionTarget::EntranceHole:
            return {0, 220, 255};
        case DetectionTarget::CenterHorn:
            return {0, 255, 80};
        case DetectionTarget::GenericCircle:
        default:
            return {255, 160, 0};
    }
}

struct RingValidationStats {
    double contrast = 0.0;
    double edge_strength = 0.0;
    double gradient_alignment = 0.0;
};

bool hasEdgeNear(const cv::Mat& edges, int x, int y, int radius) {
    for (int yy = std::max(0, y - radius); yy <= std::min(edges.rows - 1, y + radius); ++yy) {
        for (int xx = std::max(0, x - radius); xx <= std::min(edges.cols - 1, x + radius); ++xx) {
            if (edges.at<unsigned char>(yy, xx) > 0) {
                return true;
            }
        }
    }
    return false;
}

double rimEdgeSupport(const cv::Mat& edges, const cv::Point2f& center, float radius) {
    if (edges.empty() || radius <= 0.0F) {
        return 0.0;
    }

    const int samples = 180;
    const int search_radius = std::max(1, static_cast<int>(std::round(radius * 0.025F)));
    int supported = 0;
    int valid = 0;
    for (int i = 0; i < samples; ++i) {
        const double angle = 2.0 * CV_PI * static_cast<double>(i) / static_cast<double>(samples);
        const int x = static_cast<int>(std::round(center.x + radius * std::cos(angle)));
        const int y = static_cast<int>(std::round(center.y + radius * std::sin(angle)));
        if (x < 0 || y < 0 || x >= edges.cols || y >= edges.rows) {
            continue;
        }
        ++valid;
        if (hasEdgeNear(edges, x, y, search_radius)) {
            ++supported;
        }
    }

    return valid > 0 ? static_cast<double>(supported) / static_cast<double>(valid) : 0.0;
}

RingValidationStats validateRingEdge(const cv::Mat& gray,
                                     const cv::Mat& grad_x,
                                     const cv::Mat& grad_y,
                                     const cv::Point2f& center,
                                     float radius) {
    RingValidationStats stats;
    if (gray.empty() || grad_x.empty() || grad_y.empty() || radius <= 2.0F) {
        return stats;
    }

    const int samples = 180;
    const float offset = std::max(2.0F, radius * 0.08F);
    double contrast_sum = 0.0;
    double edge_sum = 0.0;
    double alignment_sum = 0.0;
    int valid = 0;

    for (int i = 0; i < samples; ++i) {
        const double angle = 2.0 * CV_PI * static_cast<double>(i) / static_cast<double>(samples);
        const double radial_x = std::cos(angle);
        const double radial_y = std::sin(angle);

        const int ring_x = static_cast<int>(std::round(center.x + radius * radial_x));
        const int ring_y = static_cast<int>(std::round(center.y + radius * radial_y));
        const int inner_x = static_cast<int>(std::round(center.x + (radius - offset) * radial_x));
        const int inner_y = static_cast<int>(std::round(center.y + (radius - offset) * radial_y));
        const int outer_x = static_cast<int>(std::round(center.x + (radius + offset) * radial_x));
        const int outer_y = static_cast<int>(std::round(center.y + (radius + offset) * radial_y));

        if (ring_x < 0 || ring_y < 0 || ring_x >= gray.cols || ring_y >= gray.rows ||
            inner_x < 0 || inner_y < 0 || inner_x >= gray.cols || inner_y >= gray.rows ||
            outer_x < 0 || outer_y < 0 || outer_x >= gray.cols || outer_y >= gray.rows) {
            continue;
        }

        const double inner_value = gray.at<unsigned char>(inner_y, inner_x);
        const double outer_value = gray.at<unsigned char>(outer_y, outer_x);
        const double gx = grad_x.at<float>(ring_y, ring_x);
        const double gy = grad_y.at<float>(ring_y, ring_x);
        const double strength = std::sqrt(gx * gx + gy * gy);
        const double alignment = strength > 1e-6 ? std::abs((gx * radial_x + gy * radial_y) / strength) : 0.0;

        contrast_sum += std::abs(outer_value - inner_value);
        edge_sum += strength;
        alignment_sum += alignment;
        ++valid;
    }

    if (valid > 0) {
        stats.contrast = contrast_sum / static_cast<double>(valid);
        stats.edge_strength = edge_sum / static_cast<double>(valid);
        stats.gradient_alignment = alignment_sum / static_cast<double>(valid);
    }
    return stats;
}

}  // namespace

CircleDetector::CircleDetector(CircleDetectorConfig config) : config_(config) {
    if (config_.min_radius_px <= 0 || config_.max_radius_px < config_.min_radius_px) {
        throw std::invalid_argument("invalid circle radius range");
    }
    if (config_.gaussian_kernel_size <= 0) {
        config_.gaussian_kernel_size = 1;
    }
    if (config_.gaussian_kernel_size % 2 == 0) {
        ++config_.gaussian_kernel_size;
    }
    if (config_.clahe_tile_grid_size <= 0) {
        config_.clahe_tile_grid_size = 8;
    }
    if (config_.clahe_clip_limit <= 0.0) {
        config_.clahe_clip_limit = 2.0;
    }
}

std::vector<CircleDetection> CircleDetector::detect(const cv::Mat& frame, DetectionTarget target) const {
    if (frame.empty()) {
        return {};
    }

    cv::Mat gray = preprocess(frame);

    if (target == DetectionTarget::CenterHorn && config_.center_horn_mode != CenterHornMode::Rim) {
        std::vector<CircleDetection> dark_detections = detectDarkCircularRegions(gray, target);
        filterByRoi(dark_detections);
        if (!dark_detections.empty()) {
            return mergeAndRank(std::move(dark_detections));
        }
        if (config_.center_horn_mode == CenterHornMode::Dark) {
            return {};
        }
    }

    std::vector<CircleDetection> detections = detectByHough(gray, target);

    if (config_.enable_contour_fallback) {
        std::vector<CircleDetection> contour_detections = detectByContours(gray, target);
        detections.insert(detections.end(), contour_detections.begin(), contour_detections.end());
    }

    filterByRoi(detections);
    return mergeAndRank(std::move(detections));
}

cv::Mat CircleDetector::drawDetections(const cv::Mat& frame, const std::vector<CircleDetection>& detections) const {
    cv::Mat output;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, output, cv::COLOR_GRAY2BGR);
    } else {
        output = frame.clone();
    }

    if (config_.draw_roi && config_.roi.width > 0 && config_.roi.height > 0) {
        const cv::Rect image_rect(0, 0, output.cols, output.rows);
        const cv::Rect roi = config_.roi & image_rect;
        if (roi.width > 0 && roi.height > 0) {
            cv::rectangle(output, roi, {255, 180, 0}, 2, cv::LINE_AA);
        }
    }

    for (const CircleDetection& detection : detections) {
        const cv::Scalar color = colorForTarget(detection.target);
        const cv::Point center(static_cast<int>(std::round(detection.center.x)),
                               static_cast<int>(std::round(detection.center.y)));
        cv::circle(output, center, static_cast<int>(std::round(detection.radius)), color, 2, cv::LINE_AA);
        cv::circle(output, center, 3, {0, 0, 255}, -1, cv::LINE_AA);

        std::ostringstream label;
        label << "#" << detection.id << " " << std::fixed << std::setprecision(2) << detection.confidence;
        const cv::Point origin(static_cast<int>(detection.center.x + detection.radius + 6),
                               static_cast<int>(detection.center.y));
        cv::putText(output, label.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.55, color, 2, cv::LINE_AA);
    }

    return output;
}

cv::Mat CircleDetector::toGray(const cv::Mat& frame) const {
    if (frame.channels() == 1) {
        return frame.clone();
    }

    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, gray, cv::COLOR_BGRA2GRAY);
    } else {
        throw std::invalid_argument("unsupported image channel count");
    }

    return gray;
}

cv::Mat CircleDetector::preprocess(const cv::Mat& frame) const {
    cv::Mat blurred;
    cv::GaussianBlur(toGray(frame), blurred, {config_.gaussian_kernel_size, config_.gaussian_kernel_size}, 1.5);

    cv::Mat preprocessed;
    if (config_.enable_clahe) {
        const cv::Size tile_size(config_.clahe_tile_grid_size, config_.clahe_tile_grid_size);
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(config_.clahe_clip_limit, tile_size);
        clahe->apply(blurred, preprocessed);
    } else if (config_.enable_hist_equalization) {
        cv::equalizeHist(blurred, preprocessed);
    } else {
        preprocessed = blurred.clone();
    }
    return preprocessed;
}

PreprocessDebugImages CircleDetector::buildPreprocessDebugImages(const cv::Mat& frame) const {
    PreprocessDebugImages images;
    if (frame.empty()) {
        return images;
    }

    images.gray = toGray(frame);
    cv::GaussianBlur(images.gray, images.blurred, {config_.gaussian_kernel_size, config_.gaussian_kernel_size}, 1.5);

    if (config_.enable_clahe) {
        const cv::Size tile_size(config_.clahe_tile_grid_size, config_.clahe_tile_grid_size);
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(config_.clahe_clip_limit, tile_size);
        clahe->apply(images.blurred, images.preprocessed);
    } else if (config_.enable_hist_equalization) {
        cv::equalizeHist(images.blurred, images.preprocessed);
    } else {
        images.preprocessed = images.blurred.clone();
    }

    cv::Canny(images.preprocessed, images.edges, config_.canny_high_threshold * 0.5, config_.canny_high_threshold);
    return images;
}

std::vector<CircleDetection> CircleDetector::detectByHough(const cv::Mat& gray, DetectionTarget target) const {
    cv::Mat edges;
    cv::Canny(gray, edges, config_.canny_high_threshold * 0.5, config_.canny_high_threshold);
    cv::Mat grad_x;
    cv::Mat grad_y;
    if (target == DetectionTarget::CenterHorn && config_.enable_ring_validation) {
        cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
        cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);
    }

    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray,
                     circles,
                     cv::HOUGH_GRADIENT,
                     config_.dp,
                     config_.min_dist_px,
                     config_.canny_high_threshold,
                     config_.hough_accumulator_threshold,
                     config_.min_radius_px,
                     config_.max_radius_px);

    std::vector<CircleDetection> detections;
    detections.reserve(circles.size());

    for (const cv::Vec3f& circle : circles) {
        const float radius = circle[2];
        const double max_radius_by_image = std::min(gray.cols, gray.rows) * config_.max_radius_image_ratio;
        if (radius > max_radius_by_image) {
            continue;
        }
        const double radius_score = clamp01(static_cast<double>(radius - config_.min_radius_px) /
                                            static_cast<double>(config_.max_radius_px - config_.min_radius_px + 1));
        const double support = rimEdgeSupport(edges, {circle[0], circle[1]}, radius);
        if (target == DetectionTarget::CenterHorn && support < config_.min_rim_edge_support) {
            continue;
        }
        RingValidationStats ring_stats;
        if (target == DetectionTarget::CenterHorn && config_.enable_ring_validation) {
            ring_stats = validateRingEdge(gray, grad_x, grad_y, {circle[0], circle[1]}, radius);
            if (ring_stats.contrast < config_.min_ring_contrast ||
                ring_stats.edge_strength < config_.min_ring_edge_strength ||
                ring_stats.gradient_alignment < config_.min_ring_gradient_alignment) {
                continue;
            }
        }
        const double contrast_score = clamp01(ring_stats.contrast / 40.0);
        const double edge_score = clamp01(ring_stats.edge_strength / 80.0);
        double confidence = clamp01(0.55 + 0.35 * radius_score);
        if (target == DetectionTarget::CenterHorn) {
            confidence = config_.enable_ring_validation
                             ? clamp01(0.10 + 0.10 * radius_score + 0.30 * support +
                                       0.20 * contrast_score + 0.20 * edge_score +
                                       0.20 * ring_stats.gradient_alignment)
                             : clamp01(0.20 + 0.35 * radius_score + 0.60 * support);
        }
        if (confidence < config_.min_confidence) {
            continue;
        }

        CircleDetection detection;
        detection.center = {circle[0], circle[1]};
        detection.radius = radius;
        detection.confidence = confidence;
        detection.target = target;
        detections.push_back(detection);
    }

    return detections;
}

std::vector<CircleDetection> CircleDetector::detectByContours(const cv::Mat& gray, DetectionTarget target) const {
    cv::Mat edges;
    cv::Canny(gray, edges, config_.canny_high_threshold * 0.5, config_.canny_high_threshold);
    cv::Mat grad_x;
    cv::Mat grad_y;
    if (target == DetectionTarget::CenterHorn && config_.enable_ring_validation) {
        cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
        cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<CircleDetection> detections;
    for (const std::vector<cv::Point>& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area <= 0.0) {
            continue;
        }

        cv::Point2f center;
        float radius = 0.0F;
        cv::minEnclosingCircle(contour, center, radius);
        const double max_radius_by_image = std::min(gray.cols, gray.rows) * config_.max_radius_image_ratio;
        if (radius > max_radius_by_image) {
            continue;
        }
        if (radius < config_.min_radius_px || radius > config_.max_radius_px) {
            continue;
        }

        const double circle_area = CV_PI * static_cast<double>(radius) * static_cast<double>(radius);
        const double fill_ratio = area / std::max(circle_area, 1.0);
        const double perimeter = cv::arcLength(contour, true);
        const double circularity = perimeter > 0.0 ? 4.0 * CV_PI * area / (perimeter * perimeter) : 0.0;
        const double support = rimEdgeSupport(edges, center, radius);
        if (target == DetectionTarget::CenterHorn && support < config_.min_rim_edge_support) {
            continue;
        }
        RingValidationStats ring_stats;
        if (target == DetectionTarget::CenterHorn && config_.enable_ring_validation) {
            ring_stats = validateRingEdge(gray, grad_x, grad_y, center, radius);
            if (ring_stats.contrast < config_.min_ring_contrast ||
                ring_stats.edge_strength < config_.min_ring_edge_strength ||
                ring_stats.gradient_alignment < config_.min_ring_gradient_alignment) {
                continue;
            }
        }
        const double contrast_score = clamp01(ring_stats.contrast / 40.0);
        const double edge_score = clamp01(ring_stats.edge_strength / 80.0);
        double confidence = clamp01(0.55 * circularity + 0.45 * std::min(fill_ratio, 1.0));
        if (target == DetectionTarget::CenterHorn) {
            confidence = config_.enable_ring_validation
                             ? clamp01(0.15 * circularity + 0.10 * std::min(fill_ratio, 1.0) +
                                       0.25 * support + 0.20 * contrast_score +
                                       0.20 * edge_score + 0.20 * ring_stats.gradient_alignment)
                             : clamp01(0.45 * circularity + 0.25 * std::min(fill_ratio, 1.0) + 0.35 * support);
        }
        if (confidence < config_.min_confidence) {
            continue;
        }

        CircleDetection detection;
        detection.center = center;
        detection.radius = radius;
        detection.confidence = confidence;
        detection.target = target;
        detections.push_back(detection);
    }

    return detections;
}

std::vector<CircleDetection> CircleDetector::detectDarkCircularRegions(const cv::Mat& gray, DetectionTarget target) const {
    cv::Mat mask;
    if (config_.dark_threshold > 0) {
        cv::threshold(gray, mask, config_.dark_threshold, 255, cv::THRESH_BINARY_INV);
    } else {
        cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    }

    const int kernel_size = std::max(5, (config_.min_radius_px / 6) * 2 + 1);
    const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, {kernel_size, kernel_size});
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<CircleDetection> detections;
    const cv::Rect image_rect(0, 0, gray.cols, gray.rows);
    for (const std::vector<cv::Point>& contour : contours) {
        const cv::Rect bounds = cv::boundingRect(contour);
        if (bounds.x <= 1 || bounds.y <= 1 || bounds.br().x >= image_rect.width - 1 ||
            bounds.br().y >= image_rect.height - 1) {
            continue;
        }

        const double area = cv::contourArea(contour);
        if (area <= 0.0) {
            continue;
        }

        cv::Point2f center;
        float radius = 0.0F;
        cv::minEnclosingCircle(contour, center, radius);
        const double max_radius_by_image = std::min(gray.cols, gray.rows) * config_.max_radius_image_ratio;
        if (radius > max_radius_by_image) {
            continue;
        }
        if (radius < config_.min_radius_px || radius > config_.max_radius_px) {
            continue;
        }

        const double perimeter = cv::arcLength(contour, true);
        const double circularity = perimeter > 0.0 ? 4.0 * CV_PI * area / (perimeter * perimeter) : 0.0;
        const double circle_area = CV_PI * static_cast<double>(radius) * static_cast<double>(radius);
        const double fill_ratio = area / std::max(circle_area, 1.0);
        const double aspect = static_cast<double>(std::min(bounds.width, bounds.height)) /
                              std::max(1.0, static_cast<double>(std::max(bounds.width, bounds.height)));

        if (circularity < config_.min_circularity || fill_ratio < config_.min_fill_ratio || aspect < 0.55) {
            continue;
        }

        CircleDetection detection;
        detection.center = center;
        detection.radius = radius;
        detection.confidence = clamp01(0.45 * circularity + 0.35 * std::min(fill_ratio, 1.0) + 0.20 * aspect);
        detection.target = target;
        detections.push_back(detection);
    }

    return detections;
}

void CircleDetector::filterByRoi(std::vector<CircleDetection>& detections) const {
    if (config_.roi.width <= 0 || config_.roi.height <= 0) {
        return;
    }

    detections.erase(std::remove_if(detections.begin(), detections.end(), [this](const CircleDetection& detection) {
                         const float left = detection.center.x - detection.radius;
                         const float right = detection.center.x + detection.radius;
                         const float top = detection.center.y - detection.radius;
                         const float bottom = detection.center.y + detection.radius;
                         const float roi_right = static_cast<float>(config_.roi.x + config_.roi.width);
                         const float roi_bottom = static_cast<float>(config_.roi.y + config_.roi.height);

                         if (config_.require_circle_inside_roi) {
                             return left < config_.roi.x || top < config_.roi.y || right >= roi_right ||
                                    bottom >= roi_bottom;
                         }

                         return detection.center.x < config_.roi.x || detection.center.y < config_.roi.y ||
                                detection.center.x >= roi_right || detection.center.y >= roi_bottom;
                     }),
                     detections.end());
}

std::vector<CircleDetection> CircleDetector::mergeAndRank(std::vector<CircleDetection> detections) const {
    std::sort(detections.begin(), detections.end(), [](const CircleDetection& lhs, const CircleDetection& rhs) {
        return lhs.confidence > rhs.confidence;
    });

    std::vector<CircleDetection> merged;
    for (const CircleDetection& detection : detections) {
        bool duplicate = false;
        for (const CircleDetection& existing : merged) {
            const double center_distance = distance(detection.center, existing.center);
            const double radius_distance = std::abs(static_cast<double>(detection.radius - existing.radius));
            if (center_distance < std::max(8.0, existing.radius * 0.35) &&
                radius_distance < std::max(6.0, existing.radius * 0.25)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            merged.push_back(detection);
        }
    }

    for (std::size_t i = 0; i < merged.size(); ++i) {
        merged[i].id = static_cast<int>(i + 1);
    }

    if (config_.max_results > 0 && merged.size() > static_cast<std::size_t>(config_.max_results)) {
        merged.resize(static_cast<std::size_t>(config_.max_results));
    }

    return merged;
}

std::string toString(DetectionTarget target) {
    switch (target) {
        case DetectionTarget::EntranceHole:
            return "entrance_hole";
        case DetectionTarget::CenterHorn:
            return "center_horn";
        case DetectionTarget::GenericCircle:
        default:
            return "generic_circle";
    }
}

DetectionTarget detectionTargetFromString(const std::string& value) {
    if (value == "entrance" || value == "entrance_hole") {
        return DetectionTarget::EntranceHole;
    }
    if (value == "center" || value == "center_horn") {
        return DetectionTarget::CenterHorn;
    }
    return DetectionTarget::GenericCircle;
}

CenterHornMode centerHornModeFromString(const std::string& value) {
    if (value == "dark") {
        return CenterHornMode::Dark;
    }
    if (value == "auto") {
        return CenterHornMode::Auto;
    }
    return CenterHornMode::Rim;
}

}  // namespace honta::vision
