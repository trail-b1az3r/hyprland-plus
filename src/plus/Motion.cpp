#include "Motion.hpp"

#include <algorithm>
#include <cmath>

#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

using namespace Plus;

CShakeDetector::CShakeDetector(SShakeConfig config) : m_config(config) {
    ;
}

void CShakeDetector::reset() {
    m_samples.clear();
}

int CShakeDetector::reversalsAlong(bool horizontal) const {
    // A stroke counts once it has gone minStroke one way; a reversal is a counted stroke the other way from the last.
    int    reversals = 0;
    int    counted   = 0; // direction of the last counted stroke: -1, 1, or 0 before the first
    int    runSign   = 0;
    double run       = 0;
    for (size_t i = 1; i < m_samples.size(); ++i) {
        const double D = horizontal ? m_samples[i].x - m_samples[i - 1].x : m_samples[i].y - m_samples[i - 1].y;
        if (D == 0)
            continue;

        const int SIGN = D > 0 ? 1 : -1;
        if (SIGN == runSign)
            run += std::abs(D);
        else {
            runSign = SIGN;
            run     = std::abs(D);
        }

        if (run < m_config.minStroke || runSign == counted)
            continue;

        if (counted != 0)
            ++reversals;
        counted = runSign;
    }
    return reversals;
}

bool CShakeDetector::feed(double x, double y, double ms) {
    if (!m_samples.empty() && ms < m_samples.back().ms)
        m_samples.clear(); // the clock went backwards: start again

    m_samples.push_back({x, y, ms});
    while (!m_samples.empty() && ms - m_samples.front().ms > m_config.windowMs)
        m_samples.pop_front();

    if (ms < m_quietUntil || m_samples.size() < 4)
        return false;

    double travel = 0;
    for (size_t i = 1; i < m_samples.size(); ++i)
        travel += std::hypot(m_samples[i].x - m_samples[i - 1].x, m_samples[i].y - m_samples[i - 1].y);
    if (travel < m_config.minTravel)
        return false;

    const double DRIFT = std::hypot(m_samples.back().x - m_samples.front().x, m_samples.back().y - m_samples.front().y);
    if (DRIFT > travel * m_config.maxDrift)
        return false;

    if (std::max(reversalsAlong(true), reversalsAlong(false)) < m_config.reversals)
        return false;

    m_quietUntil = ms + m_config.cooldownMs;
    m_samples.clear();
    return true;
}

static double easeOutCubic(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - t, 3.0);
}

double Plus::peekOpacity(double elapsedMs, double durationMs, bool entering, double target) {
    target         = std::clamp(target, 0.0, 1.0);
    const double T = durationMs <= 0 ? 1.0 : easeOutCubic(elapsedMs / durationMs);
    return entering ? 1.0 + (target - 1.0) * T : target + (1.0 - target) * T;
}

SRingFrame Plus::locateRing(double elapsedMs, double durationMs, double radius) {
    if (durationMs <= 0 || elapsedMs < 0 || elapsedMs >= durationMs)
        return {.radius = radius, .alpha = 0};

    const double T    = elapsedMs / durationMs;
    const double FADE = 2.0 / 3.0;
    return {
        .radius = radius * (3.0 - 2.0 * easeOutCubic(T / FADE)),
        .alpha  = T <= FADE ? 1.0 : 1.0 - (T - FADE) / (1.0 - FADE),
    };
}

std::vector<SRect> Plus::miniRow(const SRect& area, double w, double h, double gap, size_t count) {
    std::vector<SRect> boxes;
    if (count == 0 || w <= 0 || h <= 0)
        return boxes;

    const size_t PERROW = std::max<size_t>(1, sc<size_t>(std::max(0.0, std::floor((area.w - gap) / (w + gap)))));
    boxes.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t ROW = i / PERROW;
        const size_t COL = i % PERROW;
        const double X   = area.x + gap + sc<double>(COL) * (w + gap);
        const double Y   = std::max(area.y, area.bottom() - gap - h - sc<double>(ROW) * (h + gap));
        boxes.push_back({.x = X, .y = Y, .w = w, .h = h});
    }
    return boxes;
}
