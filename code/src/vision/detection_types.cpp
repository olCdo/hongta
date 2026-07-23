#include "honta/vision/detection_types.h"

namespace honta::vision {

bool isSupportedDetectType(const std::string& detect_type) {
    return detect_type == kDetectTypeEntranceHole || detect_type == kDetectTypeCenterHorn;
}

}  // namespace honta::vision
