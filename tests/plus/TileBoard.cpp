#include <plus/TileBoard.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace Plus;

namespace {
    // A 1920x1080 work area, with 10 px between tiles (two inner gaps of 5 px).
    constexpr SRect  BOUNDS = {0, 0, 1920, 1080};
    constexpr double TOL    = 12;

    STile            tile(uint64_t id, double x, double y, double w, double h) {
        STile t;
        t.id   = id;
        t.rect = {x, y, w, h};
        t.minW = 100;
        t.minH = 80;
        return t;
    }

    SRect rectOf(const std::vector<std::pair<uint64_t, SRect>>& rects, uint64_t id) {
        for (const auto& [i, r] : rects) {
            if (i == id)
                return r;
        }
        return {-1, -1, -1, -1};
    }

    bool has(const std::vector<uint64_t>& v, uint64_t id) {
        return std::ranges::find(v, id) != v.end();
    }
}

TEST(PlusTileBoard, edgeNamesRoundTrip) {
    for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM})
        EXPECT_EQ(edgeFromString(edgeName(E)), E);
    EXPECT_EQ(edgeFromString("up"), EDGE_TOP);
    EXPECT_EQ(edgeFromString("down"), EDGE_BOTTOM);
    EXPECT_FALSE(edgeFromString("middle").has_value());
    EXPECT_EQ(oppositeEdge(EDGE_LEFT), EDGE_RIGHT);
    EXPECT_TRUE(isVerticalEdge(EDGE_RIGHT));
    EXPECT_FALSE(isVerticalEdge(EDGE_TOP));
}

