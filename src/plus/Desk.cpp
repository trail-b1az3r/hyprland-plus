#include "Desk.hpp"

#include "../config/ConfigValue.hpp"
#include "../config/shared/complex/ComplexDataTypes.hpp"
#include "../workspace/HLWorkspace.hpp"
#include "../desktop/rule/windowRule/WindowRuleApplicator.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../managers/input/InputManager.hpp"
#include "../output/Monitor.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../render/Renderer.hpp"
#include "../render/pass/BorderPassElement.hpp"
#include "../render/pass/RectPassElement.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace Plus;

UP<CDesk>& Plus::desk() {
    static UP<CDesk> p = makeUnique<CDesk>();
    return p;
}

static double msSince(const Time::steady_tp& start) {
    return std::chrono::duration<double, std::milli>(Time::steadyNow() - start).count();
}

static CHyprColor colorOf(int64_t argb, double alphaFactor) {
    const auto A = sc<double>((argb >> 24) & 0xff) / 255.0;
    const auto R = sc<double>((argb >> 16) & 0xff) / 255.0;
    const auto G = sc<double>((argb >> 8) & 0xff) / 255.0;
    const auto B = sc<double>(argb & 0xff) / 255.0;
    return CHyprColor(R, G, B, std::clamp(A * alphaFactor, 0.0, 1.0));
}

// Desktop peek

CDesk::SPeek* CDesk::peekFor(PHLMONITOR monitor) {
    std::erase_if(m_peeks, [](const SPeek& p) { return !p.monitor.lock(); });
    for (auto& p : m_peeks) {
        if (p.monitor.lock() == monitor)
            return &p;
    }
    return nullptr;
}

double CDesk::peekElapsedMs(const SPeek& peek) const {
    return msSince(peek.start);
}

void CDesk::endPeek(SPeek& peek) {
    static auto PDURATION = CConfigValue<Config::INTEGER>("plus:peek_duration");
    if (!peek.entering)
        return;

    // Fade back from wherever the fade in got to, so a quick toggle doesn't jump.
    const double DONE = std::clamp(peekElapsedMs(peek) / std::max<double>(1, *PDURATION), 0.0, 1.0);
    peek.entering     = false;
    peek.start        = Time::steadyNow() - std::chrono::milliseconds(sc<int64_t>((1.0 - DONE) * sc<double>(*PDURATION)));
}

std::expected<void, std::string> CDesk::peek(PHLMONITOR monitor, std::optional<bool> on) {
    if (!monitor)
        return std::unexpected("no monitor to peek on");

    auto*      p    = peekFor(monitor);
    const bool IS   = p && p->entering;
    const bool WANT = on.value_or(!IS);
    if (WANT == IS)
        return {};

    if (WANT) {
        if (p)
            m_peeks.erase(m_peeks.begin() + (p - m_peeks.data()));
        m_peeks.push_back(SPeek{.monitor = monitor, .workspace = monitor->m_activeWorkspace, .entering = true, .start = Time::steadyNow()});
    } else
        endPeek(*p);

    g_pHyprRenderer->damageMonitor(monitor);
    // The pointer may be over a window that it now goes through, or the other way round.
    g_pInputManager->simulateMouseMovement();
    return {};
}

bool CDesk::peeking(PHLMONITOR monitor) {
    if (m_peeks.empty() || !monitor)
        return false;
    auto* p = peekFor(monitor);
    if (!p || !p->entering)
        return false;
    if (p->workspace.lock() != monitor->m_activeWorkspace) {
        endPeek(*p);
        return false;
    }
    return true;
}

