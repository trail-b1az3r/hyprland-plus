#include <plus/EditSession.hpp>

#include <gtest/gtest.h>

using namespace Plus;

TEST(PlusEditSession, beginSelectNudgeCommit) {
    CEditSession s;
    EXPECT_FALSE(s.active());
    ASSERT_TRUE(s.begin(7, 1));
    EXPECT_TRUE(s.active());
    EXPECT_EQ(s.tile(), 7u);
    EXPECT_EQ(s.edge(), EDGE_RIGHT);
    EXPECT_FALSE(s.hasPending());

    s.nudge(40);
    s.nudge(40);
    s.selectEdge(EDGE_BOTTOM);
    s.nudge(-10);
    EXPECT_TRUE(s.hasPending());
    EXPECT_NEAR(s.pending(EDGE_RIGHT), 80, 0.01);
    EXPECT_NEAR(s.pending(EDGE_BOTTOM), -10, 0.01);

    const auto P = s.commit();
    EXPECT_NEAR(P[EDGE_RIGHT], 80, 0.01);
    EXPECT_NEAR(P[EDGE_BOTTOM], -10, 0.01);
    EXPECT_NEAR(P[EDGE_LEFT], 0, 0.01);
    EXPECT_FALSE(s.active());
    EXPECT_FALSE(s.hasPending());
}

TEST(PlusEditSession, aSecondTileCannotTakeOverASession) {
    CEditSession s;
    ASSERT_TRUE(s.begin(7, 1));
    EXPECT_FALSE(s.begin(8, 1));
    EXPECT_EQ(s.tile(), 7u);
    // Beginning again on the same tile keeps its pending moves.
    s.nudge(20);
    EXPECT_TRUE(s.begin(7, 1, EDGE_LEFT));
    EXPECT_NEAR(s.pending(EDGE_RIGHT), 20, 0.01);
    EXPECT_EQ(s.edge(), EDGE_LEFT);
}

TEST(PlusEditSession, focusElsewhereEndsTheSession) {
    CEditSession s;
    s.begin(7, 1);
    s.nudge(40);
    EXPECT_FALSE(s.onEvent(EDIT_EVENT_FOCUS_CHANGED, 7)); // focus back on the same tile
    EXPECT_TRUE(s.active());
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_FOCUS_CHANGED, 9));
    EXPECT_FALSE(s.active());
    EXPECT_FALSE(s.hasPending());
}

TEST(PlusEditSession, workspaceChangesEndTheSession) {
    CEditSession s;
    s.begin(7, 1);
    EXPECT_FALSE(s.onEvent(EDIT_EVENT_WORKSPACE_CHANGED, 1));
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_WORKSPACE_CHANGED, 2));
    EXPECT_FALSE(s.active());
}

TEST(PlusEditSession, onlyTheEditedWindowClosingEndsIt) {
    CEditSession s;
    s.begin(7, 1);
    EXPECT_FALSE(s.onEvent(EDIT_EVENT_WINDOW_CLOSED, 9));
    EXPECT_TRUE(s.active());
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_WINDOW_CLOSED, 7));
    EXPECT_FALSE(s.active());

    s.begin(7, 1);
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_WINDOW_FLOATED, 7));
    s.begin(7, 1);
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_FULLSCREEN, 7));
    s.begin(7, 1);
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_SUBMAP_LEFT));
    s.begin(7, 1);
    EXPECT_TRUE(s.onEvent(EDIT_EVENT_PLUS_DISABLED));
}

TEST(PlusEditSession, eventsDoNothingWithoutASession) {
    CEditSession s;
    EXPECT_FALSE(s.onEvent(EDIT_EVENT_FOCUS_CHANGED, 3));
    s.nudge(50);
    s.selectEdge(EDGE_TOP);
    EXPECT_FALSE(s.hasPending());
    EXPECT_EQ(s.edge(), EDGE_RIGHT);
}

TEST(PlusEditSession, setPendingClampsAMove) {
    CEditSession s;
    s.begin(7, 1);
    s.nudge(500);
    s.setPending(EDGE_RIGHT, 155);
    EXPECT_NEAR(s.pending(EDGE_RIGHT), 155, 0.01);
}

TEST(PlusHighlightGeometry, renderBoxUsesTheMonitorOffsetAndScale) {
    // A tile at (2000, 100) on a monitor placed at (1920, 0) with scale 1.5
    const auto R = toRenderBox({2000, 100, 400, 300}, 1920, 0, 1.5);
    EXPECT_NEAR(R.x, 120, 0.01);
    EXPECT_NEAR(R.y, 150, 0.01);
    EXPECT_NEAR(R.w, 600, 0.01);
    EXPECT_NEAR(R.h, 450, 0.01);
}

TEST(PlusHighlightGeometry, renderBoxFollowsTheWorkspaceAnimation) {
    const auto R = toRenderBox({100, 100, 200, 200}, 0, 0, 1.0, -50, 25);
    EXPECT_NEAR(R.x, 50, 0.01);
    EXPECT_NEAR(R.y, 125, 0.01);
}

TEST(PlusHighlightGeometry, neighbouringBoxesStillMeetAfterRounding) {
    // Two tiles that touch at x = 333.4 on a 1.25 scale monitor must still touch in pixels.
    const auto A = toRenderBox({0, 0, 333.4, 100}, 0, 0, 1.25);
    const auto B = toRenderBox({333.4, 0, 333.4, 100}, 0, 0, 1.25);
    EXPECT_NEAR(A.right(), B.x, 0.01);
}

TEST(PlusHighlightGeometry, outlineAndEdgeBars) {
    const SRect R{10, 20, 100, 50};
    const auto  O = outline(R, 4);
    EXPECT_NEAR(O[0].h, 4, 0.01);   // top
    EXPECT_NEAR(O[1].y, 66, 0.01);  // bottom: 20 + 50 - 4
    EXPECT_NEAR(O[2].h, 42, 0.01);  // left, between top and bottom
    EXPECT_NEAR(O[3].x, 106, 0.01); // right: 10 + 100 - 4

    EXPECT_NEAR(edgeBar(R, EDGE_RIGHT, 6).x, 104, 0.01);
    EXPECT_NEAR(edgeBar(R, EDGE_BOTTOM, 6).y, 64, 0.01);
    EXPECT_NEAR(edgeBar(R, EDGE_LEFT, 6).w, 6, 0.01);
    EXPECT_NEAR(edgeBar(R, EDGE_TOP, 6).w, 100, 0.01);

    // A thickness larger than the box is clamped.
    EXPECT_NEAR(outline({0, 0, 6, 6}, 10)[0].h, 3, 0.01);
}

TEST(PlusHighlightGeometry, flashesHoldThenFade) {
    EXPECT_NEAR(flashAlpha(0, 1000), 1.0, 0.001);
    EXPECT_NEAR(flashAlpha(600, 1000), 1.0, 0.001);
    EXPECT_NEAR(flashAlpha(800, 1000), 0.5, 0.001);
    EXPECT_NEAR(flashAlpha(1000, 1000), 0.0, 0.001);
    EXPECT_NEAR(flashAlpha(5000, 1000), 0.0, 0.001);
}
