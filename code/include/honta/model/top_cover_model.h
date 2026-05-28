#pragma once

#include <map>
#include <string>
#include <vector>

namespace honta::model {

struct TopCoverHole {
    int hole_no = 0;
    double x_mm = 0.0;
    double y_mm = 0.0;
};

struct TopCoverHorn {
    int horn_no = 0;
    double x_mm = 0.0;
    double y_mm = 0.0;
    double yaw_deg = 0.0;
};

struct TopCoverModel {
    std::string model_id;
    std::vector<TopCoverHole> holes;
    std::vector<TopCoverHorn> horns;
    std::map<int, TopCoverHole> holes_by_no;
    std::map<int, TopCoverHorn> horns_by_no;
};

void validateTopCoverModel(const TopCoverModel& model);
void buildTopCoverIndexes(TopCoverModel& model);

}  // namespace honta::model
