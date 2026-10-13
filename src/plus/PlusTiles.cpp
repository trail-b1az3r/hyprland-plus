#include "PlusTiles.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/actions/ConfigActions.hpp"
#include "../config/shared/complex/ComplexDataTypes.hpp"
#include "../desktop/Workspace.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/state/ViewState.hpp"
#include "../desktop/view/Window.hpp"
#include "../event/EventBus.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../layout/supplementary/DragController.hpp"
#include "../layout/target/Target.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "../managers/input/InputManager.hpp"
#include "../notification/NotificationOverlay.hpp"
#include "../output/Monitor.hpp"
#include "../render/Renderer.hpp"
#include "../render/pass/RectPassElement.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <limits>

using namespace Plus;

namespace CA = Config::Actions;

namespace {
    Layout::eRectCorner cornerOf(eEdge edge) {
        switch (edge) {
            case EDGE_LEFT: return Layout::CORNER_LEFT;
            case EDGE_RIGHT: return Layout::CORNER_RIGHT;
            case EDGE_TOP: return Layout::CORNER_TOP;
            case EDGE_BOTTOM: return Layout::CORNER_BOTTOM;
        }
        return Layout::CORNER_NONE;
    }

    // The layout's resize delta that moves `edge` outwards by `px`.
    Vector2D deltaOf(eEdge edge, double px) {
        switch (edge) {
            case EDGE_LEFT: return {-px, 0};
            case EDGE_RIGHT: return {px, 0};
            case EDGE_TOP: return {0, -px};
            case EDGE_BOTTOM: return {0, px};
        }
        return {};
    }

    eEdge edgeOfDirection(Math::eDirection dir) {
        switch (dir) {
            case Math::DIRECTION_LEFT: return EDGE_LEFT;
            case Math::DIRECTION_RIGHT: return EDGE_RIGHT;
            case Math::DIRECTION_UP: return EDGE_TOP;
            default: return EDGE_BOTTOM;
        }
    }

    SRect rectOf(const CBox& b) {
        return {b.x, b.y, b.w, b.h};
    }

    CBox boxOf(const SRect& r) {
        return CBox{r.x, r.y, r.w, r.h};
    }

    std::string jsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (const char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (sc<unsigned char>(c) < 0x20)
                        out += std::format("\\u{:04x}", sc<int>(c));
                    else
                        out += c;
            }
        }
        return out;
    }

    // The configured color, with its alpha multiplied by `factor`.
    CHyprColor withAlpha(int64_t argb, double factor) {
        CHyprColor c(sc<uint64_t>(argb));
        return CHyprColor(c.r, c.g, c.b, std::clamp(c.a * factor, 0.0, 1.0));
    }

    // The configured color, fully opaque: for outlines and edge bars.
    CHyprColor opaque(int64_t argb, double alpha = 1.0) {
        CHyprColor c(sc<uint64_t>(argb));
        return CHyprColor(c.r, c.g, c.b, std::clamp(alpha, 0.0, 1.0));
    }

    constexpr double MIN_MOVE = 0.5;
}

uint64_t Plus::idOf(PHLWINDOW window) {
    return rc<uint64_t>(window.get());
}

UP<CPlusTiles>& Plus::tiles() {
    static UP<CPlusTiles> p = makeUnique<CPlusTiles>();
    return p;
}

CPlusTiles::CPlusTiles() {
    auto& ev = Event::bus()->m_events;

    // An edit session ends on its own when its tile loses focus, closes, floats, goes fullscreen or its workspace
    // goes away, so no highlight is ever left behind.
    m_listeners.emplace_back(ev.window.active.listen([this](PHLWINDOW w, Desktop::eFocusReason) {
        if (m_edit.onEvent(EDIT_EVENT_FOCUS_CHANGED, w ? idOf(w) : 0))
            endEdit(false);
        if (m_inspector)
            updateInspector();
    }));
    m_listeners.emplace_back(ev.window.close.listen([this](PHLWINDOW w) {
        if (m_edit.onEvent(EDIT_EVENT_WINDOW_CLOSED, idOf(w)))
            endEdit(false);
        if (w && w->m_workspace)
            scheduleRestore(w->m_workspace);
    }));
    m_listeners.emplace_back(ev.window.floating.listen([this](PHLWINDOW w) {
        if (m_edit.onEvent(EDIT_EVENT_WINDOW_FLOATED, idOf(w)))
            endEdit(false);
        if (w && w->m_workspace)
            scheduleRestore(w->m_workspace);
    }));
    m_listeners.emplace_back(ev.window.fullscreen.listen([this](PHLWINDOW w) {
        if (w && Fullscreen::controller()->isFullscreen(w) && m_edit.onEvent(EDIT_EVENT_FULLSCREEN, idOf(w)))
            endEdit(false);
    }));
    m_listeners.emplace_back(ev.window.open.listen([this](PHLWINDOW w) {
        if (w && w->m_workspace)
            scheduleRestore(w->m_workspace);
    }));
    m_listeners.emplace_back(ev.window.moveToWorkspace.listen([this](PHLWINDOW w, PHLWORKSPACE ws) {
        if (ws)
            scheduleRestore(ws);
        if (w && m_edit.active() && idOf(w) == m_edit.tile() && m_edit.onEvent(EDIT_EVENT_WORKSPACE_CHANGED, rc<uint64_t>(ws.get())))
            endEdit(false);
    }));
    m_listeners.emplace_back(ev.workspace.active.listen([this](PHLWORKSPACE ws) {
        if (m_edit.active()) {
            const auto W = m_editWindow.lock();
            // Only a change on the edited tile's own monitor matters.
            if (!W || (ws && W->m_monitor.lock() == ws->m_monitor.lock() && m_edit.onEvent(EDIT_EVENT_WORKSPACE_CHANGED, rc<uint64_t>(ws.get()))))
                endEdit(false);
        }
        clearGroup(GROUP_FLASH);
        if (m_inspector)
            updateInspector();
    }));
    m_listeners.emplace_back(ev.monitor.layoutChanged.listen([this]() {
        // Resolution, scale or arrangement changed: put custom sizes back on every visible workspace.
        for (const auto& s : m_states) {
            const auto W = s.window.lock();
            if (W && s.modified && W->m_workspace)
                scheduleRestore(W->m_workspace);
        }
    }));
    m_listeners.emplace_back(ev.keybinds.submap.listen([this](const std::string& name) {
        if (m_changingSubmap || !m_edit.active() || !m_enteredSubmap)
            return;
        static auto PSUBMAP = CConfigValue<Config::STRING>("plus:edit_submap");
        if (name != *PSUBMAP && m_edit.onEvent(EDIT_EVENT_SUBMAP_LEFT)) {
            m_enteredSubmap = false; // left by the user's own bind, nothing to put back
            endEdit(false);
        }
    }));
    m_listeners.emplace_back(ev.config.reloaded.listen([this]() {
        if (!enabled() && m_edit.onEvent(EDIT_EVENT_PLUS_DISABLED))
            endEdit(false);
    }));
}

