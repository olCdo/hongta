#pragma once

#include <cstdint>

namespace honta::vision {

int64_t elapsedMicrosToMonotonicPts(int64_t elapsed_micros,
                                    int ticks_per_second,
                                    int64_t previous_pts);

}  // namespace honta::vision