float CDesk::windowOpacity(PHLMONITOR monitor) {
    static auto PDURATION = CConfigValue<Config::INTEGER>("plus:peek_duration");
    static auto POPACITY  = CConfigValue<Config::FLOAT>("plus:peek_opacity");

    if (m_peeks.empty() || !monitor)
        return 1.F;

    auto* p = peekFor(monitor);
    if (!p)
        return 1.F;

    peeking(monitor); // ends the peek if the workspace changed

    const double ELAPSED = peekElapsedMs(*p);
    const double OPACITY = peekOpacity(ELAPSED, *PDURATION, p->entering, *POPACITY);

    if (ELAPSED < *PDURATION)
        g_pHyprRenderer->damageMonitor(monitor); // keep the fade moving
    else if (!p->entering) {
        m_peeks.erase(m_peeks.begin() + (p - m_peeks.data()));
        return 1.F;
    }

    return sc<float>(OPACITY);
}

// Locate pointer

void CDesk::damageRing() const {
    static auto  PSIZE = CConfigValue<Config::INTEGER>("plus:locate_size");
    const double REACH = sc<double>(*PSIZE) * 3.0 + 8.0;
    g_pHyprRenderer->damageBox(CBox{m_ring.pos.x - REACH, m_ring.pos.y - REACH, REACH * 2.0, REACH * 2.0});
}

void CDesk::locatePointer() {
    if (m_ring.active)
        damageRing();
    m_ring = {.active = true, .pos = g_pInputManager->getMouseCoordsInternal(), .start = Time::steadyNow()};
    damageRing();
}

void CDesk::onPointerMove(const Vector2D& pos) {
    static auto PSHAKE = CConfigValue<Config::BOOL>("plus:shake_to_find");

    if (m_ring.active) {
        // The ring follows the pointer.
        damageRing();
        m_ring.pos = pos;
        damageRing();
    }

    if (!*PSHAKE)
        return;

    if (m_shake.feed(pos.x, pos.y, sc<double>(Time::millis(Time::steadyNow()))))
        locatePointer();
}

void CDesk::render(PHLMONITOR monitor, Render::CRenderContext& ctx) {
    static auto PDURATION = CConfigValue<Config::INTEGER>("plus:locate_duration");
    static auto PSIZE     = CConfigValue<Config::INTEGER>("plus:locate_size");
    static auto PCOLOR    = CConfigValue<Config::INTEGER>("plus:feedback:locate_color");

    if (!m_ring.active || !monitor)
        return;

    const double ELAPSED = msSince(m_ring.start);
    if (ELAPSED >= *PDURATION) {
        m_ring.active = false;
        damageRing();
        return;
    }

    const auto FRAME = locateRing(ELAPSED, *PDURATION, *PSIZE);
    damageRing(); // keep it moving

    if (FRAME.alpha <= 0 || !monitor->logicalBox().expand(FRAME.radius + 8).containsPoint(m_ring.pos))
        return;

    const double                SCALE  = monitor->m_scale;
    const double                R      = FRAME.radius * SCALE;
    const auto                  CENTRE = (m_ring.pos - monitor->m_position) * SCALE;
    const CBox                  BOX{CENTRE.x - R, CENTRE.y - R, R * 2.0, R * 2.0};

    CRectPassElement::SRectData glow;
    glow.box   = BOX;
    glow.color = colorOf(*PCOLOR, FRAME.alpha * 0.18);
    glow.round = sc<int>(std::round(R));
    Render::IHyprRenderer::addPassElement(ctx, makeUnique<CRectPassElement>(glow));

    CBorderPassElement::SBorderData ring;
    ring.box        = BOX;
    ring.grad1      = Config::CGradientValueData(colorOf(*PCOLOR, 1.0));
    ring.a          = sc<float>(FRAME.alpha);
    ring.round      = sc<int>(std::round(R));
    ring.borderSize = sc<int>(std::max(2.0, std::round(4.0 * SCALE)));
    Render::IHyprRenderer::addPassElement(ctx, makeUnique<CBorderPassElement>(ring));
}

// Hold focus

bool CDesk::holdsFocus(PHLWINDOW window) const {
    static auto PGAMES = CConfigValue<Config::BOOL>("plus:game_hold_focus");

    if (!window || !window->mapped())
        return false;
    if (window->m_ruleApplicator->holdFocus().valueOrDefault())
        return true;
    return *PGAMES && window->getContentType() == NContentType::CONTENT_TYPE_GAME;
}