bool CPlusTiles::enabled() const {
    static auto PENABLED = CConfigValue<Config::BOOL>("plus:enabled");
    return *PENABLED;
}

bool CPlusTiles::participates(PHLWINDOW w) const {
    static auto PALL = CConfigValue<Config::BOOL>("plus:all_tiled");
    if (!enabled() || !w || !w->m_isMapped || w->m_isFloating || w->isMini() || Fullscreen::controller()->isFullscreen(w))
        return false;
    if (w->m_ruleApplicator->superposition().valueOrDefault())
        return false;
    return *PALL || w->m_ruleApplicator->plustile().valueOrDefault();
}

bool CPlusTiles::isModified(PHLWINDOW w) const {
    const auto* s = stateFor(w);
    return s && s->modified;
}

bool CPlusTiles::editing() const {
    return m_edit.active();
}

double CPlusTiles::tolerance() const {
    // Two tiles count as touching across the inner gap on both of them.
    static auto PGAPSINDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    auto* const GAPS        = sc<Config::CCssGapData*>(PGAPSINDATA.ptr());
    const auto  BIGGEST     = std::max({GAPS->m_top, GAPS->m_right, GAPS->m_bottom, GAPS->m_left});
    return sc<double>(BIGGEST) * 2.0 + 4.0;
}

std::optional<CTileBoard> CPlusTiles::board(PHLWORKSPACE ws) const {
    static auto PMINW = CConfigValue<Config::INTEGER>("plus:min_width");
    static auto PMINH = CConfigValue<Config::INTEGER>("plus:min_height");
    static auto PMAXW = CConfigValue<Config::INTEGER>("plus:max_width");
    static auto PMAXH = CConfigValue<Config::INTEGER>("plus:max_height");

    if (!ws || !ws->m_space)
        return std::nullopt;

    std::vector<STile> tiles;
    for (const auto& wt : ws->m_space->targets()) {
        const auto T = wt.lock();
        if (!T || T->floating())
            continue;
        const auto W = T->window();
        if (!W || !W->m_isMapped || W->isHidden() || W->isMini() || Fullscreen::controller()->isFullscreen(W))
            continue;
        if (W->m_ruleApplicator->superposition().valueOrDefault())
            continue;

        STile tile;
        tile.id   = idOf(W);
        tile.rect = rectOf(T->position());

        const auto MIN = T->minSize().value_or(Vector2D{});
        const auto MAX = T->maxSize().value_or(Vector2D{});
        tile.minW      = std::max(sc<double>(*PMINW), MIN.x);
        tile.minH      = std::max(sc<double>(*PMINH), MIN.y);
        // Windows with no real maximum report huge or zero sizes; only a plausible one counts.
        tile.maxW = MAX.x > 0 && MAX.x < 100000 ? MAX.x : 0;
        tile.maxH = MAX.y > 0 && MAX.y < 100000 ? MAX.y : 0;
        if (*PMAXW > 0)
            tile.maxW = tile.maxW > 0 ? std::min(tile.maxW, sc<double>(*PMAXW)) : sc<double>(*PMAXW);
        if (*PMAXH > 0)
            tile.maxH = tile.maxH > 0 ? std::min(tile.maxH, sc<double>(*PMAXH)) : sc<double>(*PMAXH);

        tile.locked = W->m_ruleApplicator->plusLocked().valueOrDefault();
        if (const auto* S = stateFor(W); S && S->modified) {
            tile.modified = true;
            tile.customW  = S->customW;
            tile.customH  = S->customH;
        }
        tiles.push_back(tile);
    }

    return CTileBoard(std::move(tiles), rectOf(ws->m_space->workArea()), tolerance());
}

PHLWINDOW CPlusTiles::windowById(PHLWORKSPACE ws, uint64_t id) const {
    if (!ws || !ws->m_space)
        return nullptr;
    for (const auto& wt : ws->m_space->targets()) {
        const auto T = wt.lock();
        if (T && T->window() && idOf(T->window()) == id)
            return T->window();
    }
    return nullptr;
}

CPlusTiles::SWindowState* CPlusTiles::stateFor(PHLWINDOW w, bool create) {
    if (!w)
        return nullptr;
    for (auto& s : m_states) {
        if (s.window.lock() == w)
            return &s;
    }
    if (!create)
        return nullptr;
    prune();
    m_states.push_back(SWindowState{.window = w});
    return &m_states.back();
}

const CPlusTiles::SWindowState* CPlusTiles::stateFor(PHLWINDOW w) const {
    if (!w)
        return nullptr;
    for (const auto& s : m_states) {
        if (s.window.lock() == w)
            return &s;
    }
    return nullptr;
}

void CPlusTiles::prune() {
    std::erase_if(m_states, [](const SWindowState& s) { return !s.window.lock(); });
}

std::string CPlusTiles::nameOf(PHLWINDOW w) const {
    if (!w)
        return "?";
    const std::string CLASS = w->m_class.empty() ? w->m_initialClass : w->m_class;
    return CLASS.empty() ? std::format("{:x}", rc<uintptr_t>(w.get())) : CLASS;
}

