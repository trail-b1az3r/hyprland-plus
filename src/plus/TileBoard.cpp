#include "TileBoard.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <tuple>

using namespace Plus;

namespace {
    // Positive overlap of two stretches [a0, a1] and [b0, b1]. Negative when they are apart.
    double overlap(double a0, double a1, double b0, double b1) {
        return std::min(a1, b1) - std::max(a0, b0);
    }

    // Where an edge of a rectangle lies: x for left and right edges, y for top and bottom.
    double edgeCoord(const SRect& r, eEdge edge) {
        switch (edge) {
            case EDGE_LEFT: return r.x;
            case EDGE_RIGHT: return r.right();
            case EDGE_TOP: return r.y;
            case EDGE_BOTTOM: return r.bottom();
        }
        return 0;
    }

    // The stretch an edge covers, across the direction it moves in.
    std::pair<double, double> edgeSpan(const SRect& r, eEdge edge) {
        if (isVerticalEdge(edge))
            return {r.y, r.bottom()};
        return {r.x, r.right()};
    }

    // Moves `edge` of `r` outwards by `d` (inwards when negative).
    SRect moveEdge(SRect r, eEdge edge, double d) {
        switch (edge) {
            case EDGE_LEFT:
                r.x -= d;
                r.w += d;
                break;
            case EDGE_RIGHT: r.w += d; break;
            case EDGE_TOP:
                r.y -= d;
                r.h += d;
                break;
            case EDGE_BOTTOM: r.h += d; break;
        }
        return r;
    }

    constexpr double MIN_OVERLAP = 1.0; // tiles must share more than this to count as neighbours
    constexpr double MIN_MOVE    = 0.5; // moves smaller than this did not happen

    std::string      fmtPx(double v) {
        return std::to_string(std::lround(v)) + " px";
    }
}

const char* Plus::edgeName(eEdge edge) {
    switch (edge) {
        case EDGE_LEFT: return "left";
        case EDGE_RIGHT: return "right";
        case EDGE_TOP: return "top";
        case EDGE_BOTTOM: return "bottom";
    }
    return "?";
}

std::optional<eEdge> Plus::edgeFromString(std::string_view str) {
    if (str == "left" || str == "l")
        return EDGE_LEFT;
    if (str == "right" || str == "r")
        return EDGE_RIGHT;
    if (str == "top" || str == "up" || str == "t" || str == "u")
        return EDGE_TOP;
    if (str == "bottom" || str == "down" || str == "b" || str == "d")
        return EDGE_BOTTOM;
    return std::nullopt;
}

eEdge Plus::oppositeEdge(eEdge edge) {
    switch (edge) {
        case EDGE_LEFT: return EDGE_RIGHT;
        case EDGE_RIGHT: return EDGE_LEFT;
        case EDGE_TOP: return EDGE_BOTTOM;
        case EDGE_BOTTOM: return EDGE_TOP;
    }
    return EDGE_RIGHT;
}

bool Plus::isVerticalEdge(eEdge edge) {
    return edge == EDGE_LEFT || edge == EDGE_RIGHT;
}

std::string SEdgeMove::describe() const {
    const std::string EDGE = edgeName(boundary.edge);
    const std::string SIZE = isVerticalEdge(boundary.edge) ? "width" : "height";

    std::string       limitText;
    switch (limit) {
        case LIMIT_NONE: break;
        case LIMIT_NO_NEIGHBOR: limitText = "the " + EDGE + " edge is at the edge of the work area, and no tile is on that side to give up space"; break;
        case LIMIT_MIN_SIZE:
            limitText = (limitedBy ? "#" + std::to_string(*limitedBy) : std::string("a tile")) + " is at its minimum " + SIZE + " (" + fmtPx(limitValue) + ")";
            break;
        case LIMIT_MAX_SIZE:
            limitText = (limitedBy ? "#" + std::to_string(*limitedBy) : std::string("a tile")) + " is at its maximum " + SIZE + " (" + fmtPx(limitValue) + ")";
            break;
        case LIMIT_UNKNOWN_TILE: limitText = "the tile is not on this board"; break;
    }

    if (!ok)
        return "Cannot move the " + EDGE + " edge: " + (limitText.empty() ? std::string("nothing to move") : limitText) + ".";

    std::string text = "Moved the " + EDGE + " edge " + (applied > 0 ? "out" : "in") + " by " + fmtPx(std::abs(applied));
    if (!limitText.empty())
        text += " of " + fmtPx(std::abs(requested)) + ": " + limitText;
    return text + ".";
}

