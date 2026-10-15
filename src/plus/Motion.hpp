#pragma once

// The arithmetic behind desktop peek, locate pointer (shake to find) and mini all, kept free of compositor types so
// it can be tested on its own. src/plus/Desk.cpp drives it.

#include "TileBoard.hpp"

#include <cstddef>
#include <deque>
#include <vector>

namespace Plus {

    // Recognises a shake: the pointer going back and forth quickly over a short span, as people do to find it.
    struct SShakeConfig {
        double windowMs   = 500;  // only the last this many milliseconds count
        int    reversals  = 4;    // changes of direction along one axis
        double minTravel  = 600;  // pixels moved in the window, summed
        double minStroke  = 30;   // a stroke shorter than this doesn't count as a change of direction
        double maxDrift   = 0.35; // where the pointer ends up, as a fraction of the travel: a shake goes nowhere
        double cooldownMs = 1000; // after a shake is recognised, the next one can't start for this long
    };

    class CShakeDetector {
      public:
        explicit CShakeDetector(SShakeConfig config = {});

        // A pointer position at `ms` (any steady clock). Returns true once when a shake is recognised.
        bool feed(double x, double y, double ms);
        void reset();

      private:
        struct SSample {
            double x = 0, y = 0, ms = 0;
        };

        int                 reversalsAlong(bool horizontal) const;

        SShakeConfig        m_config;
        std::deque<SSample> m_samples;
        double              m_quietUntil = -1;
    };

    // How opaque windows are during desktop peek, `elapsedMs` after it started (entering) or ended (leaving): eased
    // from 1 to `target` over `durationMs` when entering, and back when leaving.
    double peekOpacity(double elapsedMs, double durationMs, bool entering, double target);

    // The locate pointer ring, `elapsedMs` into a `durationMs` life: it closes in on the pointer from three times
    // `radius` to `radius`, and fades over the last third.
    struct SRingFrame {
        double radius = 0;
        double alpha  = 0;
    };
    SRingFrame locateRing(double elapsedMs, double durationMs, double radius);

    // Where mini all puts `count` mini windows of `w` x `h` in `area`: a row along the bottom, from the left, `gap`
    // apart and `gap` from the edges, wrapping upwards when a row is full.
    std::vector<SRect> miniRow(const SRect& area, double w, double h, double gap, size_t count);
}
