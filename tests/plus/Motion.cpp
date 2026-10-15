#include <plus/Motion.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include <hyprutils/memory/Casts.hpp>

using namespace Plus;
using Hyprutils::Memory::sc;

// Feeds a back-and-forth along x: `strokes` strokes of `span` pixels, each taking `strokeMs`, in steps of 8 ms.
static bool shake(CShakeDetector& d, int strokes, double span, double strokeMs, double startMs = 0, double y = 500) {
    bool   found = false;
    double x     = 500;
    double ms    = startMs;
    for (int s = 0; s < strokes; ++s) {
        const double DIR   = s % 2 == 0 ? 1 : -1;
        const int    STEPS = std::max(1, sc<int>(strokeMs / 8));
        for (int i = 0; i < STEPS; ++i) {
            x += DIR * span / STEPS;
            ms += 8;
            found = d.feed(x, y, ms) || found;
        }
    }
    return found;
}

TEST(PlusMotion, aQuickShakeIsFound) {
    CShakeDetector d;
    EXPECT_TRUE(shake(d, 6, 160, 64));
}

TEST(PlusMotion, slowBackAndForthIsNotAShake) {
    CShakeDetector d;
    // The same strokes, each taking 400 ms: never four turns inside the window.
    EXPECT_FALSE(shake(d, 6, 160, 400));
}

TEST(PlusMotion, aLongSweepIsNotAShake) {
    CShakeDetector d;
    // One long move one way: plenty of travel, no turns, and it ends far from where it started.
    double ms    = 0;
    bool   found = false;
    for (int i = 0; i < 60; ++i) {
        ms += 8;
        found = d.feed(100 + i * 30, 500, ms) || found;
    }
    EXPECT_FALSE(found);
}

TEST(PlusMotion, jitterIsNotAShake) {
    CShakeDetector d;
    // A hand resting on the mouse: tiny moves either way, too short to count as strokes.
    double ms    = 0;
    bool   found = false;
    for (int i = 0; i < 200; ++i) {
        ms += 4;
        found = d.feed(500 + (i % 2 == 0 ? 3 : -3), 500, ms) || found;
    }
    EXPECT_FALSE(found);
}

TEST(PlusMotion, onlyOncePerCooldown) {
    CShakeDetector d;
    EXPECT_TRUE(shake(d, 6, 160, 64, 0));
    // Straight on: still inside the cooldown.
    EXPECT_FALSE(shake(d, 6, 160, 64, 400));
    // Later: found again.
    EXPECT_TRUE(shake(d, 6, 160, 64, 3000));
}

TEST(PlusMotion, verticalShakeCounts) {
    CShakeDetector d;
    double         ms    = 0;
    double         y     = 500;
    bool           found = false;
    for (int s = 0; s < 6; ++s) {
        for (int i = 0; i < 8; ++i) {
            y += (s % 2 == 0 ? 1 : -1) * 20;
            ms += 8;
            found = d.feed(500, y, ms) || found;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PlusMotion, peekFadesBothWays) {
    EXPECT_NEAR(peekOpacity(0, 200, true, 0.1), 1.0, 1e-9);
    EXPECT_NEAR(peekOpacity(200, 200, true, 0.1), 0.1, 1e-9);
    EXPECT_NEAR(peekOpacity(1000, 200, true, 0.1), 0.1, 1e-9);
    EXPECT_NEAR(peekOpacity(0, 200, false, 0.1), 0.1, 1e-9);
    EXPECT_NEAR(peekOpacity(200, 200, false, 0.1), 1.0, 1e-9);

    // Eased: most of the fade is done by halfway.
    const double HALF = peekOpacity(100, 200, true, 0.0);
    EXPECT_LT(HALF, 0.25);
    EXPECT_GT(HALF, 0.0);

    // No duration: straight to the end.
    EXPECT_NEAR(peekOpacity(0, 0, true, 0.2), 0.2, 1e-9);
    // Out of range targets are clamped.
    EXPECT_NEAR(peekOpacity(500, 200, true, -3), 0.0, 1e-9);
}

TEST(PlusMotion, ringClosesInThenFades) {
    const auto START = locateRing(0, 900, 30);
    EXPECT_NEAR(START.radius, 90, 1e-6);
    EXPECT_NEAR(START.alpha, 1, 1e-9);

    const auto CLOSED = locateRing(600, 900, 30);
    EXPECT_NEAR(CLOSED.radius, 30, 1e-6);
    EXPECT_NEAR(CLOSED.alpha, 1, 1e-9);

    const auto FADING = locateRing(750, 900, 30);
    EXPECT_NEAR(FADING.radius, 30, 1e-6);
    EXPECT_NEAR(FADING.alpha, 0.5, 1e-6);

    EXPECT_EQ(locateRing(900, 900, 30).alpha, 0);
    EXPECT_EQ(locateRing(-1, 900, 30).alpha, 0);
}

TEST(PlusMotion, miniRowLinesUpAlongTheBottom) {
    const SRect AREA{.x = 0, .y = 30, .w = 500, .h = 1000};
    const auto  B = miniRow(AREA, 96, 54, 16, 6);
    ASSERT_EQ(B.size(), 6u);

    // Four fit across 500 px (16 + 4 * 112 = 464), so two wrap upwards.
    EXPECT_NEAR(B[0].x, 16, 1e-9);
    EXPECT_NEAR(B[0].y, 1030 - 16 - 54, 1e-9);
    EXPECT_NEAR(B[1].x, 16 + 112, 1e-9);
    EXPECT_NEAR(B[3].x, 16 + 3 * 112, 1e-9);
    EXPECT_NEAR(B[4].x, 16, 1e-9);
    EXPECT_NEAR(B[4].y, B[0].y - 70, 1e-9);

    for (const auto& b : B) {
        EXPECT_GE(b.x, AREA.x);
        EXPECT_LE(b.right(), AREA.right());
        EXPECT_GE(b.y, AREA.y);
        EXPECT_LE(b.bottom(), AREA.bottom());
    }
}

TEST(PlusMotion, miniRowOnATinyScreen) {
    // Narrower than one mini window: one per row, never above the area.
    const auto B = miniRow({.x = 0, .y = 0, .w = 50, .h = 100}, 96, 54, 16, 3);
    ASSERT_EQ(B.size(), 3u);
    for (const auto& b : B)
        EXPECT_GE(b.y, 0);
    EXPECT_TRUE(miniRow({.x = 0, .y = 0, .w = 500, .h = 500}, 96, 54, 16, 0).empty());
}
