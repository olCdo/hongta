#include "honta/vision/frame_timestamp.h"

#include <cstdint>
#include <iostream>

namespace {

bool testPtsTracksWallClock() {
    const int64_t pts =
        honta::vision::elapsedMicrosToMonotonicPts(3'200'000, 10, 0);
    if (pts != 32) {
        std::cerr << "expected 3.2 seconds to map to pts=32, got "
                  << pts << '\n';
        return false;
    }
    return true;
}

bool testPtsRemainsStrictlyMonotonicWithinOneTick() {
    const int64_t pts =
        honta::vision::elapsedMicrosToMonotonicPts(320'000, 10, 3);
    if (pts != 4) {
        std::cerr << "expected duplicate wall-clock tick to advance to pts=4, got "
                  << pts << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    return testPtsTracksWallClock() &&
                   testPtsRemainsStrictlyMonotonicWithinOneTick()
               ? 0
               : 1;
}
