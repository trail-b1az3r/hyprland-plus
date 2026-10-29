#pragma once

// plus:max_fps: draw each monitor at most this many times a second (0: no limit). A frame that comes sooner is put
// off until its turn, then drawn with everything that changed meanwhile, so nothing is lost, only drawn less often.
// Halcyon's super battery saver sets it to 12.

#include "../SharedDefs.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/time/Time.hpp"

#include <unordered_map>

class CEventLoopTimer;

namespace Plus {

    class CFrameCap {
      public:
        // True when the monitor may draw now (and counts it as drawn); false when the frame was put off.
        bool allowFrame(PHLMONITOR monitor);

      private:
        struct SMonitorState {
            Time::steady_tp     last;
            SP<CEventLoopTimer> timer;
        };
        std::unordered_map<MONITORID, SMonitorState> m_monitors;
    };

    SP<CFrameCap> frameCap();
}