CTileBoard::CTileBoard(std::vector<STile> tiles, SRect bounds, double tolerance) : m_tiles(std::move(tiles)), m_bounds(bounds), m_tolerance(std::max(tolerance, 1.0)) {
    ;
}

const std::vector<STile>& CTileBoard::tiles() const {
    return m_tiles;
}

const STile* CTileBoard::tile(uint64_t id) const {
    for (const auto& t : m_tiles) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

STile* CTileBoard::mutableTile(uint64_t id) {
    for (auto& t : m_tiles) {
        if (t.id == id)
            return &t;
    }
    return nullptr;
}

const SRect& CTileBoard::bounds() const {
    return m_bounds;
}

double CTileBoard::tolerance() const {
    return m_tolerance;
}

double CTileBoard::sizeAlong(const STile& t, eEdge edge) const {
    return isVerticalEdge(edge) ? t.rect.w : t.rect.h;
}

std::optional<double> CTileBoard::customAlong(const STile& t, eEdge edge) const {
    return isVerticalEdge(edge) ? t.customW : t.customH;
}

std::vector<SNeighbor> CTileBoard::neighbors(uint64_t id, eEdge edge) const {
    std::vector<SNeighbor> result;
    const auto*            T = tile(id);
    if (!T)
        return result;

    const double LINE    = edgeCoord(T->rect, edge);
    const auto [s0, s1]  = edgeSpan(T->rect, edge);
    const eEdge OPPOSITE = oppositeEdge(edge);

    for (const auto& o : m_tiles) {
        if (o.id == id)
            continue;
        if (std::abs(edgeCoord(o.rect, OPPOSITE) - LINE) > m_tolerance)
            continue;
        const auto [o0, o1] = edgeSpan(o.rect, OPPOSITE);
        const double SHARED = overlap(s0, s1, o0, o1);
        if (SHARED > MIN_OVERLAP)
            result.push_back({.id = o.id, .shared = SHARED});
    }

    std::ranges::sort(result, [edge, this](const SNeighbor& a, const SNeighbor& b) {
        // along the edge, from top to bottom (or left to right)
        const auto* A = tile(a.id);
        const auto* B = tile(b.id);
        return edgeSpan(A->rect, edge).first < edgeSpan(B->rect, edge).first;
    });
    return result;
}

std::optional<SBoundary> CTileBoard::boundary(uint64_t id, eEdge edge) const {
    const auto* T = tile(id);
    if (!T)
        return std::nullopt;

    SBoundary b;
    b.edge                 = edge;
    b.coord                = edgeCoord(T->rect, edge);
    std::tie(b.from, b.to) = edgeSpan(T->rect, edge);
    b.nearSide.push_back(id);

    const eEdge OPPOSITE = oppositeEdge(edge);

    // Grow the boundary by alternating sides: far tiles that really overlap a near tile, then near tiles that really
    // overlap a far tile, and so on. Tiles that only line up with the edge, without sharing any of it, are left out,
    // so two separate splits that happen to sit at the same x are not moved together.
    auto contains = [](const std::vector<uint64_t>& v, uint64_t x) { return std::ranges::find(v, x) != v.end(); };

    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto& o : m_tiles) {
            if (contains(b.nearSide, o.id) || contains(b.farSide, o.id))
                continue;

            const auto [o0, o1] = edgeSpan(o.rect, edge);

            if (std::abs(edgeCoord(o.rect, OPPOSITE) - b.coord) <= m_tolerance) {
                for (const auto nid : b.nearSide) {
                    const auto [n0, n1] = edgeSpan(tile(nid)->rect, edge);
                    if (overlap(n0, n1, o0, o1) > MIN_OVERLAP) {
                        b.farSide.push_back(o.id);
                        grew = true;
                        break;
                    }
                }
            } else if (std::abs(edgeCoord(o.rect, edge) - b.coord) <= m_tolerance) {
                for (const auto fid : b.farSide) {
                    const auto [f0, f1] = edgeSpan(tile(fid)->rect, edge);
                    if (overlap(f0, f1, o0, o1) > MIN_OVERLAP) {
                        b.nearSide.push_back(o.id);
                        grew = true;
                        break;
                    }
                }
            }

            if (grew) {
                b.from = std::min(b.from, o0);
                b.to   = std::max(b.to, o1);
            }
        }
    }

    return b;
}

