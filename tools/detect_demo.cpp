#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "honta/vision/circle_detector.h"
#include "honta/vision/rtsp_reader.h"

namespace {

struct Args {
    std::string input;
    std::string output = "output_detect.jpg";
    std::string target = "generic";
    bool save_debug_image = true;
    bool output_json = false;
    int min_radius = 12;
    int max_radius = 320;
    int max_results = 10;
    double min_confidence = 0.55;
    int dark_threshold = 0;
    double canny_threshold = 120.0;
    double hough_threshold = 28.0;
    bool min_radius_set = false;
    bool max_results_set = false;
    std::string center_mode = "rim";
    double max_radius_ratio = 0.22;
    double min_rim_edge_support = 0.24;
    cv::Rect roi{};
    bool require_circle_inside_roi = false;
    bool draw_roi = true;
};

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  detect_demo --input <image|video|rtsp_url> [--type entrance|center|generic] [--output out.jpg]\n"
        << "              [--min-radius px] [--max-radius px] [--min-confidence value]\n"
        << "              [--max-results n] [--dark-threshold value]\n"
        << "              [--center-mode rim|dark|auto] [--max-radius-ratio value]\n"
        << "              [--min-rim-edge-support value] [--canny-threshold value]\n"
        << "              [--hough-threshold value] [--roi x,y,w,h]\n"
        << "              [--require-circle-inside-roi] [--hide-roi] [--json]\n";
}

bool parseRoi(const std::string& value, cv::Rect& roi) {
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream stream(normalized);
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!(stream >> x >> y >> width >> height) || width <= 0 || height <= 0) {
        return false;
    }
    roi = {x, y, width, height};
    return true;
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--input" && i + 1 < argc) {
            args.input = argv[++i];
        } else if (key == "--type" && i + 1 < argc) {
            args.target = argv[++i];
        } else if (key == "--output" && i + 1 < argc) {
            args.output = argv[++i];
        } else if (key == "--min-radius" && i + 1 < argc) {
            args.min_radius = std::stoi(argv[++i]);
            args.min_radius_set = true;
        } else if (key == "--max-radius" && i + 1 < argc) {
            args.max_radius = std::stoi(argv[++i]);
        } else if (key == "--min-confidence" && i + 1 < argc) {
            args.min_confidence = std::stod(argv[++i]);
        } else if (key == "--max-results" && i + 1 < argc) {
            args.max_results = std::stoi(argv[++i]);
            args.max_results_set = true;
        } else if (key == "--dark-threshold" && i + 1 < argc) {
            args.dark_threshold = std::stoi(argv[++i]);
        } else if (key == "--canny-threshold" && i + 1 < argc) {
            args.canny_threshold = std::stod(argv[++i]);
        } else if (key == "--hough-threshold" && i + 1 < argc) {
            args.hough_threshold = std::stod(argv[++i]);
        } else if (key == "--center-mode" && i + 1 < argc) {
            args.center_mode = argv[++i];
        } else if (key == "--max-radius-ratio" && i + 1 < argc) {
            args.max_radius_ratio = std::stod(argv[++i]);
        } else if (key == "--min-rim-edge-support" && i + 1 < argc) {
            args.min_rim_edge_support = std::stod(argv[++i]);
        } else if (key == "--roi" && i + 1 < argc) {
            if (!parseRoi(argv[++i], args.roi)) {
                std::cerr << "Invalid ROI, expected x,y,w,h\n";
                return false;
            }
        } else if (key == "--require-circle-inside-roi") {
            args.require_circle_inside_roi = true;
        } else if (key == "--hide-roi") {
            args.draw_roi = false;
        } else if (key == "--json") {
            args.output_json = true;
        } else if (key == "--no-debug-image") {
            args.save_debug_image = false;
        } else if (key == "--help" || key == "-h") {
            printUsage();
            return false;
        } else {
            std::cerr << "Unknown or incomplete argument: " << key << "\n";
            printUsage();
            return false;
        }
    }

    if (args.input.empty()) {
        printUsage();
        return false;
    }
    const honta::vision::DetectionTarget target = honta::vision::detectionTargetFromString(args.target);
    if (target == honta::vision::DetectionTarget::CenterHorn) {
        if (!args.max_results_set) {
            args.max_results = 1;
        }
    }
    return true;
}

