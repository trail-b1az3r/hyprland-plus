#pragma once

// Window resistance (plus:window_resistance): a dragged window stops at the edges it meets, the screen's edges and
// the edges of the other windows on its workspace, and goes past one only once it has been pushed that many pixels
// beyond it. Edges shared by two monitors are left to the monitor wall (MonitorWall.hpp). Kept free of compositor
// types so it can be tested on its own; DragController drives it.

#include "TileBoard.hpp"

#include <vector>

namespace Plus {

    struct SResisted {
        double x = 0, y = 0;
        bool   held = false; // stopped at an edge on either axis
    };

    // previous: where the window was put on the last move; wanted: where the cursor would put it now. windows: the
    // other windows to stop at; monitors: the screens. resistance in pixels; 0 lets every edge through.
    SResisted resist(const SRect& previous, const SRect& wanted, const std::vector<SRect>& windows, const std::vector<SRect>& monitors, double resistance);
}