std::string CPlusTiles::withNames(std::string text, PHLWORKSPACE ws) const {
    // TileBoard writes tiles as #id. Put the window's class there instead.
    size_t pos = 0;
    while ((pos = text.find('#', pos)) != std::string::npos) {
        size_t end = pos + 1;
        while (end < text.size() && std::isdigit(sc<unsigned char>(text[end])))
            ++end;
        if (end == pos + 1) {
            ++pos;
            continue;
        }
        const auto ID   = std::stoull(text.substr(pos + 1, end - pos - 1));
        const auto NAME = nameOf(windowById(ws, ID));
        text.replace(pos, end - pos, NAME);
        pos += NAME.size();
    }
    return text;
}

void CPlusTiles::notify(const std::string& text, bool failure) const {
    Log::logger->log(Log::DEBUG, "plus: {}", text);
    Notification::overlay()->addNotification(text, failure ? CHyprColor(1.F, 0.4F, 0.4F, 1.F) : CHyprColor(0.5F, 0.8F, 1.F, 1.F), failure ? 4000.F : 2500.F,
                                             failure ? ICON_WARNING : ICON_INFO);
}

// --- Move Edge ------------------------------------------------------------------------------------------------

double CPlusTiles::stepPx(eStep step) const {
    static auto PSTEP   = CConfigValue<Config::INTEGER>("plus:step");
    static auto PFINE   = CConfigValue<Config::INTEGER>("plus:step_fine");
    static auto PCOARSE = CConfigValue<Config::INTEGER>("plus:step_coarse");
    switch (step) {
        case STEP_FINE: return *PFINE;
        case STEP_COARSE: return *PCOARSE;
        default: return *PSTEP;
    }
}

Result CPlusTiles::beginEdit(PHLWINDOW w, std::optional<eEdge> edge) {
    if (!enabled())
        return std::unexpected("Plus Tiles are disabled (plus:enabled)");
    if (!w)
        return std::unexpected("no window to edit");
    if (!participates(w))
        return std::unexpected(std::format("{} is not a Plus Tile. Give it the plustile rule, or set plus:all_tiled", nameOf(w)));

    if (m_edit.active() && m_edit.tile() != idOf(w))
        endEdit(false);

    auto B = board(w->m_workspace);
    if (!B || !B->tile(idOf(w)))
        return std::unexpected("the window is not on a tile board");

    // Start on the requested edge, or on the first edge that can move, right and bottom first.
    eEdge start = edge.value_or(EDGE_RIGHT);
    if (!edge) {
        for (const auto E : {EDGE_RIGHT, EDGE_BOTTOM, EDGE_LEFT, EDGE_TOP}) {
            if (B->adjustable(idOf(w), E)) {
                start = E;
                break;
            }
        }
    }

    m_edit.begin(idOf(w), rc<uint64_t>(w->m_workspace.get()), start);
    m_editWindow = w;

    static auto       PSUBMAP = CConfigValue<Config::STRING>("plus:edit_submap");
    const std::string SUBMAP  = *PSUBMAP;
    if (!SUBMAP.empty() && CA::state()->m_currentSubmap != SUBMAP) {
        m_submapBeforeEdit = CA::state()->m_currentSubmap;
        m_changingSubmap   = true;
        const auto R       = CA::setSubmap(SUBMAP);
        m_changingSubmap   = false;
        m_enteredSubmap    = R.has_value();
        if (!R)
            Log::logger->log(Log::WARN, "plus: edit submap \"{}\" is not defined, so edge keys must be bound globally", SUBMAP);
    }

    updatePreview();
    return {};
}

Result CPlusTiles::toggleEdit(PHLWINDOW w) {
    if (m_edit.active()) {
        endEdit(true);
        return {};
    }
    return beginEdit(w);
}

Result CPlusTiles::commitEdit() {
    if (!m_edit.active())
        return std::unexpected("not editing an edge");
    endEdit(true);
    return {};
}

Result CPlusTiles::cancelEdit() {
    if (!m_edit.active())
        return {};
    endEdit(false);
    return {};
}

void CPlusTiles::endEdit(bool commit) {
    const auto W       = m_editWindow.lock();
    const auto PENDING = m_edit.active() ? m_edit.commit() : std::array<double, 4>{0, 0, 0, 0};
    m_edit.cancel();
    m_editWindow.reset();
    clearGroup(GROUP_PREVIEW);

    static auto PSUBMAP = CConfigValue<Config::STRING>("plus:edit_submap");
    if (m_enteredSubmap && CA::state()->m_currentSubmap == *PSUBMAP) {
        m_changingSubmap = true;
        (void)CA::setSubmap(m_submapBeforeEdit.empty() ? "reset" : m_submapBeforeEdit);
        m_changingSubmap = false;
    }
    m_enteredSubmap = false;
    m_submapBeforeEdit.clear();

    if (!commit || !W || !W->m_isMapped)
        return;

    std::vector<std::string> errors;
    for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
        if (std::abs(PENDING[E]) < MIN_MOVE)
            continue;
        if (const auto R = applyMove(W, E, PENDING[E], true); !R)
            errors.push_back(R.error());
    }

    if (!errors.empty()) {
        std::string all;
        for (const auto& e : errors)
            all += (all.empty() ? "" : " ") + e;
        notify(all, true);
    }
}

Result CPlusTiles::selectEdge(eEdge edge) {
    if (!m_edit.active())
        return std::unexpected("not editing an edge. Use Move Edge first");
    m_edit.selectEdge(edge);
    updatePreview();
    return {};
}

Result CPlusTiles::moveEdge(double px) {
    static auto PPREVIEW = CConfigValue<Config::BOOL>("plus:edit_preview");

    if (!m_edit.active())
        return std::unexpected("not editing an edge. Use Move Edge first");

    const auto W = m_editWindow.lock();
    if (!W) {
        endEdit(false);
        return std::unexpected("the edited window is gone");
    }

    if (!*PPREVIEW) {
        const auto R = applyMove(W, m_edit.edge(), px, true);
        updatePreview();
        return R;
    }

    // Preview: check the whole pending move against the board, and keep only what fits.
    auto B = board(W->m_workspace);
    if (!B || !B->tile(idOf(W))) {
        endEdit(false);
        return std::unexpected("the window is not on a tile board any more");
    }

    const eEdge  EDGE    = m_edit.edge();
    const double WANTED  = m_edit.pending(EDGE) + px;
    const auto   PREVIEW = B->proposeEdgeMove(idOf(W), EDGE, WANTED);

    if (!PREVIEW.ok) {
        m_edit.setPending(EDGE, 0);
        updatePreview();
        flash(W->m_workspace, B->tile(idOf(W))->rect, FEEDBACK_FAILED, EDGE);
        if (auto* S = stateFor(W, true))
            S->lastRefusal = withNames(PREVIEW.describe(), W->m_workspace);
        return std::unexpected(withNames(PREVIEW.describe(), W->m_workspace));
    }

    m_edit.setPending(EDGE, PREVIEW.applied);
    updatePreview();

    if (PREVIEW.limit != LIMIT_NONE) {
        flash(W->m_workspace, B->tile(idOf(W))->rect, FEEDBACK_FAILED, EDGE);
        notify(withNames(PREVIEW.describe(), W->m_workspace), true);
    }
    return {};
}

