#include "honta/vision/latest_frame_mailbox.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

honta::vision::DecodedFrame makeFrame(std::int64_t pts) {
    honta::vision::DecodedFrame frame;
    frame.pts = pts;
    frame.width = 1920;
    frame.height = 1080;
    return frame;
}

bool testSlowConsumerReceivesNewestFrame() {
    honta::vision::LatestFrameMailbox mailbox;
    mailbox.publish(makeFrame(1));
    mailbox.publish(makeFrame(2));
    mailbox.publish(makeFrame(3));

    honta::vision::DecodedFrame output;
    std::uint64_t cursor = 0;
    std::uint64_t skipped_frames = 0;
    const auto result = mailbox.waitNext(
        output,
        cursor,
        skipped_frames,
        std::chrono::milliseconds(10));

    if (result != honta::vision::LatestFrameWaitResult::Frame) {
        std::cerr << "expected a frame\n";
        return false;
    }
    if (output.pts != 3 || cursor != 3 || skipped_frames != 2) {
        std::cerr << "expected latest pts=3, cursor=3, skipped=2; got pts="
                  << output.pts << ", cursor=" << cursor
                  << ", skipped=" << skipped_frames << '\n';
        return false;
    }
    return true;
}

bool testConsumerDoesNotReceiveSameFrameTwice() {
    honta::vision::LatestFrameMailbox mailbox;
    mailbox.publish(makeFrame(7));

    honta::vision::DecodedFrame output;
    std::uint64_t cursor = 0;
    std::uint64_t skipped_frames = 0;
    if (mailbox.waitNext(
            output,
            cursor,
            skipped_frames,
            std::chrono::milliseconds(10)) !=
        honta::vision::LatestFrameWaitResult::Frame) {
        std::cerr << "initial frame was not delivered\n";
        return false;
    }

    const auto second_result = mailbox.waitNext(
        output,
        cursor,
        skipped_frames,
        std::chrono::milliseconds(10));
    if (second_result != honta::vision::LatestFrameWaitResult::Timeout) {
        std::cerr << "same frame was delivered more than once\n";
        return false;
    }
    return true;
}

bool testInputErrorWakesConsumer() {
    honta::vision::LatestFrameMailbox mailbox;
    mailbox.fail("camera disconnected");

    honta::vision::DecodedFrame output;
    std::uint64_t cursor = 0;
    std::uint64_t skipped_frames = 0;
    const auto result = mailbox.waitNext(
        output,
        cursor,
        skipped_frames,
        std::chrono::milliseconds(10));

    if (result != honta::vision::LatestFrameWaitResult::Error) {
        std::cerr << "expected input error\n";
        return false;
    }
    if (mailbox.lastError() != "camera disconnected") {
        std::cerr << "unexpected input error: " << mailbox.lastError() << '\n';
        return false;
    }
    return true;
}

bool testStoppedMailboxRejectsLateFrame() {
    honta::vision::LatestFrameMailbox mailbox;
    mailbox.stop();
    mailbox.publish(makeFrame(9));

    honta::vision::DecodedFrame output;
    std::uint64_t cursor = 0;
    std::uint64_t skipped_frames = 0;
    const auto result = mailbox.waitNext(
        output,
        cursor,
        skipped_frames,
        std::chrono::milliseconds(10));

    if (result != honta::vision::LatestFrameWaitResult::Stopped) {
        std::cerr << "stopped mailbox accepted a late frame\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    return testSlowConsumerReceivesNewestFrame() &&
                   testConsumerDoesNotReceiveSameFrameTwice() &&
                   testInputErrorWakesConsumer() &&
                   testStoppedMailboxRejectsLateFrame()
               ? 0
               : 1;
}
