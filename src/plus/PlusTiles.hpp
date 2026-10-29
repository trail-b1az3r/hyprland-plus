#pragma once

// Plus Tiles: ordinary tiled windows that can be given intentionally unequal sizes.
//
// A Plus Tile (the plustile rule, or every tiled window with plus:all_tiled) behaves exactly like any other tiled
// window: the active layout places and sizes it. Nothing changes until Move Edge is used on it.
//
// Move Edge starts an edit session on the tile. An edge is selected (left, right, top, bottom) and moved in steps;
// with plus:edit_preview the moves are previewed and applied on commit. Moving an edge goes through the layout,
// so the tiles that share that boundary give up or take the space: the window is never turned into a float.
// Once an edge has been moved, the tile is "edge-modified" and remembers its custom width and/or height.
//
// Fit to Neighbors then means two different things:
//  - ordinary tile: fit the space the layout assigns, as normal tiling does;
//  - edge-modified tile: keep the custom size and make the neighbours fit around it.
// With plus:auto_restore, custom sizes are put back after windows open, close or move, and after monitor changes.
//
// The geometry (adjacency, boundaries, limits, restore plans, navigation) lives in TileBoard.hpp; the edit state
// machine and highlight geometry in EditSession.hpp. Both are tested in tests/plus/.

#include "TileBoard.hpp"
#include "EditSession.hpp"

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/math/Direction.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../helpers/time/Time.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace Render {
    class CRenderContext;
}

namespace Plus {

    enum eFeedbackKind : uint8_t {
        FEEDBACK_PLACEMENT = 0, // where a dragged window will go
        FEEDBACK_EDGE,          // the tile being edited, and its selected edge
        FEEDBACK_NEIGHBOR,      // the neighbours an edge move resizes
        FEEDBACK_FAILED,        // a move or restore that cannot be done
        FEEDBACK_FOCUS,         // directional focus target
        FEEDBACK_INSPECT,       // geometry inspector outlines
    };

    enum eStep : uint8_t {
        STEP_NORMAL = 0,
        STEP_FINE,
        STEP_COARSE,
    };

    using Result = std::expected<void, std::string>;

    class CPlusTiles {
      public:
        CPlusTiles();
        ~CPlusTiles() = default;

        bool enabled() const;
        // Tiled, not fullscreen, not mini, not superposition, and a Plus Tile by rule or by plus:all_tiled.
        bool participates(PHLWINDOW window) const;
        bool isModified(PHLWINDOW window) const;

        // Move Edge
        Result beginEdit(PHLWINDOW window, std::optional<eEdge> edge = std::nullopt);
        Result toggleEdit(PHLWINDOW window);
        Result commitEdit();
        Result cancelEdit();
        Result selectEdge(eEdge edge);
        // Moves the selected edge by px (outwards when positive): into the preview, or at once without one.
        Result moveEdge(double px);
        // Moves an edge at once, outside an edit session (hl.dsp.window.resize_edge).
        Result moveEdgeNow(PHLWINDOW window, eEdge edge, double px);
        double stepPx(eStep step) const;
        bool   editing() const;

        // Fit to Neighbors, and back to an ordinary tile.
        Result fit(PHLWINDOW window);
        Result reset(PHLWINDOW window);

        // Focus the nearest tile in a direction, by real geometry. Falls back to the normal movefocus.
        Result focus(Math::eDirection direction);

        // Geometry inspector
        Result      toggleInspector();
        bool        inspecting() const;
        std::string inspect(PHLWINDOW window, bool json) const;
        std::string boardInfo(PHLWORKSPACE workspace, bool json) const;

        // Rendering, called by the renderer after a workspace's windows.
        void renderDragGuide(PHLMONITOR monitor, PHLWORKSPACE workspace, Render::CRenderContext& ctx);
        void render(PHLMONITOR monitor, PHLWORKSPACE workspace, Render::CRenderContext& ctx);

      private:
        struct SWindowState {
            PHLWINDOWREF          window;
            bool                  modified = false;
            std::optional<double> customW, customH;
            // the size before the first edge move, for reset
            std::optional<double> originalW, originalH;
            std::string           lastRefusal;
        };

        struct SFeedback {
            PHLWORKSPACEREF      workspace;
            SRect                box; // global, logical, the tile's layout box
            eFeedbackKind        kind = FEEDBACK_EDGE;
            std::optional<eEdge> edge;
            Time::steady_tp      start;
            int                  durationMs = 0; // 0: stays until its group is cleared
            int                  group      = 0; // GROUP_*
        };

        static constexpr int             GROUP_FLASH   = 0;
        static constexpr int             GROUP_PREVIEW = 1;
        static constexpr int             GROUP_INSPECT = 2;

        std::optional<CTileBoard>        board(PHLWORKSPACE workspace) const;
        double                           tolerance() const;
        PHLWINDOW                        windowById(PHLWORKSPACE workspace, uint64_t id) const;
        SWindowState*                    stateFor(PHLWINDOW window, bool create);
        const SWindowState*              stateFor(PHLWINDOW window) const;
        void                             prune();

        Result                           applyMove(PHLWINDOW window, eEdge edge, double px, bool markModified);
        Result                           restore(PHLWORKSPACE workspace, PHLWINDOW priority, bool interactive);
        void                             scheduleRestore(PHLWORKSPACE workspace);
        void                             runScheduledRestores();
        bool                             hasCustomTiles(PHLWORKSPACE workspace) const;

        void                             endEdit(bool commit);
        void                             updatePreview();
        void                             updateInspector();
        void                             flash(PHLWORKSPACE workspace, const SRect& box, eFeedbackKind kind, std::optional<eEdge> edge = std::nullopt);
        void                             clearGroup(int group);
        void                             damage(const SFeedback& feedback) const;
        SRect                            visualBox(const SRect& layoutBox, PHLWORKSPACE workspace) const;

        void                             notify(const std::string& text, bool failure) const;
        std::string                      nameOf(PHLWINDOW window) const;
        std::string                      withNames(std::string text, PHLWORKSPACE workspace) const;

        std::vector<SWindowState>        m_states;
        std::vector<SFeedback>           m_feedback;

        CEditSession                     m_edit;
        PHLWINDOWREF                     m_editWindow;
        std::string                      m_submapBeforeEdit;
        bool                             m_enteredSubmap  = false;
        bool                             m_changingSubmap = false;

        bool                             m_inspector        = false;
        bool                             m_restoring        = false;
        bool                             m_restoreScheduled = false;
        std::vector<PHLWORKSPACEREF>     m_pendingRestore;

        SRect                            m_lastGuide;
        PHLWORKSPACEREF                  m_lastGuideWorkspace;

        std::vector<CHyprSignalListener> m_listeners;
    };

    UP<CPlusTiles>& tiles();

    uint64_t        idOf(PHLWINDOW window);
}