Result CPlusTiles::moveEdgeNow(PHLWINDOW w, eEdge edge, double px) {
    if (!w)
        return std::unexpected("no window");
    if (!participates(w))
        return std::unexpected(std::format("{} is not a Plus Tile", nameOf(w)));
    return applyMove(w, edge, px, true);
}

Result CPlusTiles::applyMove(PHLWINDOW w, eEdge edge, double px, bool markModified) {
    const auto T = w->layoutTarget();
    if (!T)
        return std::unexpected("the window has no layout target");

    auto B = board(w->m_workspace);
    if (!B || !B->tile(idOf(w)))
        return std::unexpected("the window is not on a tile board");

    const SRect BEFORE = B->tile(idOf(w))->rect;
    const auto  MOVE   = B->proposeEdgeMove(idOf(w), edge, px);
    if (!MOVE.ok) {
        const auto WHY = withNames(MOVE.describe(), w->m_workspace);
        if (auto* S = stateFor(w, true))
            S->lastRefusal = WHY;
        flash(w->m_workspace, BEFORE, FEEDBACK_FAILED, edge);
        return std::unexpected(WHY);
    }

    // The layout moves the boundary, so the tiles that share it give up or take the space.
    g_layoutManager->resizeTarget(deltaOf(edge, MOVE.applied), T, cornerOf(edge));

    const SRect AFTER   = rectOf(T->position());
    const bool  CHANGED = std::abs(AFTER.w - BEFORE.w) >= MIN_MOVE || std::abs(AFTER.h - BEFORE.h) >= MIN_MOVE || std::abs(AFTER.x - BEFORE.x) >= MIN_MOVE ||
        std::abs(AFTER.y - BEFORE.y) >= MIN_MOVE;
    if (!CHANGED) {
        const std::string WHY = std::format("The layout did not move the {} edge. The active layout may not support resizing in that direction", edgeName(edge));
        if (auto* S = stateFor(w, true))
            S->lastRefusal = WHY;
        flash(w->m_workspace, BEFORE, FEEDBACK_FAILED, edge);
        return std::unexpected(WHY);
    }

    if (markModified) {
        auto* S = stateFor(w, true);
        if (!S->modified) {
            S->originalW = BEFORE.w;
            S->originalH = BEFORE.h;
        }
        S->modified = true;
        if (isVerticalEdge(edge))
            S->customW = AFTER.w;
        else
            S->customH = AFTER.h;
        S->lastRefusal.clear();
    }

    flash(w->m_workspace, AFTER, FEEDBACK_EDGE, edge);
    for (const auto& [id, rect] : MOVE.rects) {
        if (id != idOf(w))
            flash(w->m_workspace, rect, FEEDBACK_NEIGHBOR);
    }

    if (MOVE.limit != LIMIT_NONE)
        notify(withNames(MOVE.describe(), w->m_workspace), true);

    return {};
}

void CPlusTiles::updatePreview() {
    clearGroup(GROUP_PREVIEW);

    const auto W = m_editWindow.lock();
    if (!m_edit.active() || !W)
        return;

    auto B = board(W->m_workspace);
    if (!B || !B->tile(idOf(W)))
        return;

    const auto ID       = idOf(W);
    const auto ORIGINAL = *B;
    CTileBoard sim      = *B;

    // Every pending edge, applied in turn to a copy of the board.
    for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
        if (std::abs(m_edit.pending(E)) < MIN_MOVE)
            continue;
        const auto MOVE = sim.proposeEdgeMove(ID, E, m_edit.pending(E));
        sim.apply(MOVE);
    }

    const auto NOW = Time::steadyNow();
    auto       add = [&](const SRect& r, eFeedbackKind kind, std::optional<eEdge> edge) {
        m_feedback.push_back(SFeedback{.workspace = W->m_workspace, .box = r, .kind = kind, .edge = edge, .start = NOW, .durationMs = 0, .group = GROUP_PREVIEW});
        damage(m_feedback.back());
    };

    add(sim.tile(ID)->rect, FEEDBACK_EDGE, m_edit.edge());

    // Neighbours: those the pending moves resize, or, with nothing pending yet, those across the selected edge.
    bool anyChanged = false;
    for (const auto& t : sim.tiles()) {
        if (t.id == ID)
            continue;
        const auto* O = ORIGINAL.tile(t.id);
        if (O &&
            (std::abs(O->rect.x - t.rect.x) >= MIN_MOVE || std::abs(O->rect.y - t.rect.y) >= MIN_MOVE || std::abs(O->rect.w - t.rect.w) >= MIN_MOVE ||
             std::abs(O->rect.h - t.rect.h) >= MIN_MOVE)) {
            add(t.rect, FEEDBACK_NEIGHBOR, std::nullopt);
            anyChanged = true;
        }
    }
    if (!anyChanged) {
        if (const auto BOUNDARY = sim.boundary(ID, m_edit.edge())) {
            for (const auto F : BOUNDARY->farSide)
                add(sim.tile(F)->rect, FEEDBACK_NEIGHBOR, std::nullopt);
        }
    }
}

// --- Fit to Neighbors -------------------------------------------------------------------------------------------

bool CPlusTiles::hasCustomTiles(PHLWORKSPACE ws) const {
    for (const auto& s : m_states) {
        const auto W = s.window.lock();
        if (W && s.modified && W->m_workspace == ws)
            return true;
    }
    if (ws && ws->m_space) {
        for (const auto& wt : ws->m_space->targets()) {
            const auto T = wt.lock();
            if (T && T->window() && T->window()->m_ruleApplicator->plusLocked().valueOrDefault())
                return true;
        }
    }
    return false;
}

