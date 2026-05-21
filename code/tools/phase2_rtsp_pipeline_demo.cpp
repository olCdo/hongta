#include <atomic>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "honta/vision/detection_service.h"

namespace {

std::atomic_bool g_stop_requested{false};

void handleSignal(int) {
    g_stop_requested.store(true);
}

template <typename T>
void setIfPresent(const nlohmann::json& object, const char* key, T& target) {
    if (object.contains(key)) {
        target = object.at(key).get<T>();
    }
}

honta::vision::CircleDetectorConfig parseDetectorConfig(const nlohmann::json& profile) {
    honta::vision::CircleDetectorConfig config;
    setIfPresent(profile, "min_radius_px", config.min_radius_px);
    setIfPresent(profile, "max_radius_px", config.max_radius_px);
    setIfPresent(profile, "min_confidence", config.min_confidence);
    setIfPresent(profile, "dp", config.dp);
    setIfPresent(profile, "min_dist_px", config.min_dist_px);
    setIfPresent(profile, "canny_high_threshold", config.canny_high_threshold);
    setIfPresent(profile, "hough_accumulator_threshold", config.hough_accumulator_threshold);
    setIfPresent(profile, "gaussian_kernel_size", config.gaussian_kernel_size);
    setIfPresent(profile, "enable_hist_equalization", config.enable_hist_equalization);
    setIfPresent(profile, "enable_clahe", config.enable_clahe);
    setIfPresent(profile, "clahe_clip_limit", config.clahe_clip_limit);
    setIfPresent(profile, "clahe_tile_grid_size", config.clahe_tile_grid_size);
    setIfPresent(profile, "enable_contour_fallback", config.enable_contour_fallback);
    setIfPresent(profile, "max_results", config.max_results);
    setIfPresent(profile, "max_radius_image_ratio", config.max_radius_image_ratio);
    setIfPresent(profile, "min_rim_edge_support", config.min_rim_edge_support);
    return config;
}

bool loadConfig(const std::string& path, honta::vision::DetectionServiceConfig& config) {
    std::ifstream input(path);
    if (!input) {
        std::cerr << "failed to open config: " << path << "\n";
        return false;
    }

    nlohmann::json root;
    input >> root;

    if (root.contains("rtsp")) {
        const auto& rtsp = root.at("rtsp");
        setIfPresent(rtsp, "input_url", config.input.url);
        setIfPresent(rtsp, "rtsp_transport", config.input.rtsp_transport);
        setIfPresent(rtsp, "open_timeout_ms", config.input.open_timeout_ms);
        setIfPresent(rtsp, "read_timeout_ms", config.input.read_timeout_ms);
        setIfPresent(rtsp, "low_latency", config.input.low_latency);
        setIfPresent(rtsp, "overlay_bind_ip", config.overlay.bind_ip);
        setIfPresent(rtsp, "overlay_public_host", config.overlay.public_host);
        setIfPresent(rtsp, "overlay_port", config.overlay.port);
        setIfPresent(rtsp, "overlay_path", config.overlay.path);
        setIfPresent(rtsp, "output_fps", config.overlay.fps);
        setIfPresent(rtsp, "output_bitrate", config.overlay.bitrate);
    }

    if (root.contains("runtime")) {
        const auto& runtime = root.at("runtime");
        setIfPresent(runtime, "reconnect_interval_ms", config.reconnect_interval_ms);
        setIfPresent(runtime, "max_reconnect_attempts", config.max_reconnect_attempts);
    }

    if (!root.contains("detection_profiles") || !root.at("detection_profiles").is_object()) {
        std::cerr << "config missing detection_profiles object\n";
        return false;
    }
    const auto& profiles = root.at("detection_profiles");
    for (const std::string detect_type : {std::string(honta::vision::kDetectTypeEntranceHole),
                                          std::string(honta::vision::kDetectTypeCenterHorn)}) {
        if (!profiles.contains(detect_type)) {
            std::cerr << "config missing detection profile: " << detect_type << "\n";
            return false;
        }
        honta::vision::DetectionProfile profile;
        profile.detect_type = detect_type;
        profile.detector_config = parseDetectorConfig(profiles.at(detect_type));
        config.profiles.emplace(detect_type, profile);
    }

    return true;
}

void printUsage() {
    std::cerr << "Usage: phase2_rtsp_pipeline_demo --config <config.json>"
              << " [--detect-type entrance_hole|center_horn] [--duration-sec n]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::string config_path;
    std::string detect_type = honta::vision::kDetectTypeEntranceHole;
    int duration_sec = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (key == "--detect-type" && i + 1 < argc) {
            detect_type = argv[++i];
        } else if (key == "--duration-sec" && i + 1 < argc) {
            duration_sec = std::stoi(argv[++i]);
        } else {
            printUsage();
            return EXIT_FAILURE;
        }
    }

    if (config_path.empty()) {
        printUsage();
        return EXIT_FAILURE;
    }

    honta::vision::DetectionServiceConfig config;
    try {
        if (!loadConfig(config_path, config)) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << "invalid config: " << error.what() << "\n";
        return EXIT_FAILURE;
    }

    honta::vision::DetectionService service;
    service.configure(std::move(config));

    std::cout << "overlay RTSP URL: " << service.overlayRtspUrl() << "\n";
    if (!service.startDetect(detect_type)) {
        std::cerr << "failed to start detection: " << service.lastError() << "\n";
        return EXIT_FAILURE;
    }

    const auto start_time = std::chrono::steady_clock::now();
    while (!g_stop_requested.load()) {
        const honta::vision::DetectionSessionSnapshot snapshot = service.snapshot();
        std::cout << "\rstate=" << honta::vision::toString(snapshot.state)
                  << " type=" << snapshot.detect_type
                  << " frame=" << snapshot.frame_id
                  << " candidates=" << snapshot.latest_candidates.size()
                  << " last_error=" << snapshot.last_error << "        " << std::flush;

        if (snapshot.state == honta::vision::DetectionSessionState::Error) {
            break;
        }
        if (duration_sec > 0) {
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= std::chrono::seconds(duration_sec)) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\nstopping...\n";
    service.stopDetect();
    return EXIT_SUCCESS;
}