TEST(PlusTileBoard, neighboursAcrossTheGap) {
    // | 1 | 2 |
    const CTileBoard B({tile(1, 0, 0, 955, 1080), tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);

    const auto       RIGHT = B.neighbors(1, EDGE_RIGHT);
    ASSERT_EQ(RIGHT.size(), 1u);
    EXPECT_EQ(RIGHT[0].id, 2u);
    EXPECT_NEAR(RIGHT[0].shared, 1080, 0.01);

    EXPECT_TRUE(B.neighbors(1, EDGE_LEFT).empty());
    EXPECT_TRUE(B.adjustable(1, EDGE_RIGHT));
    EXPECT_FALSE(B.adjustable(1, EDGE_LEFT)); // the work area's edge
    EXPECT_FALSE(B.adjustable(1, EDGE_TOP));
}

TEST(PlusTileBoard, smallTileBesideTwoStackedTilesMovesBoth) {
    // | 1 | 2 |
    // |   | 3 |
    const CTileBoard B({tile(1, 0, 0, 600, 1080), tile(2, 610, 0, 1310, 535), tile(3, 610, 545, 1310, 535)}, BOUNDS, TOL);

    const auto       BOUNDARY = B.boundary(1, EDGE_RIGHT);
    ASSERT_TRUE(BOUNDARY.has_value());
    EXPECT_EQ(BOUNDARY->nearSide.size(), 1u);
    EXPECT_EQ(BOUNDARY->farSide.size(), 2u);
    EXPECT_TRUE(has(BOUNDARY->farSide, 2));
    EXPECT_TRUE(has(BOUNDARY->farSide, 3));

    const auto MOVE = B.proposeEdgeMove(1, EDGE_RIGHT, 100);
    ASSERT_TRUE(MOVE.ok);
    EXPECT_NEAR(MOVE.applied, 100, 0.01);
    EXPECT_NEAR(rectOf(MOVE.rects, 1).w, 700, 0.01);
    EXPECT_NEAR(rectOf(MOVE.rects, 2).x, 710, 0.01);
    EXPECT_NEAR(rectOf(MOVE.rects, 2).w, 1210, 0.01);
    EXPECT_NEAR(rectOf(MOVE.rects, 3).x, 710, 0.01);
    // heights are not touched by a right-edge move
    EXPECT_NEAR(rectOf(MOVE.rects, 2).h, 535, 0.01);
}

TEST(PlusTileBoard, alignedButSeparateSplitsAreNotMovedTogether) {
    // | 1 | 2 |   rows split first, then each row split at the same x:
    // | 3 | 4 |   moving 1's right edge moves the top split only
    const CTileBoard B({tile(1, 0, 0, 955, 535), tile(2, 965, 0, 955, 535), tile(3, 0, 545, 955, 535), tile(4, 965, 545, 955, 535)}, BOUNDS, TOL);

    const auto       BOUNDARY = B.boundary(1, EDGE_RIGHT);
    ASSERT_TRUE(BOUNDARY.has_value());
    EXPECT_EQ(BOUNDARY->nearSide, std::vector<uint64_t>{1});
    EXPECT_EQ(BOUNDARY->farSide, std::vector<uint64_t>{2});

    const auto MOVE = B.proposeEdgeMove(1, EDGE_RIGHT, -200);
    ASSERT_TRUE(MOVE.ok);
    EXPECT_EQ(MOVE.rects.size(), 2u);
}

TEST(PlusTileBoard, stackedTilesOnTheNearSideMoveTogether) {
    // | 1 | 3 |
    // | 2 |   |
    const CTileBoard B({tile(1, 0, 0, 955, 535), tile(2, 0, 545, 955, 535), tile(3, 965, 0, 955, 1080)}, BOUNDS, TOL);

    const auto       BOUNDARY = B.boundary(1, EDGE_RIGHT);
    ASSERT_TRUE(BOUNDARY.has_value());
    EXPECT_TRUE(has(BOUNDARY->nearSide, 1));
    EXPECT_TRUE(has(BOUNDARY->nearSide, 2));
    EXPECT_EQ(BOUNDARY->farSide, std::vector<uint64_t>{3});
}

TEST(PlusTileBoard, inwardMoveShrinksTheTileAndGrowsTheNeighbour) {
    CTileBoard B({tile(1, 0, 0, 955, 1080), tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);

    const auto MOVE = B.proposeEdgeMove(1, EDGE_RIGHT, -300);
    ASSERT_TRUE(MOVE.ok);
    EXPECT_NEAR(MOVE.applied, -300, 0.01);
    B.apply(MOVE);
    EXPECT_NEAR(B.tile(1)->rect.w, 655, 0.01);
    EXPECT_NEAR(B.tile(2)->rect.x, 665, 0.01);
    EXPECT_NEAR(B.tile(2)->rect.w, 1255, 0.01);
    EXPECT_TRUE(B.validate().empty());
}

TEST(PlusTileBoard, leftAndTopEdgesMoveTheRightWay) {
    CTileBoard B({tile(1, 0, 0, 1920, 535), tile(2, 0, 545, 1920, 535)}, BOUNDS, TOL);

    // 2's top edge, outwards (up) by 100: 2 grows upwards, 1 shrinks
    const auto MOVE = B.proposeEdgeMove(2, EDGE_TOP, 100);
    ASSERT_TRUE(MOVE.ok);
    B.apply(MOVE);
    EXPECT_NEAR(B.tile(2)->rect.y, 445, 0.01);
    EXPECT_NEAR(B.tile(2)->rect.h, 635, 0.01);
    EXPECT_NEAR(B.tile(1)->rect.h, 435, 0.01);
    EXPECT_TRUE(B.validate().empty());

    CTileBoard H({tile(1, 0, 0, 955, 1080), tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);
    const auto LEFT = H.proposeEdgeMove(2, EDGE_LEFT, 55);
    ASSERT_TRUE(LEFT.ok);
    H.apply(LEFT);
    EXPECT_NEAR(H.tile(2)->rect.x, 910, 0.01);
    EXPECT_NEAR(H.tile(1)->rect.w, 900, 0.01);
}

TEST(PlusTileBoard, movesStopAtMinimumSizes) {
    auto a = tile(1, 0, 0, 955, 1080);
    auto b = tile(2, 965, 0, 955, 1080);
    b.minW = 800;
    CTileBoard B({a, b}, BOUNDS, TOL);

    const auto MOVE = B.proposeEdgeMove(1, EDGE_RIGHT, 400);
    ASSERT_TRUE(MOVE.ok);
    EXPECT_NEAR(MOVE.applied, 155, 0.01);
    EXPECT_EQ(MOVE.limit, LIMIT_MIN_SIZE);
    ASSERT_TRUE(MOVE.limitedBy.has_value());
    EXPECT_EQ(*MOVE.limitedBy, 2u);
    EXPECT_NE(MOVE.describe().find("minimum width"), std::string::npos);

    B.apply(MOVE);
    const auto AGAIN = B.proposeEdgeMove(1, EDGE_RIGHT, 10);
    EXPECT_FALSE(AGAIN.ok);
    EXPECT_EQ(AGAIN.limit, LIMIT_MIN_SIZE);
    EXPECT_NE(AGAIN.describe().find("Cannot move"), std::string::npos);
}

TEST(PlusTileBoard, movesStopAtMaximumSizes) {
    auto a = tile(1, 0, 0, 955, 1080);
    a.maxW = 1000;
    const CTileBoard B({a, tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);

    const auto       MOVE = B.proposeEdgeMove(1, EDGE_RIGHT, 300);
    ASSERT_TRUE(MOVE.ok);
    EXPECT_NEAR(MOVE.applied, 45, 0.01);
    EXPECT_EQ(MOVE.limit, LIMIT_MAX_SIZE);
}

TEST(PlusTileBoard, edgesAtTheWorkAreaAreRefused) {
    const CTileBoard B({tile(1, 0, 0, 955, 1080), tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);

    for (const auto E : {EDGE_LEFT, EDGE_TOP, EDGE_BOTTOM}) {
        const auto MOVE = B.proposeEdgeMove(1, E, 50);
        EXPECT_FALSE(MOVE.ok);
        EXPECT_EQ(MOVE.limit, LIMIT_NO_NEIGHBOR);
        EXPECT_TRUE(MOVE.rects.empty());
    }
}

TEST(PlusTileBoard, unknownTilesAreRefused) {
    const CTileBoard B({tile(1, 0, 0, 1920, 1080)}, BOUNDS, TOL);
    const auto       MOVE = B.proposeEdgeMove(99, EDGE_RIGHT, 50);
    EXPECT_FALSE(MOVE.ok);
    EXPECT_EQ(MOVE.limit, LIMIT_UNKNOWN_TILE);
}

TEST(PlusTileBoard, restoreKeepsTheCustomSizeAndNeighboursFill) {
    // Four ordinary tiles and one Plus Tile (1), made smaller on its right and bottom edges, after the layout
    // recalculated and lost the custom size:
    // | 1 | 2     |
    // | 3 | 4 | 5 |
    auto p     = tile(1, 0, 0, 950, 530);
    p.modified = true;
    p.customW  = 600;
    p.customH  = 300;
    const CTileBoard B({p, tile(2, 960, 0, 960, 530), tile(3, 0, 540, 950, 540), tile(4, 960, 540, 470, 540), tile(5, 1440, 540, 480, 540)}, BOUNDS, TOL);

    const auto       PLAN = B.planRestore(1u);
    EXPECT_TRUE(PLAN.unsatisfied.empty());
    EXPECT_FALSE(PLAN.steps.empty());

    const SRect P = rectOf(PLAN.finalRects, 1);
    EXPECT_NEAR(P.w, 600, 0.5);
    EXPECT_NEAR(P.h, 300, 0.5);

    // The other tiles fill the space, and are not made equal to each other.
    const SRect R2 = rectOf(PLAN.finalRects, 2);
    EXPECT_NEAR(R2.x, 610, 0.5);
    EXPECT_NEAR(R2.right(), 1920, 0.5);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 4).w, 470, 0.5);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 5).w, 480, 0.5);

    // No overlaps, nothing outside the work area.
    std::vector<STile> after;
    for (const auto& [id, r] : PLAN.finalRects)
        after.push_back(tile(id, r.x, r.y, r.w, r.h));
    EXPECT_TRUE(CTileBoard(after, BOUNDS, TOL).validate().empty());
}

TEST(PlusTileBoard, conflictingCustomSizesKeepThePriorityTile) {
    // Two edge-modified tiles side by side. Together they want more than the screen, so one must give way.
    auto a     = tile(1, 0, 0, 955, 1080);
    a.modified = true;
    a.customW  = 1400;
    auto b     = tile(2, 965, 0, 955, 1080);
    b.modified = true;
    b.customW  = 1400;
    const CTileBoard B({a, b}, BOUNDS, TOL);

    const auto       PLAN = B.planRestore(2u, 8);
    EXPECT_LE(PLAN.passes, 8);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 2).w, 1400, 0.5);
    ASSERT_EQ(PLAN.unsatisfied.size(), 1u);
    EXPECT_EQ(PLAN.unsatisfied[0].id, 1u);
    EXPECT_FALSE(PLAN.unsatisfied[0].reason.empty());
}