Result CPlusTiles::fit(PHLWINDOW w) {
    if (!w)
        return std::unexpected("no window");
    if (!participates(w))
        return std::unexpected(std::format("{} is not a Plus Tile", nameOf(w)));

    const auto WS = w->m_workspace;
    if (!WS || !WS->m_space)
        return std::unexpected("the window has no workspace");

    if (!isModified(w) && !w->m_ruleApplicator->plusLocked().valueOrDefault()) {
        // Ordinary tile: fit the space the layout assigns, by the layout's own rules.
        WS->m_space->recalculate();
        if (const auto T = w->layoutTarget())
            flash(WS, rectOf(T->position()), FEEDBACK_EDGE);
        return {};
    }

    // Edge-modified tile: keep its custom size and make the neighbours fit around it.
    return restore(WS, w, true);
}

Result CPlusTiles::reset(PHLWINDOW w) {
    if (!w)
        return std::unexpected("no window");
    auto* S = stateFor(w, false);
    if (!S || !S->modified)
        return {};

    // Go back to the size the tile had before its first edge move, then forget the custom size.
    if (S->originalW || S->originalH) {
        S->customW = S->originalW;
        S->customH = S->originalH;
        restore(w->m_workspace, w, false);
    }

    S = stateFor(w, false);
    if (S) {
        S->modified = false;
        S->customW.reset();
        S->customH.reset();
        S->originalW.reset();
        S->originalH.reset();
        S->lastRefusal.clear();
    }

    if (const auto T = w->layoutTarget())
        flash(w->m_workspace, rectOf(T->position()), FEEDBACK_EDGE);
    return {};
}

Result CPlusTiles::restore(PHLWORKSPACE ws, PHLWINDOW priority, bool interactive) {
    if (m_restoring || !ws || !ws->m_space)
        return {};

    // Our own resizes must not start another restore.
    m_restoring = true;

    auto customError = [](const CTileBoard& b) {
        double e = 0;
        for (const auto& t : b.tiles()) {
            if (!t.modified && !t.locked)
                continue;
            if (t.customW)
                e += std::abs(*t.customW - t.rect.w);
            if (t.customH)
                e += std::abs(*t.customH - t.rect.h);
        }
        return e;
    };

    const std::optional<uint64_t> PRIORITY = priority ? std::optional<uint64_t>{idOf(priority)} : std::nullopt;

    // The plan says which boundaries to move. The layout then really moves them, and may do it a little differently
    // (rounding, its own minimums), so the board is read again and planned again, a few times at most.
    double previous = std::numeric_limits<double>::infinity();
    for (int round = 0; round < 4; ++round) {
        auto B = board(ws);
        if (!B)
            break;

        const double ERR = customError(*B);
        if (ERR <= B->tolerance() || ERR >= previous - MIN_MOVE)
            break;
        previous = ERR;

        const auto PLAN = B->planRestore(PRIORITY);
        if (PLAN.steps.empty())
            break;

        for (const auto& step : PLAN.steps) {
            const auto W = windowById(ws, step.id);
            const auto T = W ? W->layoutTarget() : nullptr;
            if (T)
                g_layoutManager->resizeTarget(deltaOf(step.edge, step.delta), T, cornerOf(step.edge));
        }
    }

    m_restoring = false;

    const auto B = board(ws);
    if (!B)
        return {};

    const auto FINAL = B->planRestore(PRIORITY);
    if (FINAL.unsatisfied.empty())
        return {};

    std::string text;
    for (const auto& u : FINAL.unsatisfied) {
        const auto W = windowById(ws, u.id);
        if (auto* S = stateFor(W, false))
            S->lastRefusal = withNames(u.reason, ws);
        if (interactive)
            flash(ws, B->tile(u.id)->rect, FEEDBACK_FAILED);
        text += std::format("{}{}: {}", text.empty() ? "" : " ", nameOf(W), withNames(u.reason, ws));
    }

    if (interactive) {
        notify("Fit to Neighbors kept what it could. " + text, true);
        return std::unexpected(text);
    }
    return {};
}

void CPlusTiles::scheduleRestore(PHLWORKSPACE ws) {
    static auto PAUTO = CConfigValue<Config::BOOL>("plus:auto_restore");
    if (!*PAUTO || !enabled() || m_restoring || !ws || !hasCustomTiles(ws))
        return;

    if (std::ranges::none_of(m_pendingRestore, [&](const PHLWORKSPACEREF& r) { return r.lock() == ws; }))
        m_pendingRestore.emplace_back(ws);

    if (m_restoreScheduled)
        return;
    m_restoreScheduled = true;
    // After the layout has finished reacting to whatever happened, once.
    g_pEventLoopManager->doLater([this]() { runScheduledRestores(); });
}

void CPlusTiles::runScheduledRestores() {
    m_restoreScheduled = false;
    auto pending       = std::move(m_pendingRestore);
    m_pendingRestore.clear();

    // Not while a window is being dragged: the drop recalculates anyway, and will schedule again.
    if (g_layoutManager->dragController()->target())
        return;

    for (const auto& r : pending) {
        if (const auto WS = r.lock())
            restore(WS, nullptr, false);
    }
}

// --- Navigation -----------------------------------------------------------------------------------------------

Result CPlusTiles::focus(Math::eDirection direction) {
    static auto PPREVIEW = CConfigValue<Config::BOOL>("plus:focus_preview");

    const auto  W = Desktop::focusState()->window();
    if (!enabled() || !W || W->m_isFloating) {
        const auto R = CA::moveFocus(direction);
        return R ? Result{} : std::unexpected(R.error().message);
    }

    auto       B      = board(W->m_workspace);
    const auto TARGET = B && B->tile(idOf(W)) ? B->neighborInDirection(idOf(W), edgeOfDirection(direction)) : std::nullopt;
    const auto NEXT   = TARGET ? windowById(W->m_workspace, *TARGET) : nullptr;

    if (!NEXT) {
        // No tile that way on this workspace: the normal movefocus handles monitors and floats.
        const auto R = CA::moveFocus(direction);
        return R ? Result{} : std::unexpected(R.error().message);
    }

    if (*PPREVIEW)
        flash(W->m_workspace, B->tile(*TARGET)->rect, FEEDBACK_FOCUS);

    const auto R = CA::focus(NEXT);
    return R ? Result{} : std::unexpected(R.error().message);
}

