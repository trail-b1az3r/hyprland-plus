#pragma once

// The wall between monitors (plus:monitor_wall): a window dragged against an edge that another monitor shares stops
// there, as if against something elastic, until it has been pushed past the edge by the wall's strength; then it goes
// through. While it is held, the other monitor gets a bulge where the window would come in (the shell draws it from
// the monitorwall event). Kept free of compositor types so it can be tested on its own; DragController drives it.

#include "TileBoard.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace Plus {

    struct SWallPush {
        size_t monitor  = 0;         // index of the monitor pushed against, in the list given to step()
        eEdge  edge     = EDGE_LEFT; // that monitor's edge the bulge is on
        double start    = 0;         // where along that edge the window's side begins, in its local coordinates
        double length   = 0;         // how long the window's side is
        double strength = 0;         // 0..1: how far into the wall, 1 the moment it gives
    };

    struct SWallStep {
        double                   x = 0, y = 0; // where the window goes (held at the edge, or where it was asked to)
        std::optional<SWallPush> push;         // set while the window is held against the wall
        bool                     brokeThrough = false;
    };

    class CMonitorWall {
      public:
        // A drag begins: home is the monitor the window is on. A window already over an edge isn't stopped there.
        void begin(const SRect& home, const SRect& window);
        // Each move: the window's wanted box, the monitors (home among them, at any index). strength in pixels; 0 is
        // no wall.
        SWallStep step(const SRect& wanted, const std::vector<SRect>& monitors, double strength);
        void      end();

        bool      holding() const;

      private:
        SRect m_home   = {};
        bool  m_active = false;
        bool  m_broken = false;
        bool  m_held   = false;
    };
}
