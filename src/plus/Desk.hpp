#pragma once

// hyprland-plus desk features, beside Plus Tiles:
//
//  - Desktop peek: the windows on a monitor fade almost away and the pointer goes through them, to the wallpaper and
//    the widgets on it. Run it again, or change workspace, to bring them back. (hl.dsp.plus.peek)
//  - Locate pointer: a ring closes in on the pointer. Shaking the mouse does it with plus:shake_to_find.
//    (hl.dsp.plus.locate_pointer)
//  - Hold focus: a window with the hold_focus rule (or a game, with plus:game_hold_focus) keeps the keyboard when the
//    pointer leaves it, as if input:follow_mouse were 2 while it has focus. A click still moves focus.
//
// The arithmetic (fades, the ring, shake recognition) is in Motion.hpp and tested in tests/plus/.

#include "Motion.hpp"

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/time/Time.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace Render {
    class CRenderContext;
}

namespace Plus {

    class CDesk {
      public:
        // Desktop peek on a monitor: on, off, or the other way round.
        std::expected<void, std::string> peek(PHLMONITOR monitor, std::optional<bool> on);
        // Whether the pointer goes through the windows on this monitor (peek is on, and not fading back).
        bool peeking(PHLMONITOR monitor);
        // What the renderer multiplies a window's opacity by on this monitor: 1 outside peek.
        float windowOpacity(PHLMONITOR monitor);

        void  locatePointer();
        // Every pointer move, for plus:shake_to_find.
        void onPointerMove(const Vector2D& pos);
        // Drawn above everything but the cursor.
        void render(PHLMONITOR monitor, Render::CRenderContext& ctx);

        bool holdsFocus(PHLWINDOW window) const;

      private:
        struct SPeek {
            PHLMONITORREF   monitor;
            PHLWORKSPACEREF workspace; // peek ends when the monitor shows another one
            bool            entering = true;
            Time::steady_tp start;
        };

        SPeek*             peekFor(PHLMONITOR monitor);
        double             peekElapsedMs(const SPeek& peek) const;
        void               endPeek(SPeek& peek);

        std::vector<SPeek> m_peeks;

        struct SRing {
            bool            active = false;
            Vector2D        pos;
            Time::steady_tp start;
        } m_ring;

        void           damageRing() const;

        CShakeDetector m_shake;
    };

    UP<CDesk>& desk();
}
