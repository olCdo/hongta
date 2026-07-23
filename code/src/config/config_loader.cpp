#include "honta/config/config_loader.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace honta::config {
namespace {

nlohmann::json loadJsonFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open JSON file: " + path);
    }

    nlohmann::json json;
    input >> json;
    return json;
}

template <typename T>
void readIfPresent(const nlohmann::json& object, const char* key, T& target) {
    if (object.contains(key)) {
        target = object.at(key).get<T>();
    }
}

template <typename T>
void readAliasIfPresent(const nlohmann::json& object,
                        const char* primary_key,
                        const char* fallback_key,
                        T& target) {
    if (object.contains(primary_key)) {
        target = object.at(primary_key).get<T>();
    } else if (object.contains(fallback_key)) {
        target = object.at(fallback_key).get<T>();
    }
}

void rejectLegacyCropFields(const nlohmann::json& crop) {
    if (crop.contains("x") || crop.contains("y") ||
        crop.contains("width") || crop.contains("height")) {
        throw std::invalid_argument(
            "crop x/y/width/height are no longer supported; use left/top/right/bottom");
    }
}

}  // namespace

RuntimeConfig loadRuntimeConfig(const std::string& path) {
    const nlohmann::json root = loadJsonFile(path);
    RuntimeConfig config;

    if (root.contains("rtsp")) {
        const auto& rtsp = root.at("rtsp");
        readIfPresent(rtsp, "input_url", config.input_rtsp_url);
        readIfPresent(rtsp, "overlay_bind_ip", config.overlay_bind_ip);
        readIfPresent(rtsp, "overlay_public_host", config.overlay_public_host);
        readIfPresent(rtsp, "overlay_port", config.overlay_port);
        readIfPresent(rtsp, "overlay_path", config.overlay_path);
    }

    if (root.contains("runtime")) {
        const auto& runtime = root.at("runtime");
        readIfPresent(runtime, "camera_config_path", config.camera_config_path);
        readIfPresent(runtime, "top_cover_data_dir", config.top_cover_data_dir);
    }

    if (root.contains("crop")) {
        const auto& crop = root.at("crop");
        if (!crop.is_object()) {
            throw std::invalid_argument("crop must be a JSON object");
        }
        rejectLegacyCropFields(crop);
        readIfPresent(crop, "enabled", config.crop.enabled);
        readIfPresent(crop, "left", config.crop.left);
        readIfPresent(crop, "top", config.crop.top);
        readIfPresent(crop, "right", config.crop.right);
        readIfPresent(crop, "bottom", config.crop.bottom);
    }

    if (root.contains("detection_profiles")) {
        const auto& profiles = root.at("detection_profiles");
        for (const auto& item : profiles.items()) {
            DetectionProfileConfig profile;
            readIfPresent(item.value(), "min_radius_px", profile.min_radius_px);
            readIfPresent(item.value(), "max_radius_px", profile.max_radius_px);
            readIfPresent(item.value(), "min_confidence", profile.min_confidence);
            readIfPresent(item.value(), "min_rim_edge_support", profile.min_rim_edge_support);
            readIfPresent(item.value(), "canny_high_threshold", profile.canny_high_threshold);
            readIfPresent(item.value(), "hough_accumulator_threshold", profile.hough_accumulator_threshold);
            readIfPresent(item.value(), "enable_clahe", profile.enable_clahe);
            readIfPresent(item.value(), "enable_hist_equalization", profile.enable_hist_equalization);
            readIfPresent(item.value(), "max_radius_image_ratio", profile.max_radius_image_ratio);
            readIfPresent(item.value(), "max_results", profile.max_results);
            config.detection_profiles.emplace(item.key(), profile);
        }
    }

    validateRuntimeConfig(config);
    return config;
}

CameraConfig loadCameraConfig(const std::string& path) {
    const nlohmann::json root = loadJsonFile(path);
    CameraConfig config;

    readIfPresent(root, "camera_id", config.camera_id);
    readIfPresent(root, "image_width", config.image_width);
    readIfPresent(root, "image_height", config.image_height);

    if (root.contains("intrinsic")) {
        const auto& intrinsic = root.at("intrinsic");
        readIfPresent(intrinsic, "fx", config.fx);
        readIfPresent(intrinsic, "fy", config.fy);
        readIfPresent(intrinsic, "cx", config.cx);
        readIfPresent(intrinsic, "cy", config.cy);
    }

    if (root.contains("distortion")) {
        const auto& distortion = root.at("distortion");
        readIfPresent(distortion, "k1", config.k1);
        readIfPresent(distortion, "k2", config.k2);
        readIfPresent(distortion, "p1", config.p1);
        readIfPresent(distortion, "p2", config.p2);
        readIfPresent(distortion, "k3", config.k3);
    }

    if (root.contains("mount")) {
        const auto& mount = root.at("mount");
        readAliasIfPresent(mount, "x_mm", "x", config.mount_x_mm);
        readAliasIfPresent(mount, "y_mm", "y", config.mount_y_mm);
        readAliasIfPresent(mount, "z_mm", "z", config.mount_z_mm);
        readAliasIfPresent(mount, "yaw_deg", "yaw", config.mount_yaw_deg);
        readAliasIfPresent(mount, "pitch_deg", "pitch", config.mount_pitch_deg);
        readAliasIfPresent(mount, "roll_deg", "roll", config.mount_roll_deg);
    }

    validateCameraConfig(config);
    return config;
}

honta::model::TopCoverModel loadTopCoverModel(const std::string& data_dir,
                                              const std::string& model_id) {
    if (model_id.empty()) {
        throw std::invalid_argument("model_id is required");
    }

    const std::filesystem::path model_path =
        std::filesystem::path(data_dir) / (model_id + ".json");
    const nlohmann::json root = loadJsonFile(model_path.string());

    honta::model::TopCoverModel model;
    readIfPresent(root, "model_id", model.model_id);
    if (model.model_id != model_id) {
        throw std::invalid_argument("top cover file model_id does not match requested model_id");
    }

    if (root.contains("holes")) {
        for (const auto& item : root.at("holes")) {
            honta::model::TopCoverHole hole;
            readIfPresent(item, "hole_no", hole.hole_no);
            readAliasIfPresent(item, "x_mm", "x", hole.x_mm);
            readAliasIfPresent(item, "y_mm", "y", hole.y_mm);
            model.holes.push_back(hole);
        }
    }

    if (root.contains("horns")) {
        for (const auto& item : root.at("horns")) {
            honta::model::TopCoverHorn horn;
            readIfPresent(item, "horn_no", horn.horn_no);
            readAliasIfPresent(item, "x_mm", "x", horn.x_mm);
            readAliasIfPresent(item, "y_mm", "y", horn.y_mm);
            readAliasIfPresent(item, "yaw_deg", "yaw", horn.yaw_deg);
            model.horns.push_back(horn);
        }
    }

    honta::model::validateTopCoverModel(model);
    honta::model::buildTopCoverIndexes(model);
    return model;
}

}  // namespace honta::config