// --- Inspector --------------------------------------------------------------------------------------------------

Result CPlusTiles::toggleInspector() {
    m_inspector = !m_inspector;
    if (m_inspector)
        updateInspector();
    else
        clearGroup(GROUP_INSPECT);
    return {};
}

bool CPlusTiles::inspecting() const {
    return m_inspector;
}

void CPlusTiles::updateInspector() {
    clearGroup(GROUP_INSPECT);
    const auto W = Desktop::focusState()->window();
    if (!m_inspector || !W || !W->m_workspace)
        return;

    auto B = board(W->m_workspace);
    if (!B)
        return;

    const auto NOW = Time::steadyNow();
    for (const auto& t : B->tiles()) {
        m_feedback.push_back(SFeedback{.workspace = W->m_workspace, .box = t.rect, .kind = FEEDBACK_INSPECT, .start = NOW, .durationMs = 0, .group = GROUP_INSPECT});
        damage(m_feedback.back());
    }

    if (!B->tile(idOf(W)))
        return;

    // The focused tile's adjustable edges, and its neighbours.
    for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
        if (!B->adjustable(idOf(W), E))
            continue;
        m_feedback.push_back(
            SFeedback{.workspace = W->m_workspace, .box = B->tile(idOf(W))->rect, .kind = FEEDBACK_EDGE, .edge = E, .start = NOW, .durationMs = 0, .group = GROUP_INSPECT});
        for (const auto& n : B->neighbors(idOf(W), E))
            m_feedback.push_back(
                SFeedback{.workspace = W->m_workspace, .box = B->tile(n.id)->rect, .kind = FEEDBACK_NEIGHBOR, .start = NOW, .durationMs = 0, .group = GROUP_INSPECT});
    }

    notify(inspect(W, false), false);
}

std::string CPlusTiles::inspect(PHLWINDOW w, bool json) const {
    if (!w)
        return json ? "{\"error\": \"no window\"}" : "No window.";

    const auto  WS = w->m_workspace;
    auto        B  = board(WS);
    const auto  ID = idOf(w);
    const auto* T  = B ? B->tile(ID) : nullptr;
    const auto* S  = stateFor(w);

    const bool  LOCKED   = w->m_ruleApplicator->plusLocked().valueOrDefault();
    const bool  MODIFIED = S && S->modified;
    std::string state    = !participates(w) ? "not a Plus Tile" : MODIFIED ? "edge-modified" : "ordinary";
    if (LOCKED)
        state += ", locked";

    std::string pending;
    if (m_edit.active() && m_edit.tile() == ID) {
        for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
            if (std::abs(m_edit.pending(E)) >= MIN_MOVE)
                pending += std::format("{}{} {:+.0f} px", pending.empty() ? "" : ", ", edgeName(E), m_edit.pending(E));
        }
        pending = std::format("editing the {} edge{}", edgeName(m_edit.edge()), pending.empty() ? "" : ": " + pending);
    }

    if (json) {
        std::string out = std::format("{{\"address\": \"0x{:x}\", \"class\": \"{}\", \"title\": \"{}\", \"state\": \"{}\", \"plustile\": {}, \"modified\": {}, \"locked\": {}",
                                      rc<uintptr_t>(w.get()), jsonEscape(w->m_class), jsonEscape(w->m_title), jsonEscape(state), participates(w), MODIFIED, LOCKED);
        if (T) {
            out += std::format(", \"geometry\": [{:.0f}, {:.0f}, {:.0f}, {:.0f}], \"min\": [{:.0f}, {:.0f}], \"max\": [{:.0f}, {:.0f}]", T->rect.x, T->rect.y, T->rect.w, T->rect.h,
                               T->minW, T->minH, T->maxW, T->maxH);
            out += ", \"neighbors\": {";
            bool firstEdge = true;
            for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
                out += std::format("{}\"{}\": [", firstEdge ? "" : ", ", edgeName(E));
                firstEdge  = false;
                bool first = true;
                for (const auto& n : B->neighbors(ID, E)) {
                    const auto NW = windowById(WS, n.id);
                    out += std::format("{}{{\"address\": \"0x{:x}\", \"class\": \"{}\", \"shared\": {:.0f}}}", first ? "" : ", ", n.id, jsonEscape(nameOf(NW)), n.shared);
                    first = false;
                }
                out += "]";
            }
            out += "}, \"adjustable\": [";
            bool first = true;
            for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
                if (B->adjustable(ID, E)) {
                    out += std::format("{}\"{}\"", first ? "" : ", ", edgeName(E));
                    first = false;
                }
            }
            out += "]";
        }
        out += std::format(", \"custom\": [{}, {}]", S && S->customW ? std::format("{:.0f}", *S->customW) : "null", S && S->customH ? std::format("{:.0f}", *S->customH) : "null");
        out += std::format(", \"pending\": \"{}\", \"last_refusal\": \"{}\"}}", jsonEscape(pending), jsonEscape(S ? S->lastRefusal : ""));
        return out;
    }

    std::string out = std::format("{} ({}): {}\n", nameOf(w), w->m_title, state);
    if (!T)
        return out + "Not on a tile board (floating, fullscreen, mini or superposition).\n";

    out += std::format("Geometry: {:.0f},{:.0f} {:.0f}x{:.0f}\n", T->rect.x, T->rect.y, T->rect.w, T->rect.h);
    out += std::format("Minimum: {:.0f}x{:.0f}", T->minW, T->minH);
    if (T->maxW > 0 || T->maxH > 0)
        out += std::format("  Maximum: {}x{}", T->maxW > 0 ? std::format("{:.0f}", T->maxW) : "-", T->maxH > 0 ? std::format("{:.0f}", T->maxH) : "-");
    out += "\n";
    if (S && (S->customW || S->customH))
        out += std::format("Custom size: {} x {}\n", S->customW ? std::format("{:.0f}", *S->customW) : "layout", S->customH ? std::format("{:.0f}", *S->customH) : "layout");

    for (const auto E : {EDGE_LEFT, EDGE_RIGHT, EDGE_TOP, EDGE_BOTTOM}) {
        const auto N = B->neighbors(ID, E);
        out += std::format("{}: ", edgeName(E));
        if (N.empty())
            out += "work area edge";
        for (size_t i = 0; i < N.size(); ++i)
            out += std::format("{}{} (shares {:.0f} px)", i ? ", " : "", nameOf(windowById(WS, N[i].id)), N[i].shared);
        if (const auto BOUNDARY = B->boundary(ID, E); BOUNDARY && !BOUNDARY->farSide.empty() && BOUNDARY->nearSide.size() > 1)
            out += std::format("; moves with {} other tile(s) on this side", BOUNDARY->nearSide.size() - 1);
        out += "\n";
    }

    if (!pending.empty())
        out += "Pending: " + pending + "\n";
    if (S && !S->lastRefusal.empty())
        out += "Last refusal: " + S->lastRefusal + "\n";

    const auto ISSUES = B->validate();
    for (const auto& issue : ISSUES)
        out += "Board: " + withNames(issue, WS) + "\n";
    return out;
}

