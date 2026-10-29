#include "FrameCap.hpp"

#include "../config/ConfigValue.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"
#include "../output/Monitor.hpp"
#include "../state/MonitorState.hpp"

#include <chrono>

using namespace Plus;

SP<CFrameCap> Plus::frameCap() {
    static SP<CFrameCap> cap = makeShared<CFrameCap>();
    return cap;
}

bool CFrameCap::allowFrame(PHLMONITOR monitor) {
    static auto PMAXFPS = CConfigValue<Config::INTEGER>("plus:max_fps");
    if (*PMAXFPS <= 0 || !monitor)
        return true;

    const auto NOW      = Time::steadyNow();
    const auto INTERVAL = std::chrono::duration_cast<Time::steady_dur>(std::chrono::microseconds(1'000'000 / *PMAXFPS));
    auto&      state    = m_monitors[monitor->m_id];

    if (NOW - state.last >= INTERVAL) {
        state.last = NOW;
        return true;
    }

    // Too soon: draw it when its turn comes. One timer per monitor, armed once however many frames ask meanwhile.
    if (!state.timer) {
        const MONITORID ID = monitor->m_id;
        state.timer        = makeShared<CEventLoopTimer>(
            std::nullopt,
            [ID](SP<CEventLoopTimer> self, void* data) {
                const auto M = State::monitorState()->query().id(ID).run();
                if (M)
                    M->scheduleFrame();
            },
            nullptr);
        g_pEventLoopManager->addTimer(state.timer);
    }
    if (!state.timer->armed())
        state.timer->updateTimeout(INTERVAL - (NOW - state.last));
    return false;
}