TEST(PlusTileBoard, restoreIsBoundedWhenMinimumsMakeItImpossible) {
    auto a     = tile(1, 0, 0, 955, 1080);
    a.modified = true;
    a.customW  = 1900; // more than the neighbour's minimum allows
    const CTileBoard B({a, tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);

    const auto       PLAN = B.planRestore(std::nullopt, 8);
    EXPECT_LE(PLAN.passes, 8);
    ASSERT_EQ(PLAN.unsatisfied.size(), 1u);
    EXPECT_NE(PLAN.unsatisfied[0].reason.find("minimum width"), std::string::npos);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 2).w, 100, 0.5);
}

TEST(PlusTileBoard, lockedTilesKeepTheirSize) {
    // 2 is locked. Restoring 1's custom width takes space from 3 rather than from 2.
    // | 1 | 2 | 3 |
    auto a     = tile(1, 0, 0, 630, 1080);
    a.modified = true;
    a.customW  = 400;
    auto b     = tile(2, 640, 0, 630, 1080);
    b.locked   = true;
    const CTileBoard B({a, b, tile(3, 1280, 0, 640, 1080)}, BOUNDS, TOL);

    const auto       PLAN = B.planRestore(1u);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 1).w, 400, 0.5);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 2).w, 630, 0.5);
    EXPECT_NEAR(rectOf(PLAN.finalRects, 3).w, 870, 0.5);
}

