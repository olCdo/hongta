#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "honta/config/config_loader.h"

namespace {

void printUsage() {
    std::cerr << "Usage: phase3_config_check --config <runtime.json> --model <model_id>\n";
}

std::string resolvePath(const std::filesystem::path& base_dir, const std::string& path) {
    const std::filesystem::path input(path);
    if (input.is_absolute()) {
        return input.string();
    }
    return (base_dir / input).lexically_normal().string();
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path;
    std::string model_id;

    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (key == "--model" && i + 1 < argc) {
            model_id = argv[++i];
        } else {
            printUsage();
            return EXIT_FAILURE;
        }
    }

    if (config_path.empty() || model_id.empty()) {
        printUsage();
        return EXIT_FAILURE;
    }

    try {
        const auto runtime = honta::config::loadRuntimeConfig(config_path);
        const std::filesystem::path config_dir =
            std::filesystem::absolute(std::filesystem::path(config_path)).parent_path();
        const std::string camera_path = resolvePath(config_dir, runtime.camera_config_path);
        const std::string top_cover_dir = resolvePath(config_dir, runtime.top_cover_data_dir);

        const auto camera = honta::config::loadCameraConfig(camera_path);
        const auto top_cover = honta::config::loadTopCoverModel(top_cover_dir, model_id);

        std::cout << "runtime_config: ok\n"
                  << "  input_rtsp_url: " << runtime.input_rtsp_url << "\n"
                  << "  overlay_bind_ip: " << runtime.overlay_bind_ip << "\n"
                  << "  overlay_url: rtsp://" << runtime.overlay_public_host << ':'
                  << runtime.overlay_port << runtime.overlay_path << "\n"
                  << "  detection_profiles: " << runtime.detection_profiles.size() << "\n"
                  << "camera_config: ok\n"
                  << "  camera_id: " << camera.camera_id << "\n"
                  << "  calibration_resolution: " << camera.image_width << 'x'
                  << camera.image_height << "\n"
                  << "top_cover_model: ok\n"
                  << "  model_id: " << top_cover.model_id << "\n"
                  << "  holes: " << top_cover.holes.size() << "\n"
                  << "  horns: " << top_cover.horns.size() << "\n"
                  << "phase3_config_check: ok\n";
    } catch (const std::exception& error) {
        std::cerr << "phase3_config_check: failed: " << error.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