std::string CPlusTiles::boardInfo(PHLWORKSPACE ws, bool json) const {
    auto B = board(ws);
    if (!B)
        return json ? "[]" : "No tile board.\n";

    std::string out   = json ? "[" : "";
    bool        first = true;
    for (const auto& t : B->tiles()) {
        const auto W = windowById(ws, t.id);
        if (json) {
            out += (first ? "" : ",") + inspect(W, true);
            first = false;
        } else
            out += inspect(W, false) + "\n";
    }
    return json ? out + "]" : out;
}

// --- Feedback and rendering -------------------------------------------------------------------------------------

SRect CPlusTiles::visualBox(const SRect& r, PHLWORKSPACE ws) const {
    // Layout boxes include the gaps. Inset them the way the windows are, so highlights sit on the windows.
    static auto PGAPSINDATA  = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    auto* const GIN          = sc<Config::CCssGapData*>(PGAPSINDATA.ptr());
    auto* const GOUT         = sc<Config::CCssGapData*>(PGAPSOUTDATA.ptr());

    SRect       area = r;
    if (ws && ws->m_space)
        area = rectOf(ws->m_space->workArea());

    auto       sticks = [](double a, double b) { return std::abs(a - b) < 2.0; };
    const auto L      = sticks(r.x, area.x) ? GOUT->m_left : GIN->m_left;
    const auto T      = sticks(r.y, area.y) ? GOUT->m_top : GIN->m_top;
    const auto R      = sticks(r.right(), area.right()) ? GOUT->m_right : GIN->m_right;
    const auto B      = sticks(r.bottom(), area.bottom()) ? GOUT->m_bottom : GIN->m_bottom;

    // The work area already leaves room for the outer gaps on most layouts, so only inner gaps are taken off there.
    SRect v{r.x + (sticks(r.x, area.x) ? 0 : L), r.y + (sticks(r.y, area.y) ? 0 : T), 0, 0};
    v.w = std::max(1.0, r.right() - (sticks(r.right(), area.right()) ? 0 : R) - v.x);
    v.h = std::max(1.0, r.bottom() - (sticks(r.bottom(), area.bottom()) ? 0 : B) - v.y);
    return v;
}

void CPlusTiles::flash(PHLWORKSPACE ws, const SRect& box, eFeedbackKind kind, std::optional<eEdge> edge) {
    static auto PDURATION = CConfigValue<Config::INTEGER>("plus:feedback:duration");
    if (!ws || *PDURATION <= 0)
        return;
    m_feedback.push_back(SFeedback{.workspace = ws, .box = box, .kind = kind, .edge = edge, .start = Time::steadyNow(), .durationMs = sc<int>(*PDURATION), .group = GROUP_FLASH});
    damage(m_feedback.back());
}

void CPlusTiles::clearGroup(int group) {
    for (const auto& f : m_feedback) {
        if (f.group == group)
            damage(f);
    }
    std::erase_if(m_feedback, [group](const SFeedback& f) { return f.group == group; });
}

void CPlusTiles::damage(const SFeedback& f) const {
    static auto PBORDER = CConfigValue<Config::INTEGER>("plus:feedback:border_size");
    const auto  V       = inflate(visualBox(f.box, f.workspace.lock()), *PBORDER + 2);
    g_pHyprRenderer->damageBox(boxOf(V));
}

