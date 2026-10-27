#include "MonitorWall.hpp"

#include <algorithm>
#include <cmath>

using namespace Plus;

static bool contains(const SRect& outer, const SRect& inner) {
    constexpr double SLACK = 1.0;
    return inner.x >= outer.x - SLACK && inner.y >= outer.y - SLACK && inner.right() <= outer.right() + SLACK && inner.bottom() <= outer.bottom() + SLACK;
}

static bool sameRect(const SRect& a, const SRect& b) {
    return std::abs(a.x - b.x) < 0.5 && std::abs(a.y - b.y) < 0.5 && std::abs(a.w - b.w) < 0.5 && std::abs(a.h - b.h) < 0.5;
}

static double overlap(double a0, double a1, double b0, double b1) {
    return std::min(a1, b1) - std::max(a0, b0);
}

// The monitor sharing `home`'s edge `edge` where the window's side is, if any.
static std::optional<size_t> neighbour(const SRect& home, eEdge edge, const SRect& window, const std::vector<SRect>& monitors) {
    constexpr double TOUCH = 2.0;
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        if (sameRect(m, home))
            continue;
        bool touches = false;
        switch (edge) {
            case EDGE_RIGHT: touches = std::abs(m.x - home.right()) <= TOUCH && overlap(m.y, m.bottom(), window.y, window.bottom()) > 0; break;
            case EDGE_LEFT: touches = std::abs(m.right() - home.x) <= TOUCH && overlap(m.y, m.bottom(), window.y, window.bottom()) > 0; break;
            case EDGE_BOTTOM: touches = std::abs(m.y - home.bottom()) <= TOUCH && overlap(m.x, m.right(), window.x, window.right()) > 0; break;
            case EDGE_TOP: touches = std::abs(m.bottom() - home.y) <= TOUCH && overlap(m.x, m.right(), window.x, window.right()) > 0; break;
        }
        if (touches)
            return i;
    }
    return std::nullopt;
}

void CMonitorWall::begin(const SRect& home, const SRect& window) {
    m_home   = home;
    m_active = true;
    m_broken = !contains(home, window);
    m_held   = false;
}

void CMonitorWall::end() {
    m_active = false;
    m_held   = false;
}

bool CMonitorWall::holding() const {
    return m_held;
}

SWallStep CMonitorWall::step(const SRect& wanted, const std::vector<SRect>& monitors, double strength) {
    SWallStep result{.x = wanted.x, .y = wanted.y};
    m_held = false;
    if (!m_active || strength <= 0)
        return result;

    // Once the window's middle is on another monitor, that monitor is home, with a wall of its own.
    for (const auto& m : monitors) {
        if (sameRect(m, m_home) || wanted.centerX() < m.x || wanted.centerX() >= m.right() || wanted.centerY() < m.y || wanted.centerY() >= m.bottom())
            continue;
        m_home   = m;
        m_broken = !contains(m, wanted);
        break;
    }

    // Pulled back inside: the wall stands again.
    if (contains(m_home, wanted)) {
        m_broken = false;
        return result;
    }
    if (m_broken)
        return result;

    struct SOver {
        eEdge  edge;
        double by;
    };
    const SOver overs[] = {
        {EDGE_RIGHT, wanted.right() - m_home.right()},
        {EDGE_LEFT, m_home.x - wanted.x},
        {EDGE_BOTTOM, wanted.bottom() - m_home.bottom()},
        {EDGE_TOP, m_home.y - wanted.y},
    };
    for (const auto& over : overs) {
        if (over.by <= 0)
            continue;
        const auto OTHER = neighbour(m_home, over.edge, wanted, monitors);
        if (!OTHER)
            continue;
        if (over.by >= strength) {
            m_broken            = true;
            result.brokeThrough = true;
            return result;
        }

        const auto& m = monitors[*OTHER];
        SWallPush   push{.monitor = *OTHER, .edge = oppositeEdge(over.edge), .strength = over.by / strength};
        switch (over.edge) {
            case EDGE_RIGHT: result.x = m_home.right() - wanted.w; break;
            case EDGE_LEFT: result.x = m_home.x; break;
            case EDGE_BOTTOM: result.y = m_home.bottom() - wanted.h; break;
            case EDGE_TOP: result.y = m_home.y; break;
        }
        if (isVerticalEdge(over.edge)) {
            push.start  = wanted.y - m.y;
            push.length = wanted.h;
        } else {
            push.start  = wanted.x - m.x;
            push.length = wanted.w;
        }
        if (!result.push || push.strength > result.push->strength)
            result.push = push;
    }
    m_held = result.push.has_value();
    return result;
}
