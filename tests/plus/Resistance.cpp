#include <plus/Resistance.hpp>

#include <gtest/gtest.h>

using namespace Plus;

static const std::vector<SRect> ONE_SCREEN = {{0, 0, 1920, 1080}};

TEST(PlusResistance, stopsAtTheScreenEdgeUntilPushedPast) {
    // The right edge reaches the screen's right edge: held there for the first 40 px of push.
    const auto HELD = resist({1000, 100, 800, 600}, {1150, 100, 800, 600}, {}, ONE_SCREEN, 40);
    EXPECT_TRUE(HELD.held);
    EXPECT_DOUBLE_EQ(HELD.x, 1120);
    // From the edge, pushed 50 px past it: through.
    const auto THROUGH = resist({1120, 100, 800, 600}, {1170, 100, 800, 600}, {}, ONE_SCREEN, 40);
    EXPECT_FALSE(THROUGH.held);
    EXPECT_DOUBLE_EQ(THROUGH.x, 1170);
    // Already past it, it moves freely, either way.
    EXPECT_FALSE(resist({1170, 100, 800, 600}, {1160, 100, 800, 600}, {}, ONE_SCREEN, 40).held);
}

TEST(PlusResistance, stopsAgainstAnotherWindowOnlyWhereTheyMeet) {
    const std::vector<SRect> OTHER = {{900, 200, 400, 400}};
    // Moving right into its left edge, overlapping it vertically: held touching it.
    const auto HELD = resist({300, 300, 500, 200}, {420, 300, 500, 200}, OTHER, ONE_SCREEN, 40);
    EXPECT_TRUE(HELD.held);
    EXPECT_DOUBLE_EQ(HELD.x, 400);
    // Same move, passing below it: nothing in the way.
    EXPECT_FALSE(resist({300, 700, 500, 200}, {420, 700, 500, 200}, OTHER, ONE_SCREEN, 40).held);
    // Moving up onto its bottom edge.
    const auto UP = resist({950, 650, 200, 200}, {950, 590, 200, 200}, OTHER, ONE_SCREEN, 40);
    EXPECT_TRUE(UP.held);
    EXPECT_DOUBLE_EQ(UP.y, 600);
}

TEST(PlusResistance, leavesSharedMonitorEdgesToTheWall) {
    const std::vector<SRect> TWO = {{0, 0, 1920, 1080}, {1920, 0, 1920, 1080}};
    EXPECT_FALSE(resist({1000, 100, 800, 600}, {1150, 100, 800, 600}, {}, TWO, 40).held);
    // The far edge of the second screen still resists.
    EXPECT_TRUE(resist({2900, 100, 800, 600}, {3060, 100, 800, 600}, {}, TWO, 40).held);
}

TEST(PlusResistance, zeroIsOff) {
    const auto R = resist({1000, 100, 800, 600}, {1150, 100, 800, 600}, {}, ONE_SCREEN, 0);
    EXPECT_FALSE(R.held);
    EXPECT_DOUBLE_EQ(R.x, 1150);
}
