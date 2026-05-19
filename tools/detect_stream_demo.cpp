#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <nlohmann/json.hpp>

#include "honta/vision/circle_detector.h"
#include "honta/vision/rtsp_reader.h"

namespace {

struct Args {
    std::string input;
    int camera_index = -1;
    bool use_camera = false;
    int camera_width = 0;
    int camera_height = 0;
    double camera_fps = 0.0;
    std::string config_path;
    int min_radius = 12;
    int max_radius = 320;
    int max_results = 10;
    double min_confidence = 0.55;
    bool enable_hist_equalization = false;
    bool enable_clahe = false;
    double clahe_clip_limit = 2.0;
    int clahe_tile_grid_size = 8;
    double canny_threshold = 120.0;
    double hough_threshold = 28.0;
    double max_radius_ratio = 0.22;
    bool latest_frame_mode = true;
    double min_rim_edge_support = 0.24;
    double process_scale = 0.5;
    int detect_every = 1;
    int display_width = 1280;
    bool save_debug_stages = false;
};

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  detect_stream_demo [--config config.json]\n"
        << "                     (--input <rtsp_url|video> | --camera <index>)\n"
        << "                     [--min-radius px] [--max-radius px] [--min-confidence value]\n"
        << "                     [--max-results n] [--max-radius-ratio value]\n"
        << "                     [--min-rim-edge-support value] [--canny-threshold value]\n"
        << "                     [--hough-threshold value] [--no-latest-frame]\n"
        << "                     [--process-scale value] [--detect-every n] [--display-width px]\n"
        << "                     [--camera-width px] [--camera-height px] [--camera-fps value]\n"
        << "Keys: q/ESC quit, s save current debug frame.\n";
}

bool parseConfigPath(int argc, char** argv, std::string& config_path) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --config\n";
                return false;
            }
            config_path = argv[++i];
        }
    }
    return true;
}

template <typename T>
void setIfPresent(const nlohmann::json& object, const char* key, T& target) {
    if (object.contains(key)) {
        target = object.at(key).get<T>();
    }
}

bool loadConfig(const std::string& path, Args& args) {
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Failed to open config: " << path << "\n";
        return false;
    }

    nlohmann::json config;
    try {
        input >> config;

        if (!config.is_object()) {
            std::cerr << "Invalid config: root must be a JSON object\n";
            return false;
        }

        if (config.contains("input")) {
            const auto& input_config = config.at("input");
            if (!input_config.is_object()) {
                std::cerr << "Invalid config: input must be an object\n";
                return false;
            }
            const bool has_url = input_config.contains("url");
            const bool has_camera = input_config.contains("camera_index");
            if (has_url && has_camera) {
                std::cerr << "Invalid config: input.url and input.camera_index are mutually exclusive\n";
                return false;
            }
            if (has_url) {
                args.input = input_config.at("url").get<std::string>();
                args.camera_index = -1;
                args.use_camera = false;
            }
            if (has_camera) {
                args.camera_index = input_config.at("camera_index").get<int>();
                args.input.clear();
                args.use_camera = true;
            }
            setIfPresent(input_config, "camera_width", args.camera_width);
            setIfPresent(input_config, "camera_height", args.camera_height);
            setIfPresent(input_config, "camera_fps", args.camera_fps);
        }

        if (config.contains("detection")) {
            const auto& detection = config.at("detection");
            if (!detection.is_object()) {
                std::cerr << "Invalid config: detection must be an object\n";
                return false;
            }
            setIfPresent(detection, "min_radius_px", args.min_radius);
            setIfPresent(detection, "max_radius_px", args.max_radius);
            setIfPresent(detection, "min_confidence", args.min_confidence);
            setIfPresent(detection, "enable_hist_equalization", args.enable_hist_equalization);
            setIfPresent(detection, "enable_clahe", args.enable_clahe);
            setIfPresent(detection, "clahe_clip_limit", args.clahe_clip_limit);
            setIfPresent(detection, "clahe_tile_grid_size", args.clahe_tile_grid_size);
            setIfPresent(detection, "canny_high_threshold", args.canny_threshold);
            setIfPresent(detection, "hough_accumulator_threshold", args.hough_threshold);
            setIfPresent(detection, "max_radius_image_ratio", args.max_radius_ratio);
            setIfPresent(detection, "min_rim_edge_support", args.min_rim_edge_support);
            if (detection.contains("max_results")) {
                args.max_results = detection.at("max_results").get<int>();
            }
        }

        if (config.contains("runtime")) {
            const auto& runtime = config.at("runtime");
            if (!runtime.is_object()) {
                std::cerr << "Invalid config: runtime must be an object\n";
                return false;
            }
            setIfPresent(runtime, "process_scale", args.process_scale);
            setIfPresent(runtime, "detect_every", args.detect_every);
            setIfPresent(runtime, "display_width", args.display_width);
            setIfPresent(runtime, "latest_frame_mode", args.latest_frame_mode);
            setIfPresent(runtime, "save_debug_stages", args.save_debug_stages);
            args.detect_every = std::max(1, args.detect_every);
            args.display_width = std::max(320, args.display_width);
        }
    } catch (const nlohmann::json::exception& error) {
        std::cerr << "Invalid config: " << error.what() << "\n";
        return false;
    }

    args.config_path = path;
    return true;
}

