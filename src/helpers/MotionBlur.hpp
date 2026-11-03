#pragma once

#include "time/Time.hpp"
#include "math/Math.hpp"
#include <optional>

namespace MotionBlur {
    CBox extents(const CBox& previous, const CBox& current);

    /** How many samples a move from previous to current needs, at most maxSamples: 0 when no corner moved as much as
     *  1.5 px (a blur that small cannot be seen, so the pass is skipped), then one sample more per 6 px of the
     *  largest corner's travel, from 2. Slow moves cost a fraction of a fast one's. */
    int samplesFor(const CBox& previous, const CBox& current, int maxSamples);

    struct SState {
        CBox previous;
        CBox current;
        int  samples = 1;

        CBox extents() const;
    };

    class CTracker {
      public:
        void                  record(const CBox& previous, const CBox& current);
        void                  reset();

        std::optional<SState> state(int samples, const Vector2D& offset = {}, bool allowStale = false) const;

      private:
        CBox            m_previous;
        CBox            m_current;
        Time::steady_tp m_updatedAt;
        bool            m_valid = false;
    };
};
