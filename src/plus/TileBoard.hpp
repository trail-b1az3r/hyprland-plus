#pragma once

// The Plus Tiles board: the tiled windows of one workspace as plain rectangles, and the geometry that the Plus
// features need from them. Nothing here touches the compositor, so it can be tested on its own.
//
//  - Adjacency: which tiles touch each edge of a tile, and how long the shared stretch is.
//  - Boundaries: the line an edge lies on, with every tile on either side of it that moves when the line moves.
//  - Edge moves: what a requested edge move would do to every affected tile, clamped to minimum and maximum
//    sizes and to the work area, with the reason when it is limited or refused.
//  - Restore plans: the edge moves that bring edge-modified tiles back to their custom sizes (Fit to Neighbors).
//  - Navigation: the nearest tile in a direction, using the real geometry.
//  - Validation: overlaps and tiles outside the work area.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Plus {

    struct SRect {
        double x = 0, y = 0, w = 0, h = 0;

        double right() const {
            return x + w;
        }
        double bottom() const {
            return y + h;
        }
        double centerX() const {
            return x + w / 2.0;
        }
        double centerY() const {
            return y + h / 2.0;
        }
    };

    enum eEdge : uint8_t {
        EDGE_LEFT = 0,
        EDGE_RIGHT,
        EDGE_TOP,
        EDGE_BOTTOM,
    };

    const char*          edgeName(eEdge edge);
    std::optional<eEdge> edgeFromString(std::string_view str);
    eEdge                oppositeEdge(eEdge edge);
    // Left and right edges move along x, and change widths. Top and bottom move along y, and change heights.
    bool isVerticalEdge(eEdge edge);

    struct STile {
        uint64_t id   = 0;
        SRect    rect = {};
        double   minW = 1, minH = 1;
        double   maxW = 0, maxH = 0; // 0: no maximum
        // A locked tile keeps its size: restore plans treat its current size as a custom size.
        bool locked = false;
        // Set once Move Edge has been used on the tile. Only modified tiles have custom sizes.
        bool                  modified = false;
        std::optional<double> customW, customH;
    };

    struct SNeighbor {
        uint64_t id     = 0;
        double   shared = 0; // length of the shared stretch of edge
    };

    // The line an edge lies on. Moving it outwards grows the near tiles and shrinks the far ones.
    struct SBoundary {
        eEdge                 edge  = EDGE_RIGHT; // as seen from the tile it was asked for
        double                coord = 0;          // x for left/right edges, y for top/bottom
        double                from = 0, to = 0;   // the stretch of the line the boundary covers
        std::vector<uint64_t> nearSide;           // tiles whose `edge` lies on the line
        std::vector<uint64_t> farSide;            // tiles whose opposite edge lies on the line
    };

    enum eLimit : uint8_t {
        LIMIT_NONE = 0,
        LIMIT_NO_NEIGHBOR, // nothing on the other side: the edge is at the work area's edge
        LIMIT_MIN_SIZE,    // a shrinking tile reached its minimum
        LIMIT_MAX_SIZE,    // a growing tile reached its maximum
        LIMIT_UNKNOWN_TILE,
    };

    struct SEdgeMove {
        bool                                    ok        = false; // something moved, possibly less than asked
        double                                  requested = 0;
        double                                  applied   = 0;
        eLimit                                  limit     = LIMIT_NONE;
        std::optional<uint64_t>                 limitedBy;      // the tile whose size limited the move
        double                                  limitValue = 0; // that tile's minimum or maximum, in px
        std::vector<std::pair<uint64_t, SRect>> rects;          // proposed rectangles of every affected tile
        SBoundary                               boundary;

        // A sentence for logs, notifications and the inspector. Tile ids are written as #id.
        std::string describe() const;
    };

    struct SRestoreStep {
        uint64_t id    = 0;
        eEdge    edge  = EDGE_RIGHT;
        double   delta = 0; // outwards when positive
    };

    struct SUnsatisfied {
        uint64_t    id     = 0;
        double      errorW = 0, errorH = 0; // custom size minus the size the plan could reach
        std::string reason;
    };

    struct SRestorePlan {
        std::vector<SRestoreStep>               steps;
        std::vector<std::pair<uint64_t, SRect>> finalRects; // the board after the steps, every tile
        std::vector<SUnsatisfied>               unsatisfied;
        int                                     passes = 0;
    };

    class CTileBoard {
      public:
        // tolerance: how far apart two edges can be and still count as touching. It must cover the gap between
        // tiles (two inner gaps) or nothing will look adjacent.
        CTileBoard(std::vector<STile> tiles, SRect bounds, double tolerance);

        const std::vector<STile>& tiles() const;
        const STile*              tile(uint64_t id) const;
        const SRect&              bounds() const;
        double                    tolerance() const;

        std::vector<SNeighbor>    neighbors(uint64_t id, eEdge edge) const;
        std::optional<SBoundary>  boundary(uint64_t id, eEdge edge) const;
        // An edge can be moved when another tile is on its other side.
        bool      adjustable(uint64_t id, eEdge edge) const;

        SEdgeMove proposeEdgeMove(uint64_t id, eEdge edge, double delta) const;
        // Apply a proposal to this board. Used by restore planning and by tests.
        void                     apply(const SEdgeMove& move);

        SRestorePlan             planRestore(std::optional<uint64_t> priority = std::nullopt, int maxPasses = 8) const;

        std::optional<uint64_t>  neighborInDirection(uint64_t id, eEdge direction) const;

        std::vector<std::string> validate() const;

      private:
        STile*                mutableTile(uint64_t id);
        double                sizeAlong(const STile& t, eEdge edge) const;
        std::optional<double> customAlong(const STile& t, eEdge edge) const;

        std::vector<STile>    m_tiles;
        SRect                 m_bounds;
        double                m_tolerance = 4;
    };
}
