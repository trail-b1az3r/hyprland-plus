#include "MotionBlur.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace MotionBlur;

CBox MotionBlur::extents(const CBox& previous, const CBox& current) {
    const double x1 = std::min(previous.x, current.x);
    const double y1 = std::min(previous.y, current.y);
    const double x2 = std::max(previous.x + previous.w, current.x + current.w);
    const double y2 = std::max(previous.y + previous.h, current.y + current.h);

    return {x1, y1, x2 - x1, y2 - y1};
}

CBox SState::extents() const {
    return MotionBlur::extents(previous, current);
}

int MotionBlur::samplesFor(const CBox& previous, const CBox& current, int maxSamples) {
    const double TRAVEL = std::max({std::abs(current.x - previous.x), std::abs(current.y - previous.y), std::abs((current.x + current.w) - (previous.x + previous.w)),
                                    std::abs((current.y + current.h) - (previous.y + previous.h))});
    if (TRAVEL < 1.5 || maxSamples < 2)
        return 0;
    return std::clamp(static_cast<int>(std::ceil(TRAVEL / 6.0)) + 1, 2, maxSamples);
}

void CTracker::record(const CBox& previous, const CBox& current) {
    m_previous  = previous;
    m_current   = current;
    m_updatedAt = Time::steadyNow();
    m_valid     = true;
}

void CTracker::reset() {
    m_previous = {};
    m_current  = {};
    m_valid    = false;
}

std::optional<SState> CTracker::state(int samples, const Vector2D& offset, bool allowStale) const {
    if (!m_valid)
        return std::nullopt;

    const float AGE = std::chrono::duration<float, std::milli>(Time::steadyNow() - m_updatedAt).count();
    if (!allowStale && AGE > 100.F)
        return std::nullopt;

    CBox previous = m_previous.copy().translate(offset);
    CBox current  = m_current.copy().translate(offset);

    if (previous == current || previous.w <= 0.F || previous.h <= 0.F || current.w <= 0.F || current.h <= 0.F)
        return std::nullopt;

    const int NEEDED = samplesFor(previous, current, samples);
    if (NEEDED < 2)
        return std::nullopt;

    return SState{
        .previous = previous,
        .current  = current,
        .samples  = NEEDED,
    };
}
