#include "honta/vision/frame_timestamp.h"

#include <algorithm>

namespace honta::vision {

int64_t elapsedMicrosToMonotonicPts(int64_t elapsed_micros,
                                    int ticks_per_second,
                                    int64_t previous_pts) {
    const int64_t clamped_micros = std::max<int64_t>(0, elapsed_micros);
    const int64_t whole_seconds = clamped_micros / 1'000'000;
    const int64_t remaining_micros = clamped_micros % 1'000'000;
    const int64_t wall_clock_pts =
        whole_seconds * ticks_per_second +
        remaining_micros * ticks_per_second / 1'000'000;

    return std::max(wall_clock_pts, previous_pts + 1);
}

}  // namespace honta::vision