bool CTileBoard::adjustable(uint64_t id, eEdge edge) const {
    const auto b = boundary(id, edge);
    return b && !b->farSide.empty();
}

SEdgeMove CTileBoard::proposeEdgeMove(uint64_t id, eEdge edge, double delta) const {
    SEdgeMove move;
    move.requested = delta;

    const auto b = boundary(id, edge);
    if (!b) {
        move.limit = LIMIT_UNKNOWN_TILE;
        return move;
    }
    move.boundary = *b;

    if (b->farSide.empty()) {
        move.limit = LIMIT_NO_NEIGHBOR;
        return move;
    }

    if (std::abs(delta) < MIN_MOVE)
        return move;

    const bool OUTWARDS = delta > 0;
    const bool VERTICAL = isVerticalEdge(edge);

    // How far the line can go before some tile leaves its size range.
    double room = std::numeric_limits<double>::infinity();

    auto   limitBy = [&](const STile& t, double r, eLimit why, double value) {
        r = std::max(r, 0.0);
        if (r < room) {
            room            = r;
            move.limit      = why;
            move.limitedBy  = t.id;
            move.limitValue = value;
        }
    };

    for (const auto nid : b->nearSide) {
        const auto&  t   = *tile(nid);
        const double SZ  = VERTICAL ? t.rect.w : t.rect.h;
        const double MIN = VERTICAL ? t.minW : t.minH;
        const double MAX = VERTICAL ? t.maxW : t.maxH;
        if (OUTWARDS && MAX > 0)
            limitBy(t, MAX - SZ, LIMIT_MAX_SIZE, MAX);
        else if (!OUTWARDS)
            limitBy(t, SZ - MIN, LIMIT_MIN_SIZE, MIN);
    }
    for (const auto fid : b->farSide) {
        const auto&  t   = *tile(fid);
        const double SZ  = VERTICAL ? t.rect.w : t.rect.h;
        const double MIN = VERTICAL ? t.minW : t.minH;
        const double MAX = VERTICAL ? t.maxW : t.maxH;
        if (OUTWARDS)
            limitBy(t, SZ - MIN, LIMIT_MIN_SIZE, MIN);
        else if (MAX > 0)
            limitBy(t, MAX - SZ, LIMIT_MAX_SIZE, MAX);
    }

    const double MAGNITUDE = std::min(std::abs(delta), room);
    move.applied           = OUTWARDS ? MAGNITUDE : -MAGNITUDE;

    if (MAGNITUDE < MIN_MOVE) {
        move.applied = 0;
        return move; // ok stays false, the limit says why
    }

    move.ok = true;
    if (MAGNITUDE >= std::abs(delta) - MIN_MOVE) {
        move.limit = LIMIT_NONE;
        move.limitedBy.reset();
        move.limitValue = 0;
    }

    for (const auto nid : b->nearSide)
        move.rects.emplace_back(nid, moveEdge(tile(nid)->rect, edge, move.applied));
    for (const auto fid : b->farSide)
        move.rects.emplace_back(fid, moveEdge(tile(fid)->rect, oppositeEdge(edge), -move.applied));

    return move;
}

void CTileBoard::apply(const SEdgeMove& move) {
    if (!move.ok)
        return;
    for (const auto& [id, rect] : move.rects) {
        if (auto* t = mutableTile(id))
            t->rect = rect;
    }
}