bool parseArgs(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--config" && i + 1 < argc) {
            args.config_path = argv[++i];
        } else if (key == "--input" && i + 1 < argc) {
            args.input = argv[++i];
            args.camera_index = -1;
            args.use_camera = false;
        } else if (key == "--camera" && i + 1 < argc) {
            args.camera_index = std::stoi(argv[++i]);
            args.input.clear();
            args.use_camera = true;
        } else if (key == "--camera-width" && i + 1 < argc) {
            args.camera_width = std::stoi(argv[++i]);
        } else if (key == "--camera-height" && i + 1 < argc) {
            args.camera_height = std::stoi(argv[++i]);
        } else if (key == "--camera-fps" && i + 1 < argc) {
            args.camera_fps = std::stod(argv[++i]);
        } else if (key == "--min-radius" && i + 1 < argc) {
            args.min_radius = std::stoi(argv[++i]);
        } else if (key == "--max-radius" && i + 1 < argc) {
            args.max_radius = std::stoi(argv[++i]);
        } else if (key == "--min-confidence" && i + 1 < argc) {
            args.min_confidence = std::stod(argv[++i]);
        } else if (key == "--max-results" && i + 1 < argc) {
            args.max_results = std::stoi(argv[++i]);
        } else if (key == "--canny-threshold" && i + 1 < argc) {
            args.canny_threshold = std::stod(argv[++i]);
        } else if (key == "--hough-threshold" && i + 1 < argc) {
            args.hough_threshold = std::stod(argv[++i]);
        } else if (key == "--max-radius-ratio" && i + 1 < argc) {
            args.max_radius_ratio = std::stod(argv[++i]);
        } else if (key == "--min-rim-edge-support" && i + 1 < argc) {
            args.min_rim_edge_support = std::stod(argv[++i]);
        } else if (key == "--process-scale" && i + 1 < argc) {
            args.process_scale = std::stod(argv[++i]);
        } else if (key == "--detect-every" && i + 1 < argc) {
            args.detect_every = std::max(1, std::stoi(argv[++i]));
        } else if (key == "--display-width" && i + 1 < argc) {
            args.display_width = std::max(320, std::stoi(argv[++i]));
        } else if (key == "--no-latest-frame") {
            args.latest_frame_mode = false;
        } else {
            printUsage();
            return false;
        }
    }

    if (args.input.empty() == !args.use_camera) {
        std::cerr << "Expected exactly one input source: --input or --camera\n";
        printUsage();
        return false;
    }
    if (args.use_camera && args.camera_index < 0) {
        std::cerr << "Invalid camera index, expected >= 0\n";
        return false;
    }
    if ((args.camera_width > 0) != (args.camera_height > 0)) {
        std::cerr << "Camera width and height must be set together\n";
        return false;
    }
    if (args.process_scale <= 0.0 || args.process_scale > 1.0) {
        std::cerr << "Invalid process scale, expected 0 < value <= 1\n";
        return false;
    }
    return true;
}

