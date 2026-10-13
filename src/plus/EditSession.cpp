#include "EditSession.hpp"

#include <algorithm>
#include <cmath>

using namespace Plus;

bool CEditSession::begin(uint64_t tile, uint64_t workspace, eEdge initialEdge) {
    if (m_active && m_tile != tile)
        return false;

    if (!m_active)
        m_pending = {0, 0, 0, 0};

    m_active    = true;
    m_tile      = tile;
    m_workspace = workspace;
    m_edge      = initialEdge;
    return true;
}

bool CEditSession::active() const {
    return m_active;
}

uint64_t CEditSession::tile() const {
    return m_tile;
}

uint64_t CEditSession::workspace() const {
    return m_workspace;
}

eEdge CEditSession::edge() const {
    return m_edge;
}

double CEditSession::pending(eEdge edge) const {
    return m_pending[edge];
}

bool CEditSession::hasPending() const {
    return std::ranges::any_of(m_pending, [](double v) { return std::abs(v) >= 0.5; });
}

void CEditSession::selectEdge(eEdge edge) {
    if (m_active)
        m_edge = edge;
}

void CEditSession::nudge(double px) {
    if (m_active)
        m_pending[m_edge] += px;
}

void CEditSession::setPending(eEdge edge, double px) {
    if (m_active)
        m_pending[edge] = px;
}

bool CEditSession::onEvent(eEditEvent event, uint64_t subject) {
    if (!m_active)
        return false;

    bool end = false;
    switch (event) {
        // Focus moving to another window, or the edited one going away, ends the session. Focus landing on the
        // edited window again (a refocus after a resize) does not.
        case EDIT_EVENT_FOCUS_CHANGED: end = subject != m_tile; break;
        case EDIT_EVENT_WORKSPACE_CHANGED: end = subject != m_workspace; break;
        case EDIT_EVENT_WINDOW_CLOSED:
        case EDIT_EVENT_WINDOW_FLOATED:
        case EDIT_EVENT_FULLSCREEN: end = subject == m_tile; break;
        case EDIT_EVENT_SUBMAP_LEFT:
        case EDIT_EVENT_PLUS_DISABLED: end = true; break;
    }

    if (end)
        cancel();
    return end;
}

std::array<double, 4> CEditSession::commit() {
    const auto RESULT = m_pending;
    cancel();
    return RESULT;
}

void CEditSession::cancel() {
    m_active    = false;
    m_tile      = 0;
    m_workspace = 0;
    m_edge      = EDGE_RIGHT;
    m_pending   = {0, 0, 0, 0};
}

SRect Plus::toRenderBox(const SRect& global, double monitorX, double monitorY, double scale, double workspaceOffsetX, double workspaceOffsetY) {
    SRect r;
    r.x = std::round((global.x - monitorX + workspaceOffsetX) * scale);
    r.y = std::round((global.y - monitorY + workspaceOffsetY) * scale);
    // Round the far edges, not the size, so neighbouring boxes still meet exactly.
    r.w = std::round((global.right() - monitorX + workspaceOffsetX) * scale) - r.x;
    r.h = std::round((global.bottom() - monitorY + workspaceOffsetY) * scale) - r.y;
    return r;
}

std::array<SRect, 4> Plus::outline(const SRect& r, double thickness) {
    const double T = std::clamp(thickness, 0.0, std::min(r.w, r.h) / 2.0);
    return {
        SRect{r.x, r.y, r.w, T},                       // top
        SRect{r.x, r.bottom() - T, r.w, T},            // bottom
        SRect{r.x, r.y + T, T, r.h - 2 * T},           // left
        SRect{r.right() - T, r.y + T, T, r.h - 2 * T}, // right
    };
}

SRect Plus::edgeBar(const SRect& r, eEdge edge, double thickness) {
    const double T = std::clamp(thickness, 0.0, std::min(r.w, r.h));
    switch (edge) {
        case EDGE_LEFT: return {r.x, r.y, T, r.h};
        case EDGE_RIGHT: return {r.right() - T, r.y, T, r.h};
        case EDGE_TOP: return {r.x, r.y, r.w, T};
        case EDGE_BOTTOM: return {r.x, r.bottom() - T, r.w, T};
    }
    return r;
}

SRect Plus::inflate(const SRect& r, double by) {
    return {r.x - by, r.y - by, std::max(0.0, r.w + 2 * by), std::max(0.0, r.h + 2 * by)};
}

double Plus::flashAlpha(double elapsedMs, double durationMs) {
    if (durationMs <= 0 || elapsedMs >= durationMs || elapsedMs < 0)
        return elapsedMs < 0 ? 1.0 : 0.0;
    const double HOLD = durationMs * 0.6;
    if (elapsedMs <= HOLD)
        return 1.0;
    return 1.0 - (elapsedMs - HOLD) / (durationMs - HOLD);
}