SRestorePlan CTileBoard::planRestore(std::optional<uint64_t> priority, int maxPasses) const {
    SRestorePlan plan;
    CTileBoard   sim = *this;

    // Locked tiles keep the size they have now.
    for (auto& t : sim.m_tiles) {
        if (t.locked) {
            if (!t.customW)
                t.customW = t.rect.w;
            if (!t.customH)
                t.customH = t.rect.h;
        }
    }

    auto hasCustom = [](const STile& t) { return (t.modified || t.locked) && (t.customW || t.customH); };

    // Order: the priority tile first, then the rest as they appear. Earlier tiles win when two can't both be met.
    std::vector<uint64_t> order;
    if (priority && sim.tile(*priority) && hasCustom(*sim.tile(*priority)))
        order.push_back(*priority);
    for (const auto& t : sim.m_tiles) {
        if (hasCustom(t) && (!priority || t.id != *priority))
            order.push_back(t.id);
    }

    auto totalError = [&]() {
        double e = 0;
        for (const auto id : order) {
            const auto& t = *sim.tile(id);
            if (t.customW)
                e += std::abs(*t.customW - t.rect.w);
            if (t.customH)
                e += std::abs(*t.customH - t.rect.h);
        }
        return e;
    };

    auto isOtherCustom = [&](uint64_t id, uint64_t self) {
        const auto* t = sim.tile(id);
        return id != self && t && hasCustom(*t);
    };

    std::vector<std::pair<uint64_t, std::string>> lastReason;
    auto                                          setReason = [&](uint64_t id, std::string r) {
        for (auto& [i, s] : lastReason) {
            if (i == id) {
                s = std::move(r);
                return;
            }
        }
        lastReason.emplace_back(id, std::move(r));
    };

    double previous = totalError();
    for (int pass = 0; pass < maxPasses && previous > m_tolerance / 2.0; ++pass) {
        plan.passes = pass + 1;

        for (const auto id : order) {
            for (const bool WIDTH : {true, false}) {
                const auto& t      = *sim.tile(id);
                const auto  CUSTOM = WIDTH ? t.customW : t.customH;
                if (!CUSTOM)
                    continue;

                const double DIFF = *CUSTOM - (WIDTH ? t.rect.w : t.rect.h);
                if (std::abs(DIFF) <= m_tolerance / 2.0)
                    continue;

                // Prefer the edge whose move disturbs no other custom-sized tile. Right and bottom first.
                const std::array<eEdge, 2> EDGES = WIDTH ? std::array<eEdge, 2>{EDGE_RIGHT, EDGE_LEFT} : std::array<eEdge, 2>{EDGE_BOTTOM, EDGE_TOP};
                std::optional<eEdge>       best;
                size_t                     bestCost = std::numeric_limits<size_t>::max();
                for (const auto E : EDGES) {
                    const auto B = sim.boundary(id, E);
                    if (!B || B->farSide.empty())
                        continue;
                    size_t cost = 0;
                    for (const auto o : B->nearSide)
                        cost += isOtherCustom(o, id);
                    for (const auto o : B->farSide)
                        cost += isOtherCustom(o, id);
                    if (cost < bestCost) {
                        bestCost = cost;
                        best     = E;
                    }
                }

                if (!best) {
                    setReason(id, std::string("no tile beside it on either ") + (WIDTH ? "side" : "end") + " can give up or take space");
                    continue;
                }

                // Tiles earlier in the order keep what they have reached: they may move towards their custom size,
                // never away from it. Proposals are made on a copy with their size range narrowed that way.
                CTileBoard view = sim;
                for (const auto other : order) {
                    if (other == id)
                        break;
                    auto*      o  = view.mutableTile(other);
                    const auto OC = WIDTH ? o->customW : o->customH;
                    if (!OC)
                        continue;
                    const double CUR = WIDTH ? o->rect.w : o->rect.h;
                    double&      mn  = WIDTH ? o->minW : o->minH;
                    double&      mx  = WIDTH ? o->maxW : o->maxH;
                    mn               = std::max(mn, std::min(CUR, *OC));
                    mx               = mx > 0 ? std::min(mx, std::max(CUR, *OC)) : std::max(CUR, *OC);
                }

                const auto MOVE = view.proposeEdgeMove(id, *best, DIFF);
                if (!MOVE.ok || MOVE.limit != LIMIT_NONE) {
                    const bool BY_PRIORITY =
                        MOVE.limitedBy && *MOVE.limitedBy != id && std::ranges::find(order.begin(), std::ranges::find(order, id), *MOVE.limitedBy) != std::ranges::find(order, id);
                    if (BY_PRIORITY)
                        setReason(id, "#" + std::to_string(*MOVE.limitedBy) + " comes first and keeps its custom " + (WIDTH ? "width" : "height"));
                    else
                        setReason(id, MOVE.describe());
                }
                if (!MOVE.ok)
                    continue;

                sim.apply(MOVE);
                plan.steps.push_back({.id = id, .edge = *best, .delta = MOVE.applied});
            }
        }

        const double NOW = totalError();
        // No progress means the custom sizes fight each other. Stop instead of going round in circles.
        if (NOW >= previous - MIN_MOVE)
            break;
        previous = NOW;
    }

    for (const auto id : order) {
        const auto&  t  = *sim.tile(id);
        const double EW = t.customW ? *t.customW - t.rect.w : 0;
        const double EH = t.customH ? *t.customH - t.rect.h : 0;
        if (std::abs(EW) <= m_tolerance / 2.0 && std::abs(EH) <= m_tolerance / 2.0)
            continue;

        std::string reason;
        for (const auto& [i, s] : lastReason) {
            if (i == id)
                reason = s;
        }
        if (reason.empty())
            reason = "another custom-sized tile with a higher priority needs the same space";
        plan.unsatisfied.push_back({.id = id, .errorW = EW, .errorH = EH, .reason = reason});
    }

    for (const auto& t : sim.m_tiles)
        plan.finalRects.emplace_back(t.id, t.rect);

    return plan;
}

