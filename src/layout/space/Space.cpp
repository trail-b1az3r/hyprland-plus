#include "Space.hpp"

#include "../target/Target.hpp"
#include "../algorithm/Algorithm.hpp"

#include "../../debug/log/Logger.hpp"
#include "../../desktop/Workspace.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../event/EventBus.hpp"
#include "../../output/Monitor.hpp"

using namespace Layout;

SP<CSpace> CSpace::create(PHLWORKSPACE w) {
    auto space    = SP<CSpace>(new CSpace(w));
    space->m_self = space;
    return space;
}

CSpace::CSpace(PHLWORKSPACE parent) : m_parent(parent) {
    recheckWorkArea();

    // NOLINTNEXTLINE
    m_geomUpdateCallback = Event::bus()->m_events.monitor.layoutChanged.listen([this] {
        // During monitor disconnect/reconnect (e.g. sleep/wake), some workspaces
        // may have stale or null monitors. Guard against that to avoid crashing
        // when recalculating layout for workspaces mid-migration.
        if (!m_parent || !m_parent->m_monitor)
            return;

        recheckWorkArea();

        if (m_algorithm)
            m_algorithm->recalculate();
    });
}

// The strip at one screen edge that the superposition windows share. Edges: 0 left, 1 right, 2 top, 3 bottom.
static CBox superpositionStrip(const CBox& area, Config::INTEGER edge, Config::FLOAT size) {
    switch (edge) {
        case 0: return {area.x, area.y, area.w * size, area.h};
        case 2: return {area.x, area.y, area.w, area.h * size};
        case 3: return {area.x, area.y + area.h * (1.F - size), area.w, area.h * size};
        default: return {area.x + area.w * (1.F - size), area.y, area.w * size, area.h};
    }
}

bool CSpace::isSuperposition(const SP<ITarget>& t) const {
    const auto W = t->window();
    return W && !t->floating() && W->m_ruleApplicator->superposition().valueOrDefault();
}

std::vector<SP<ITarget>> CSpace::superpositionTargets() const {
    std::vector<SP<ITarget>> result;
    for (const auto& w : m_targets) {
        const auto t = w.lock();
        if (t && isSuperposition(t))
            result.emplace_back(t);
    }
    return result;
}

void CSpace::placeSuperpositions() {
    const auto TARGETS = superpositionTargets();
    if (TARGETS.empty() || !m_parent || !m_parent->m_monitor)
        return;

    static auto PSUPEREDGE = CConfigValue<Config::INTEGER>("superposition:edge");
    static auto PSUPERSIZE = CConfigValue<Config::FLOAT>("superposition:size");

    const auto  STRIP    = superpositionStrip(m_parent->m_monitor->logicalBoxMinusReserved(), *PSUPEREDGE, *PSUPERSIZE);
    const bool  VERTICAL = *PSUPEREDGE == 0 || *PSUPEREDGE == 1; // left or right: the windows stack from top to bottom
    const auto  N        = sc<double>(TARGETS.size());

    for (size_t i = 0; i < TARGETS.size(); ++i) {
        const auto I   = sc<double>(i);
        const CBox BOX = VERTICAL ? CBox{STRIP.x, STRIP.y + I * STRIP.h / N, STRIP.w, STRIP.h / N} : CBox{STRIP.x + I * STRIP.w / N, STRIP.y, STRIP.w / N, STRIP.h};
        TARGETS[i]->setPositionGlobal(BOX);
        TARGETS[i]->warpPositionSize();
    }
}

void CSpace::add(SP<ITarget> t) {
    m_targets.emplace_back(t);

    if (isSuperposition(t)) {
        // Not in the tile tree: the strip is placed instead, and in reserve mode the tiles' area shrinks.
        recalculate();
        m_parent->updateWindows();
        return;
    }

    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->addTarget(t);

    m_parent->updateWindows();
}

void CSpace::move(SP<ITarget> t, std::optional<Vector2D> focalPoint) {
    m_targets.emplace_back(t);

    if (isSuperposition(t)) {
        recalculate();
        m_parent->updateWindows();
        return;
    }

    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->moveTarget(t, focalPoint);

    m_parent->updateWindows();
}

void CSpace::remove(SP<ITarget> t) {
    const bool SUPERPOSITION = isSuperposition(t);

    std::erase_if(m_targets, [&t](const auto& e) { return !e || e == t; });

    recheckWorkArea();

    if (m_algorithm && !SUPERPOSITION)
        m_algorithm->removeTarget(t);

    if (SUPERPOSITION)
        recalculate();

    if (m_parent) // can be null if the workspace is gone
        m_parent->updateWindows();
}

void CSpace::setAlgorithmProvider(SP<CAlgorithm> algo) {
    m_algorithm = algo;
}

