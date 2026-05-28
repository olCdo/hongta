#include "honta/model/top_cover_model.h"

#include <cmath>
#include <stdexcept>

namespace honta::model {

void buildTopCoverIndexes(TopCoverModel& model) {
    model.holes_by_no.clear();
    model.horns_by_no.clear();

    for (const TopCoverHole& hole : model.holes) {
        if (!model.holes_by_no.emplace(hole.hole_no, hole).second) {
            throw std::invalid_argument("duplicate hole_no: " + std::to_string(hole.hole_no));
        }
    }

    for (const TopCoverHorn& horn : model.horns) {
        if (!model.horns_by_no.emplace(horn.horn_no, horn).second) {
            throw std::invalid_argument("duplicate horn_no: " + std::to_string(horn.horn_no));
        }
    }
}

void validateTopCoverModel(const TopCoverModel& model) {
    if (model.model_id.empty()) {
        throw std::invalid_argument("model_id is required");
    }
    if (model.holes.empty()) {
        throw std::invalid_argument("holes must not be empty");
    }
    if (model.horns.empty()) {
        throw std::invalid_argument("horns must not be empty");
    }

    for (const TopCoverHole& hole : model.holes) {
        if (hole.hole_no <= 0) {
            throw std::invalid_argument("hole_no must be greater than 0");
        }
        if (!std::isfinite(hole.x_mm) || !std::isfinite(hole.y_mm)) {
            throw std::invalid_argument("hole coordinates must be finite");
        }
    }
    for (const TopCoverHorn& horn : model.horns) {
        if (horn.horn_no <= 0) {
            throw std::invalid_argument("horn_no must be greater than 0");
        }
        if (!std::isfinite(horn.x_mm) || !std::isfinite(horn.y_mm) ||
            !std::isfinite(horn.yaw_deg)) {
            throw std::invalid_argument("horn coordinates must be finite");
        }
    }

    TopCoverModel indexed = model;
    buildTopCoverIndexes(indexed);

    if (indexed.holes_by_no.find(1) == indexed.holes_by_no.end()) {
        throw std::invalid_argument("top cover model must contain hole_no = 1");
    }
    if (indexed.horns_by_no.find(1) == indexed.horns_by_no.end()) {
        throw std::invalid_argument("top cover model must contain horn_no = 1");
    }
}

}  // namespace honta::model
