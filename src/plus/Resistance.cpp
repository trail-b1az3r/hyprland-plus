#include "Resistance.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

using namespace Plus;

static constexpr double EPSILON = 0.5;

// An edge to stop at: a line at `at`, spanning [from, to] along the other axis.
struct SLine {
    double at = 0, from = 0, to = 0;
};

static bool spans(const SLine& line, double from, double to) {
    return from < line.to && to > line.from;
}

// Whether a monitor edge is shared with another monitor, which the monitor wall handles.
static bool sharedVertical(double x, const SRect& self, const std::vector<SRect>& monitors) {
    return std::ranges::any_of(
        monitors, [&](const SRect& m) { return &m != &self && (std::abs(m.x - x) < EPSILON || std::abs(m.right() - x) < EPSILON) && m.y < self.bottom() && m.bottom() > self.y; });
}

static bool sharedHorizontal(double y, const SRect& self, const std::vector<SRect>& monitors) {
    return std::ranges::any_of(
        monitors, [&](const SRect& m) { return &m != &self && (std::abs(m.y - y) < EPSILON || std::abs(m.bottom() - y) < EPSILON) && m.x < self.right() && m.right() > self.x; });
}

// One axis. Moving toward higher values, the leading edge (previous end, wanted end) stops at `ahead` lines; toward
// lower values, the leading edge (previous start, wanted start) stops at `behind` lines. Returns the held start.
static std::optional<double> axis(double prevStart, double wantStart, double size, double from, double to, const std::vector<SLine>& ahead, const std::vector<SLine>& behind,
                                  double resistance) {
    std::optional<double> held;
    if (wantStart > prevStart) {
        const double PREVEND = prevStart + size, WANTEND = wantStart + size;
        for (const auto& line : ahead) {
            if (!spans(line, from, to) || PREVEND > line.at + EPSILON || WANTEND <= line.at || WANTEND - line.at >= resistance)
                continue;
            if (!held || line.at - size < *held)
                held = line.at - size;
        }
    } else if (wantStart < prevStart) {
        for (const auto& line : behind) {
            if (!spans(line, from, to) || prevStart < line.at - EPSILON || wantStart >= line.at || line.at - wantStart >= resistance)
                continue;
            if (!held || line.at > *held)
                held = line.at;
        }
    }
    return held;
}

SResisted Plus::resist(const SRect& previous, const SRect& wanted, const std::vector<SRect>& windows, const std::vector<SRect>& monitors, double resistance) {
    SResisted result = {.x = wanted.x, .y = wanted.y};
    if (resistance <= 0)
        return result;

    // Right/bottom edges stop at lines "ahead": a screen's far edge, another window's near edge. Left/top edges stop at
    // lines "behind": a screen's near edge, another window's far edge.
    std::vector<SLine> right, left, down, up;
    for (const auto& m : monitors) {
        if (!sharedVertical(m.right(), m, monitors))
            right.push_back({m.right(), m.y, m.bottom()});
        if (!sharedVertical(m.x, m, monitors))
            left.push_back({m.x, m.y, m.bottom()});
        if (!sharedHorizontal(m.bottom(), m, monitors))
            down.push_back({m.bottom(), m.x, m.right()});
        if (!sharedHorizontal(m.y, m, monitors))
            up.push_back({m.y, m.x, m.right()});
    }
    for (const auto& w : windows) {
        right.push_back({w.x, w.y, w.bottom()});
        left.push_back({w.right(), w.y, w.bottom()});
        down.push_back({w.y, w.x, w.right()});
        up.push_back({w.bottom(), w.x, w.right()});
    }

    if (const auto X = axis(previous.x, wanted.x, wanted.w, wanted.y, wanted.bottom(), right, left, resistance)) {
        result.x    = *X;
        result.held = true;
    }
    if (const auto Y = axis(previous.y, wanted.y, wanted.h, result.x, result.x + wanted.w, down, up, resistance)) {
        result.y    = *Y;
        result.held = true;
    }
    return result;
}
