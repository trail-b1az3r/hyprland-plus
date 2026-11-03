#include <helpers/MotionBlur.hpp>

#include <gtest/gtest.h>

TEST(MotionBlur, skipsMovesTooSmallToSee) {
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {101, 100, 400, 300}, 5), 0);
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {100, 100, 400, 300}, 5), 0);
}

TEST(MotionBlur, slowMovesTakeFewerSamples) {
    // 4 px: the minimum, 2.
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {104, 100, 400, 300}, 5), 2);
    // 13 px: 4.
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {100, 113, 400, 300}, 5), 4);
    // Fast: capped at the configured samples.
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {180, 100, 400, 300}, 5), 5);
}

TEST(MotionBlur, aResizeCountsTheCornerThatTravelledFurthest) {
    // The left edge stays, the right edge moves 30 px.
    EXPECT_EQ(MotionBlur::samplesFor({100, 100, 400, 300}, {100, 100, 430, 300}, 8), 6);
}

TEST(MotionBlur, trackerSkipsTinyMoves) {
    MotionBlur::CTracker tracker;
    tracker.record({100, 100, 400, 300}, {100.5, 100, 400, 300});
    EXPECT_FALSE(tracker.state(5).has_value());
    tracker.record({100, 100, 400, 300}, {160, 100, 400, 300});
    ASSERT_TRUE(tracker.state(5).has_value());
    EXPECT_EQ(tracker.state(5)->samples, 5);
}