TEST(PlusTileBoard, nothingToRestoreForOrdinaryTiles) {
    const CTileBoard B({tile(1, 0, 0, 955, 1080), tile(2, 965, 0, 955, 1080)}, BOUNDS, TOL);
    const auto       PLAN = B.planRestore();
    EXPECT_TRUE(PLAN.steps.empty());
    EXPECT_TRUE(PLAN.unsatisfied.empty());
}

TEST(PlusTileBoard, navigationUsesRealGeometry) {
    // | 1 | 2 |
    // |   | 3 |   2 is shorter than 3, so going right from 1 picks 3, which shares more of 1's edge.
    const CTileBoard B({tile(1, 0, 0, 600, 1080), tile(2, 610, 0, 1310, 400), tile(3, 610, 410, 1310, 670)}, BOUNDS, TOL);

    EXPECT_EQ(B.neighborInDirection(1, EDGE_RIGHT), std::optional<uint64_t>{3});
    EXPECT_EQ(B.neighborInDirection(2, EDGE_LEFT), std::optional<uint64_t>{1});
    EXPECT_EQ(B.neighborInDirection(3, EDGE_LEFT), std::optional<uint64_t>{1});
    EXPECT_EQ(B.neighborInDirection(2, EDGE_BOTTOM), std::optional<uint64_t>{3});
    EXPECT_EQ(B.neighborInDirection(3, EDGE_TOP), std::optional<uint64_t>{2});

    // Nothing beyond the work area.
    EXPECT_FALSE(B.neighborInDirection(1, EDGE_LEFT).has_value());
    EXPECT_FALSE(B.neighborInDirection(2, EDGE_TOP).has_value());
}

TEST(PlusTileBoard, navigationPrefersBesideOverDiagonal) {
    // | 1 |   |
    // |   | 2 |   2 is below and to the right of 1, 3 is directly right of 1 but further away.
    // | 3 is far right, sharing 1's rows
    const CTileBoard B({tile(1, 0, 0, 400, 500), tile(2, 410, 510, 400, 570), tile(3, 1000, 0, 920, 500)}, BOUNDS, TOL);
    EXPECT_EQ(B.neighborInDirection(1, EDGE_RIGHT), std::optional<uint64_t>{3});
}

TEST(PlusTileBoard, validateFindsOverlaps) {
    const CTileBoard B({tile(1, 0, 0, 1000, 1080), tile(2, 900, 0, 1020, 1080)}, BOUNDS, TOL);
    const auto       ISSUES = B.validate();
    ASSERT_EQ(ISSUES.size(), 1u);
    EXPECT_NE(ISSUES[0].find("overlap"), std::string::npos);
}
