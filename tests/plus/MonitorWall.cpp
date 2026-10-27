#include <plus/MonitorWall.hpp>

#include <gtest/gtest.h>

using namespace Plus;

// Two 1920x1080 monitors side by side, and a third under the right one.
static const std::vector<SRect> MONITORS = {{0, 0, 1920, 1080}, {1920, 0, 1920, 1080}, {1920, 1080, 1920, 1080}};

TEST(PlusMonitorWall, heldAtTheEdgeAndBulgesOnTheOtherSide) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1000, 200, 800, 600});
    const auto S = wall.step({1180, 210, 800, 600}, MONITORS, 120);
    EXPECT_DOUBLE_EQ(S.x, 1120);
    EXPECT_DOUBLE_EQ(S.y, 210);
    ASSERT_TRUE(S.push);
    EXPECT_EQ(S.push->monitor, 1u);
    EXPECT_EQ(S.push->edge, EDGE_LEFT);
    EXPECT_DOUBLE_EQ(S.push->start, 210);
    EXPECT_DOUBLE_EQ(S.push->length, 600);
    EXPECT_NEAR(S.push->strength, 60.0 / 120.0, 1e-9);
    EXPECT_TRUE(wall.holding());
}

TEST(PlusMonitorWall, givesWayPastItsStrengthAndStaysOpen) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1000, 200, 800, 600});
    const auto THROUGH = wall.step({1260, 200, 800, 600}, MONITORS, 120);
    EXPECT_TRUE(THROUGH.brokeThrough);
    EXPECT_DOUBLE_EQ(THROUGH.x, 1260);
    EXPECT_FALSE(THROUGH.push);
    // Moving on, a little back, it isn't stopped again.
    const auto ON = wall.step({1200, 200, 800, 600}, MONITORS, 120);
    EXPECT_DOUBLE_EQ(ON.x, 1200);
    EXPECT_FALSE(ON.push);
}

TEST(PlusMonitorWall, standsAgainWhenPulledBack) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1000, 200, 800, 600});
    wall.step({1300, 200, 800, 600}, MONITORS, 120);
    wall.step({900, 200, 800, 600}, MONITORS, 120);
    EXPECT_TRUE(wall.step({1150, 200, 800, 600}, MONITORS, 120).push);
}

TEST(PlusMonitorWall, noWallWhereNoMonitorIs) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {100, 200, 800, 600});
    const auto S = wall.step({-50, 200, 800, 600}, MONITORS, 120);
    EXPECT_DOUBLE_EQ(S.x, -50);
    EXPECT_FALSE(S.push);
}

TEST(PlusMonitorWall, theNewMonitorBecomesHome) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1000, 200, 800, 600});
    wall.step({1300, 200, 800, 600}, MONITORS, 120); // through
    wall.step({2200, 200, 800, 600}, MONITORS, 120); // on the right monitor now
    const auto DOWN = wall.step({2200, 540, 800, 600}, MONITORS, 120);
    EXPECT_DOUBLE_EQ(DOWN.y, 480);
    ASSERT_TRUE(DOWN.push);
    EXPECT_EQ(DOWN.push->monitor, 2u);
    EXPECT_EQ(DOWN.push->edge, EDGE_TOP);
    EXPECT_DOUBLE_EQ(DOWN.push->start, 280);
}

TEST(PlusMonitorWall, aWindowAlreadyAcrossIsLeftAlone) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1500, 200, 800, 600});
    EXPECT_FALSE(wall.step({1510, 200, 800, 600}, MONITORS, 120).push);
}

TEST(PlusMonitorWall, offMeansOff) {
    CMonitorWall wall;
    wall.begin(MONITORS[0], {1000, 200, 800, 600});
    EXPECT_FALSE(wall.step({1180, 200, 800, 600}, MONITORS, 0).push);
}