bool looksLikeImage(const std::string& path) {
    const std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp", ".webp"};
    std::string lower = path;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const std::string& ext : extensions) {
        if (lower.size() >= ext.size() && lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0) {
            return true;
        }
    }
    return false;
}

void printJson(const std::vector<honta::vision::CircleDetection>& detections) {
    std::cout << "{\n";
    std::cout << "  \"success\": " << (!detections.empty() ? "true" : "false") << ",\n";
    std::cout << "  \"detections\": [\n";
    for (std::size_t i = 0; i < detections.size(); ++i) {
        const auto& detection = detections[i];
        std::cout << "    {\n";
        std::cout << "      \"id\": " << detection.id << ",\n";
        std::cout << "      \"center_x\": " << detection.center.x << ",\n";
        std::cout << "      \"center_y\": " << detection.center.y << ",\n";
        std::cout << "      \"radius\": " << detection.radius << ",\n";
        std::cout << "      \"confidence\": " << detection.confidence << ",\n";
        std::cout << "      \"type\": \"" << honta::vision::toString(detection.target) << "\"\n";
        std::cout << "    }" << (i + 1 == detections.size() ? "\n" : ",\n");
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}

void printSummary(const std::vector<honta::vision::CircleDetection>& detections) {
    std::cout << "success: " << (!detections.empty() ? "true" : "false") << "\n";
    std::cout << "count: " << detections.size() << "\n";
    if (detections.empty()) {
        return;
    }

    std::cout << "id\tcenter_x\tcenter_y\tradius\tconfidence\ttype\n";
    for (const auto& detection : detections) {
        std::cout << detection.id << "\t"
                  << detection.center.x << "\t"
                  << detection.center.y << "\t"
                  << detection.radius << "\t"
                  << detection.confidence << "\t"
                  << honta::vision::toString(detection.target) << "\n";
    }
}

bool readFirstFrame(const std::string& input, cv::Mat& frame) {
    if (looksLikeImage(input)) {
        frame = cv::imread(input, cv::IMREAD_COLOR);
        return !frame.empty();
    }

    honta::vision::RtspReaderConfig config;
    config.url = input;
    config.max_reconnect_attempts = 1;
    honta::vision::RtspReader reader(config);
    if (!reader.open()) {
        std::cerr << reader.lastError() << "\n";
        return false;
    }
    if (!reader.read(frame)) {
        std::cerr << reader.lastError() << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    Args args;
    if (!parseArgs(argc, argv, args)) {
        return EXIT_FAILURE;
    }

    cv::Mat frame;
    if (!readFirstFrame(args.input, frame)) {
        std::cerr << "failed to read input frame\n";
        return EXIT_FAILURE;
    }

    honta::vision::CircleDetectorConfig config;
    config.min_radius_px = args.min_radius;
    config.max_radius_px = args.max_radius;
    config.min_confidence = args.min_confidence;
    config.max_results = args.max_results;
    config.dark_threshold = args.dark_threshold;
    config.canny_high_threshold = args.canny_threshold;
    config.hough_accumulator_threshold = args.hough_threshold;
    config.center_horn_mode = honta::vision::centerHornModeFromString(args.center_mode);
    config.max_radius_image_ratio = args.max_radius_ratio;
    config.min_rim_edge_support = args.min_rim_edge_support;
    config.roi = args.roi;
    config.require_circle_inside_roi = args.require_circle_inside_roi;
    config.draw_roi = args.draw_roi;
    honta::vision::CircleDetector detector(config);
    const honta::vision::DetectionTarget target = honta::vision::detectionTargetFromString(args.target);
    const std::vector<honta::vision::CircleDetection> detections = detector.detect(frame, target);

    if (args.output_json) {
        printJson(detections);
    } else {
        printSummary(detections);
    }

    if (args.save_debug_image) {
        const cv::Mat debug = detector.drawDetections(frame, detections);
        if (!cv::imwrite(args.output, debug)) {
            std::cerr << "failed to write debug image: " << args.output << "\n";
            return EXIT_FAILURE;
        }
    }

    return detections.empty() ? EXIT_FAILURE : EXIT_SUCCESS;
}