std::optional<uint64_t> CTileBoard::neighborInDirection(uint64_t id, eEdge direction) const {
    const auto* T = tile(id);
    if (!T)
        return std::nullopt;

    const double LINE     = edgeCoord(T->rect, direction);
    const auto [s0, s1]   = edgeSpan(T->rect, direction);
    const double CENTER   = (s0 + s1) / 2.0;
    const eEdge  OPPOSITE = oppositeEdge(direction);
    const bool   POSITIVE = direction == EDGE_RIGHT || direction == EDGE_BOTTOM;

    struct SCandidate {
        uint64_t id;
        double   distance, shared, centerOffset;
    };
    std::optional<SCandidate> best;

    for (const auto& o : m_tiles) {
        if (o.id == id)
            continue;

        const double NEAR = edgeCoord(o.rect, OPPOSITE);
        const double DIST = POSITIVE ? NEAR - LINE : LINE - NEAR;
        if (DIST < -m_tolerance)
            continue; // not in that direction

        const auto [o0, o1] = edgeSpan(o.rect, direction);
        const double SHARED = overlap(s0, s1, o0, o1);
        const double OFFSET = std::abs((o0 + o1) / 2.0 - CENTER);
        // Tiles beside this one are always better than tiles that are only diagonally in that direction.
        const double     DISTANCE = SHARED > MIN_OVERLAP ? std::max(DIST, 0.0) : std::max(DIST, 0.0) + std::abs(SHARED) + 1e6;

        const SCandidate C{.id = o.id, .distance = DISTANCE, .shared = SHARED, .centerOffset = OFFSET};
        if (!best) {
            best = C;
            continue;
        }

        // Nearest first. Within the gap tolerance, the one sharing most of the edge, then the most centred.
        if (C.distance < best->distance - m_tolerance)
            best = C;
        else if (std::abs(C.distance - best->distance) <= m_tolerance) {
            if (C.shared > best->shared + MIN_OVERLAP || (std::abs(C.shared - best->shared) <= MIN_OVERLAP && C.centerOffset < best->centerOffset))
                best = C;
        }
    }

    if (!best)
        return std::nullopt;
    return best->id;
}

std::vector<std::string> CTileBoard::validate() const {
    std::vector<std::string> issues;

    for (size_t i = 0; i < m_tiles.size(); ++i) {
        const auto& a = m_tiles[i];

        if (a.rect.x < m_bounds.x - m_tolerance || a.rect.y < m_bounds.y - m_tolerance || a.rect.right() > m_bounds.right() + m_tolerance ||
            a.rect.bottom() > m_bounds.bottom() + m_tolerance)
            issues.push_back("#" + std::to_string(a.id) + " extends past the work area");

        if (a.rect.w < a.minW - MIN_MOVE || a.rect.h < a.minH - MIN_MOVE)
            issues.push_back("#" + std::to_string(a.id) + " is smaller than its minimum size");

        for (size_t j = i + 1; j < m_tiles.size(); ++j) {
            const auto&  b  = m_tiles[j];
            const double OX = overlap(a.rect.x, a.rect.right(), b.rect.x, b.rect.right());
            const double OY = overlap(a.rect.y, a.rect.bottom(), b.rect.y, b.rect.bottom());
            if (OX > m_tolerance && OY > m_tolerance)
                issues.push_back("#" + std::to_string(a.id) + " and #" + std::to_string(b.id) + " overlap by " + fmtPx(OX) + " x " + fmtPx(OY));
        }
    }

    return issues;
}
