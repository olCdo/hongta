#pragma once

#include <string>

#include "honta/config/camera_config.h"
#include "honta/config/runtime_config.h"
#include "honta/model/top_cover_model.h"

namespace honta::config {

RuntimeConfig loadRuntimeConfig(const std::string& path);
CameraConfig loadCameraConfig(const std::string& path);
honta::model::TopCoverModel loadTopCoverModel(const std::string& data_dir,
                                              const std::string& model_id);

}  // namespace honta::config