std::vector<honta::vision::CircleDetection> scaleDetections(std::vector<honta::vision::CircleDetection> detections,
                                                            double scale) {
    if (scale == 1.0) {
        return detections;
    }
    const float inverse = static_cast<float>(1.0 / scale);
    for (auto& detection : detections) {
        detection.center.x *= inverse;
        detection.center.y *= inverse;
        detection.radius *= inverse;
    }
    return detections;
}

void drawStatusOverlay(cv::Mat& frame,
                       int frame_index,
                       const std::vector<honta::vision::CircleDetection>& detections) {
    const double best_confidence = detections.empty() ? 0.0 : detections.front().confidence;
    std::ostringstream status;
    status << "frame=" << frame_index
           << " detections=" << detections.size()
           << " best=" << std::fixed << std::setprecision(2) << best_confidence
           << " size=" << frame.cols << "x" << frame.rows;

    const cv::Point origin(12, 28);
    cv::putText(frame, status.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.65, {0, 0, 0}, 4, cv::LINE_AA);
    cv::putText(frame, status.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.65, {255, 255, 255}, 1, cv::LINE_AA);
}

void savePreprocessDebugImages(const honta::vision::CircleDetector& detector,
                               const cv::Mat& frame,
                               const std::string& prefix) {
    const honta::vision::PreprocessDebugImages images = detector.buildPreprocessDebugImages(frame);
    if (!images.gray.empty()) {
        cv::imwrite(prefix + "_gray.jpg", images.gray);
    }
    if (!images.blurred.empty()) {
        cv::imwrite(prefix + "_blur.jpg", images.blurred);
    }
    if (!images.preprocessed.empty()) {
        cv::imwrite(prefix + "_preprocess.jpg", images.preprocessed);
    }
    if (!images.edges.empty()) {
        cv::imwrite(prefix + "_edges.jpg", images.edges);
    }
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    Args args;
    std::string config_path;
    if (!parseConfigPath(argc, argv, config_path)) {
        return EXIT_FAILURE;
    }
    if (!config_path.empty() && !loadConfig(config_path, args)) {
        return EXIT_FAILURE;
    }
    if (!parseArgs(argc, argv, args)) {
        return EXIT_FAILURE;
    }

    honta::vision::RtspReaderConfig reader_config;
    if (!args.use_camera) {
        reader_config.url = args.input;
        reader_config.max_reconnect_attempts = 10;
        reader_config.low_latency = true;
        reader_config.buffer_size = 1;
    }

    honta::vision::CircleDetectorConfig detector_config;
    detector_config.min_radius_px = args.min_radius;
    detector_config.max_radius_px = args.max_radius;
    detector_config.min_confidence = args.min_confidence;
    detector_config.enable_hist_equalization = args.enable_hist_equalization;
    detector_config.enable_clahe = args.enable_clahe;
    detector_config.clahe_clip_limit = args.clahe_clip_limit;
    detector_config.clahe_tile_grid_size = args.clahe_tile_grid_size;
    detector_config.max_results = args.max_results;
    detector_config.canny_high_threshold = args.canny_threshold;
    detector_config.hough_accumulator_threshold = args.hough_threshold;
    detector_config.max_radius_image_ratio = args.max_radius_ratio;
    detector_config.min_rim_edge_support = args.min_rim_edge_support;
    detector_config.min_radius_px = std::max(1, static_cast<int>(std::round(detector_config.min_radius_px * args.process_scale)));
    detector_config.max_radius_px = std::max(detector_config.min_radius_px,
                                             static_cast<int>(std::round(detector_config.max_radius_px * args.process_scale)));
    detector_config.min_dist_px *= args.process_scale;

    honta::vision::CircleDetector detector(detector_config);

    cv::namedWindow("honta detect stream", cv::WINDOW_NORMAL);
    int frame_index = 0;
    int last_frame_id = -1;
    cv::Mat last_debug;
    std::vector<honta::vision::CircleDetection> last_detections;

    honta::vision::RtspReader direct_reader(reader_config);
    honta::vision::LatestFrameReader latest_reader(reader_config);
    cv::VideoCapture camera_capture;
    if (args.use_camera) {
        if (!camera_capture.open(args.camera_index, cv::CAP_ANY)) {
            std::cerr << "failed to open camera: " << args.camera_index << "\n";
            return EXIT_FAILURE;
        }
        if (args.camera_width > 0 && args.camera_height > 0) {
            camera_capture.set(cv::CAP_PROP_FRAME_WIDTH, args.camera_width);
            camera_capture.set(cv::CAP_PROP_FRAME_HEIGHT, args.camera_height);
        }
        if (args.camera_fps > 0.0) {
            camera_capture.set(cv::CAP_PROP_FPS, args.camera_fps);
        }
        std::cout << "camera " << args.camera_index
                  << " opened: "
                  << static_cast<int>(camera_capture.get(cv::CAP_PROP_FRAME_WIDTH))
                  << "x"
                  << static_cast<int>(camera_capture.get(cv::CAP_PROP_FRAME_HEIGHT))
                  << " fps="
                  << camera_capture.get(cv::CAP_PROP_FPS)
                  << "\n";
    } else if (args.latest_frame_mode) {
        latest_reader.start();
    } else if (!direct_reader.open()) {
        std::cerr << direct_reader.lastError() << "\n";
        return EXIT_FAILURE;
    }

    while (true) {
        cv::Mat frame;
        int source_frame_id = frame_index;
        if (args.use_camera) {
            if (!camera_capture.read(frame) || frame.empty()) {
                std::cerr << "read failed: camera " << args.camera_index << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
        } else if (args.latest_frame_mode) {
            if (!latest_reader.readLatest(frame, source_frame_id)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (source_frame_id == last_frame_id) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            last_frame_id = source_frame_id;
        } else {
            if (!direct_reader.read(frame)) {
                std::cerr << "read failed: " << direct_reader.lastError() << "\n";
                continue;
            }
        }

        if (frame_index % args.detect_every == 0 || last_debug.empty()) {
            cv::Mat process_frame;
            if (args.process_scale == 1.0) {
                process_frame = frame;
            } else {
                cv::resize(frame, process_frame, {}, args.process_scale, args.process_scale, cv::INTER_AREA);
            }
            last_detections = scaleDetections(detector.detect(process_frame), args.process_scale);
        }

        honta::vision::CircleDetectorConfig draw_config = detector_config;
        honta::vision::CircleDetector draw_detector(draw_config);
        last_debug = draw_detector.drawDetections(frame, last_detections);
        drawStatusOverlay(last_debug, frame_index, last_detections);

        cv::Mat display_frame = last_debug;
        if (args.display_width > 0 && last_debug.cols > args.display_width) {
            const double display_scale = static_cast<double>(args.display_width) / static_cast<double>(last_debug.cols);
            cv::resize(last_debug, display_frame, {}, display_scale, display_scale, cv::INTER_AREA);
        }
        cv::imshow("honta detect stream", display_frame);

        std::cout << "\rframe=" << frame_index
                  << " detections=" << last_detections.size()
                  << " best_confidence=" << (last_detections.empty() ? 0.0 : last_detections.front().confidence)
                  << "          " << std::flush;

        const int key = cv::waitKey(1);
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }
        if ((key == 's' || key == 'S') && !last_debug.empty()) {
            const std::string prefix = "stream_debug_" + std::to_string(frame_index);
            const std::string filename = prefix + ".jpg";
            cv::imwrite(filename, last_debug);
            std::cout << "\nsaved " << filename << "\n";
            if (args.save_debug_stages) {
                savePreprocessDebugImages(detector, frame, prefix);
                std::cout << "saved " << prefix << "_gray.jpg, "
                          << prefix << "_blur.jpg, "
                          << prefix << "_preprocess.jpg, "
                          << prefix << "_edges.jpg\n";
            }
        }

        ++frame_index;
    }

    std::cout << "\n";
    return EXIT_SUCCESS;
}