void CSpace::recheckWorkArea() {
    if (!m_parent || !m_parent->m_monitor) {
        Log::logger->log(Log::ERR, "CSpace: recheckWorkArea on no parent / mon?!");
        return;
    }

    const auto  WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_parent.lock()).value_or(Config::CWorkspaceRule{});

    auto        workArea = m_parent->m_monitor->logicalBoxMinusReserved();

    static auto PGAPSOUTDATA   = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    static auto PFLOATGAPSDATA = CConfigValue<Config::IComplexConfigValue>("general:float_gaps");
    auto* const PGAPSOUT       = sc<Config::CCssGapData*>(PGAPSOUTDATA.ptr());
    auto*       PFLOATGAPS     = sc<Config::CCssGapData*>(PFLOATGAPSDATA.ptr());
    if (PFLOATGAPS->m_bottom < 0 || PFLOATGAPS->m_left < 0 || PFLOATGAPS->m_right < 0 || PFLOATGAPS->m_top < 0)
        PFLOATGAPS = PGAPSOUT;

    auto                   gapsOut   = WORKSPACERULE.m_gapsOut.value_or(*PGAPSOUT);
    auto                   gapsFloat = WORKSPACERULE.m_floatGaps.value_or(*PFLOATGAPS);

    Desktop::CReservedArea reservedGaps{gapsOut.m_top, gapsOut.m_right, gapsOut.m_bottom, gapsOut.m_left};
    Desktop::CReservedArea reservedFloatGaps{gapsFloat.m_top, gapsFloat.m_right, gapsFloat.m_bottom, gapsFloat.m_left};

    auto                   floatWorkArea = workArea;

    reservedFloatGaps.applyip(floatWorkArea);
    reservedGaps.applyip(workArea);

    m_workArea         = workArea;
    m_floatingWorkArea = floatWorkArea;

    // Reserve mode: the strip the superposition windows sit on comes out of the area the tiles use.
    static auto PSUPEREDGE    = CConfigValue<Config::INTEGER>("superposition:edge");
    static auto PSUPERSIZE    = CConfigValue<Config::FLOAT>("superposition:size");
    static auto PSUPERRESERVE = CConfigValue<Config::BOOL>("superposition:reserve");
    if (*PSUPERRESERVE && m_parent && m_parent->m_monitor && !superpositionTargets().empty()) {
        const auto STRIP = superpositionStrip(m_parent->m_monitor->logicalBoxMinusReserved(), *PSUPEREDGE, *PSUPERSIZE);
        switch (*PSUPEREDGE) {
            case 0:
                m_workArea.x += STRIP.w;
                m_workArea.w -= STRIP.w;
                break;
            case 2:
                m_workArea.y += STRIP.h;
                m_workArea.h -= STRIP.h;
                break;
            case 3: m_workArea.h -= STRIP.h; break;
            default: m_workArea.w -= STRIP.w; break;
        }
    }
}

const CBox& CSpace::workArea(bool floating) const {
    return floating ? m_floatingWorkArea : m_workArea;
}

PHLWORKSPACE CSpace::workspace() const {
    return m_parent.lock();
}

void CSpace::toggleTargetFloating(SP<ITarget> t) {
    t->setWasTiling(true);
    m_algorithm->setFloating(t, !t->floating());
    t->setWasTiling(false);

    m_parent->updateWindows();

    recalculate();
}

CBox CSpace::targetPositionLocal(SP<ITarget> t) const {
    return t->position().translate(-m_workArea.pos());
}

void CSpace::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    if (!m_algorithm)
        return;

    m_algorithm->resizeTarget(Δ, target, corner);
}

void CSpace::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    if (!m_algorithm)
        return;

    m_algorithm->moveTarget(Δ, target);
}

SP<CAlgorithm> CSpace::algorithm() const {
    return m_algorithm;
}

void CSpace::recalculate(eRecalculateReason reason) {
    recheckWorkArea();

    if (m_algorithm)
        m_algorithm->recalculate(reason);

    placeSuperpositions();
}

Config::ErrorResult CSpace::layoutMsg(const std::string_view& sv) {
    if (m_algorithm)
        return m_algorithm->layoutMsg(sv);

    return {};
}

std::optional<Vector2D> CSpace::predictSizeForNewTiledTarget() {
    if (m_algorithm)
        return m_algorithm->predictSizeForNewTiledTarget();

    return std::nullopt;
}

void CSpace::swap(SP<ITarget> a, SP<ITarget> b) {
    for (auto& t : m_targets) {
        if (t == a)
            t = b;
        else if (t == b)
            t = a;
    }

    if (m_algorithm)
        m_algorithm->swapTargets(a, b);
}

void CSpace::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    if (m_algorithm)
        m_algorithm->moveTargetInDirection(t, dir, silent);
}

void CSpace::setTargetGeom(const CBox& box, SP<ITarget> target) {
    if (m_algorithm)
        m_algorithm->setTargetGeom(box, target);
}

SP<ITarget> CSpace::getNextCandidate(SP<ITarget> old) {
    return !m_algorithm ? nullptr : m_algorithm->getNextCandidate(old);
}

bool Layout::isHardRecalculateReason(eRecalculateReason reason) {
    return reason != RECALCULATE_REASON_WORKSPACE_CHANGE && reason != RECALCULATE_REASON_SPECIAL_WORKSPACE_TOGGLE &&
        reason != RECALCULATE_REASON_TOGGLE_LAYOUT_HANDLED_FULLSCREEN && reason != RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN &&
        reason != RECALCULATE_REASON_INVALIDATE_MONITOR_GEOMETRIES && reason != RECALCULATE_REASON_RENDER_MONITOR;
}

const std::vector<WP<ITarget>>& CSpace::targets() const {
    return m_targets;
}

eRecalculateReason Layout::recalcMonitorReasonToRecalcReason(CLayoutManager::eRecalculateMonitorReason reason) {
    // If eRecalculateMonitorReason doesn't have a eRecalculateReason pair, it'll return nullopt
    switch (reason) {
        case CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_SPECIAL_WORKSPACE: return RECALCULATE_REASON_SPECIAL_WORKSPACE_TOGGLE;
        case CLayoutManager::RECALCULATE_MONITOR_REASON_WORKSPACE_CHANGE: return RECALCULATE_REASON_WORKSPACE_CHANGE;
        case CLayoutManager::RECALCULATE_MONITOR_REASON_TOGGLE_FULLSCREEN: return RECALCULATE_REASON_TOGGLE_DEFAULT_HANDLED_FULLSCREEN;
        default: return RECALCULATE_REASON_UNKNOWN;
    }
}