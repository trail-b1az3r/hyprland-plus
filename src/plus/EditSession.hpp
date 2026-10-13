#pragma once

// The Move Edge edit session, and the geometry of Plus highlights, kept free of compositor types so both can be
// tested on their own. src/plus/PlusTiles.cpp drives them.

#include "TileBoard.hpp"

#include <array>
#include <cstdint>

namespace Plus {

    // Things that end an edit session when they happen to the tile being edited, or around it.
    enum eEditEvent : uint8_t {
        EDIT_EVENT_FOCUS_CHANGED = 0, // subject: the window that got focus
        EDIT_EVENT_WORKSPACE_CHANGED, // subject: the workspace that became active on the tile's monitor
        EDIT_EVENT_WINDOW_CLOSED,     // subject: the window that closed
        EDIT_EVENT_WINDOW_FLOATED,    // subject: the window that became floating
        EDIT_EVENT_FULLSCREEN,        // subject: the window that went fullscreen
        EDIT_EVENT_SUBMAP_LEFT,       // the edit submap was left without committing
        EDIT_EVENT_PLUS_DISABLED,     // plus:enabled was turned off
    };

    class CEditSession {
      public:
        // Starts editing a tile. Returns false if a session is already running for another tile.
        bool     begin(uint64_t tile, uint64_t workspace, eEdge initialEdge = EDGE_RIGHT);

        bool     active() const;
        uint64_t tile() const;
        uint64_t workspace() const;
        eEdge    edge() const;
        double   pending(eEdge edge) const;
        bool     hasPending() const;

        void     selectEdge(eEdge edge);
        // Adds px to the selected edge's pending move: outwards when positive.
        void nudge(double px);
        // Replaces an edge's pending move, e.g. with the part of it the board allows.
        void setPending(eEdge edge, double px);

        // Returns true if the event ended the session.
        bool onEvent(eEditEvent event, uint64_t subject = 0);

        // Ends the session and returns the pending moves, indexed by eEdge.
        std::array<double, 4> commit();
        void                  cancel();

      private:
        bool                  m_active    = false;
        uint64_t              m_tile      = 0;
        uint64_t              m_workspace = 0;
        eEdge                 m_edge      = EDGE_RIGHT;
        std::array<double, 4> m_pending   = {0, 0, 0, 0};
    };

    // A global, logical box as the renderer wants it: relative to the monitor, moved by the workspace's animation
    // offset, and scaled to the monitor's pixels.
    SRect toRenderBox(const SRect& global, double monitorX, double monitorY, double scale, double workspaceOffsetX = 0, double workspaceOffsetY = 0);

    // The four sides of an outline drawn inside `r`.
    std::array<SRect, 4> outline(const SRect& r, double thickness);

    // A bar along one edge of `r`, inside it.
    SRect edgeBar(const SRect& r, eEdge edge, double thickness);

    // Grows or shrinks a rectangle on every side.
    SRect inflate(const SRect& r, double by);

    // Alpha of a flash `elapsedMs` into a `durationMs` life: full for the first 60 %, then fading out. 0 once over.
    double flashAlpha(double elapsedMs, double durationMs);
}