void CPlusTiles::renderDragGuide(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace) {
    static auto PDRAGGUIDE = CConfigValue<Config::BOOL>("binds:drag_guide");
    static auto PCOLOR     = CConfigValue<Config::INTEGER>("plus:feedback:placement_color");
    static auto PBORDER    = CConfigValue<Config::INTEGER>("plus:feedback:border_size");
    static auto PROUNDING  = CConfigValue<Config::INTEGER>("plus:feedback:rounding");
    static auto PFILL      = CConfigValue<Config::FLOAT>("plus:feedback:fill_opacity");

    auto        forget = [this]() {
        if (m_lastGuide.w > 0) {
            g_pHyprRenderer->damageBox(boxOf(inflate(m_lastGuide, *PBORDER + 2)));
            m_lastGuide = {};
            m_lastGuideWorkspace.reset();
        }
    };

    if (pMonitor->m_activeWorkspace != pWorkspace)
        return;

    const auto& DRAG   = g_layoutManager->dragController();
    const auto  TARGET = DRAG->target();

    // A tiled window becomes floating while it is dragged (see CDragStateController::updateDragWindow), so checking
    // floating() alone hid the guide for every tiled drag. draggingTiled() says what it was before the drag.
    if (!*PDRAGGUIDE || !TARGET || !TARGET->window() || DRAG->mode() != MBIND_MOVE || !DRAG->dragThresholdReached() || (TARGET->floating() && !DRAG->draggingTiled())) {
        if (m_lastGuideWorkspace.lock() == pWorkspace || !m_lastGuideWorkspace.lock())
            forget();
        return;
    }

    const auto HOVERED = Desktop::viewState()->hitTest().windowAt(g_pInputManager->getMouseCoordsInternal(),
                                                                  Desktop::View::ALLOW_FLOATING | Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS, TARGET->window());
    if (!HOVERED || HOVERED == TARGET->window() || HOVERED->m_isFloating || HOVERED->m_workspace != pWorkspace) {
        forget();
        return;
    }

    const auto HT = HOVERED->layoutTarget();
    if (!HT) {
        forget();
        return;
    }

    const SRect GUIDE = visualBox(rectOf(HT->position()), pWorkspace);
    if (GUIDE.x != m_lastGuide.x || GUIDE.y != m_lastGuide.y || GUIDE.w != m_lastGuide.w || GUIDE.h != m_lastGuide.h) {
        // The old and new places both need redrawing, or the old highlight stays behind.
        forget();
        g_pHyprRenderer->damageBox(boxOf(inflate(GUIDE, *PBORDER + 2)));
        m_lastGuide          = GUIDE;
        m_lastGuideWorkspace = pWorkspace;
    }

    const auto  OFFSET = pWorkspace->m_renderOffset->value();
    const SRect R      = toRenderBox(GUIDE, pMonitor->m_position.x, pMonitor->m_position.y, pMonitor->m_scale, OFFSET.x, OFFSET.y);

    // The placement color's own alpha is the fill. The outline is solid, so the target reads at a glance.
    if (*PFILL > 0) {
        CRectPassElement::SRectData data;
        data.box   = boxOf(R);
        data.color = withAlpha(*PCOLOR, 1.0);
        data.round = sc<int>(*PROUNDING * pMonitor->m_scale);
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    if (*PBORDER > 0) {
        for (const auto& side : outline(R, std::max(1.0, *PBORDER * pMonitor->m_scale))) {
            CRectPassElement::SRectData o;
            o.box   = boxOf(side);
            o.color = opaque(*PCOLOR);
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(o));
        }
    }
}

void CPlusTiles::render(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace) {
    static auto PENABLED   = CConfigValue<Config::BOOL>("plus:feedback:enabled");
    static auto PPLACEMENT = CConfigValue<Config::INTEGER>("plus:feedback:placement_color");
    static auto PEDGE      = CConfigValue<Config::INTEGER>("plus:feedback:edge_color");
    static auto PNEIGHBOR  = CConfigValue<Config::INTEGER>("plus:feedback:neighbor_color");
    static auto PFAILED    = CConfigValue<Config::INTEGER>("plus:feedback:failed_color");
    static auto PFOCUS     = CConfigValue<Config::INTEGER>("plus:feedback:focus_color");
    static auto PINSPECT   = CConfigValue<Config::INTEGER>("plus:feedback:inspect_color");
    static auto PBORDER    = CConfigValue<Config::INTEGER>("plus:feedback:border_size");
    static auto PFILL      = CConfigValue<Config::FLOAT>("plus:feedback:fill_opacity");
    static auto PROUNDING  = CConfigValue<Config::INTEGER>("plus:feedback:rounding");

    if (m_feedback.empty())
        return;

    // Flashes that are over: one last damage so they are drawn away, then gone.
    const auto NOW       = Time::steadyNow();
    auto       elapsedMs = [&](const SFeedback& f) { return std::chrono::duration<double, std::milli>(NOW - f.start).count(); };
    for (const auto& f : m_feedback) {
        if (f.durationMs > 0 && elapsedMs(f) >= f.durationMs)
            damage(f);
    }
    std::erase_if(m_feedback, [&](const SFeedback& f) { return f.durationMs > 0 && elapsedMs(f) >= f.durationMs; });
    std::erase_if(m_feedback, [](const SFeedback& f) { return !f.workspace.lock(); });

    if (!*PENABLED || pMonitor->m_activeWorkspace != pWorkspace)
        return;

    const auto OFFSET = pWorkspace->m_renderOffset->value();

    for (const auto& f : m_feedback) {
        if (f.workspace.lock() != pWorkspace)
            continue;

        const double ALPHA = f.durationMs > 0 ? flashAlpha(elapsedMs(f), f.durationMs) : 1.0;
        if (ALPHA <= 0)
            continue;

        int64_t argb = *PEDGE;
        switch (f.kind) {
            case FEEDBACK_PLACEMENT: argb = *PPLACEMENT; break;
            case FEEDBACK_EDGE: argb = *PEDGE; break;
            case FEEDBACK_NEIGHBOR: argb = *PNEIGHBOR; break;
            case FEEDBACK_FAILED: argb = *PFAILED; break;
            case FEEDBACK_FOCUS: argb = *PFOCUS; break;
            case FEEDBACK_INSPECT: argb = *PINSPECT; break;
        }

        const SRect R = toRenderBox(visualBox(f.box, pWorkspace), pMonitor->m_position.x, pMonitor->m_position.y, pMonitor->m_scale, OFFSET.x, OFFSET.y);

        // An inspector outline of a whole board would hide it under fills; edge bars and outlines are enough there.
        if (*PFILL > 0 && !(f.kind == FEEDBACK_INSPECT || (f.group == GROUP_INSPECT && !f.edge))) {
            CRectPassElement::SRectData fill;
            fill.box   = boxOf(R);
            fill.color = withAlpha(argb, *PFILL * ALPHA);
            fill.round = sc<int>(*PROUNDING * pMonitor->m_scale);
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(fill));
        }

        if (*PBORDER > 0 && !f.edge) {
            for (const auto& side : outline(R, *PBORDER * pMonitor->m_scale)) {
                CRectPassElement::SRectData o;
                o.box   = boxOf(side);
                o.color = opaque(argb, ALPHA);
                g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(o));
            }
        }

        if (f.edge) {
            CRectPassElement::SRectData bar;
            bar.box   = boxOf(edgeBar(R, *f.edge, std::max(4.0, *PBORDER * 3.0 * pMonitor->m_scale)));
            bar.color = opaque(argb, ALPHA);
            bar.round = sc<int>(std::max(2.0, *PBORDER * pMonitor->m_scale));
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(bar));
        }

        // Keep fading flashes moving.
        if (f.durationMs > 0)
            damage(f);
    }
}
