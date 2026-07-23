#pragma once

#include <cstdint>
#include <string>

namespace honta::vision {

constexpr const char* kDetectTypeEntranceHole = "entrance_hole";
constexpr const char* kDetectTypeCenterHorn = "center_horn";

struct DetectionCandidate {
    int candidate_id = 0;
    std::string detect_type;
    double center_x = 0.0;
    double center_y = 0.0;
    double radius = 0.0;
    double confidence = 0.0;
    int frame_id = 0;
    std::int64_t timestamp_ms = 0;
};

bool isSupportedDetectType(const std::string& detect_type);

}  // namespace honta::vision
