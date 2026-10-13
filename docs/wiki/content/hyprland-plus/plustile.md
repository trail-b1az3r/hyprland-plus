---
weight: 60
title: Plus Tiles
---

A Plus Tile is a tiled window that can be given an intentionally unequal size. It behaves like every other tiled
window until you use **Move Edge** on it: the active layout places and sizes it, and focus, moving and resizing work
as normal. Nothing about the layout changes when a Plus Tile opens.

## Which windows are Plus Tiles

```lua
hl.window_rule({ match = { class = "^(kitty)$" }, plustile = true })
```

Or every tiled window, with `plus:all_tiled = true`. Floating, fullscreen, mini and superposition windows are never
Plus Tiles.

## Move Edge

Move Edge starts an edit session on the focused Plus Tile. In it you select an edge, move it in steps, look at the
preview, and then apply or cancel.

- The selected edge is drawn as a bar. The tile's proposed size, and every neighbour the move resizes, are drawn as
  a preview (`plus:feedback:*`).
- A move that a neighbour's minimum size, a maximum size or the work area stops is cut to what fits, flashed in
  the failed colour, and explained in a notification.
- **Commit** applies the moves through the layout: the tiles that share the moved boundary give up or take the space,
  so the board stays aligned. The window is never turned into a float.
- **Cancel**, or focus moving to another window, the workspace changing, or the tile closing, floating or going
  fullscreen, ends the session with nothing applied. The preview goes away at once.

With `plus:edit_preview = false`, every step is applied as it is made.

While editing, the compositor switches to the submap named in `plus:edit_submap` (default `plus-edit`), and back
when the session ends. Define the edit keys in that submap:

```lua
hl.bind("SUPER + ALT + E", hl.dsp.plus.edit({ action = "toggle" }), { description = "Plus: Move edge" })

hl.define_submap("plus-edit", function()
    hl.bind("Left",  hl.dsp.plus.select_edge("left"))
    hl.bind("Right", hl.dsp.plus.select_edge("right"))
    hl.bind("Up",    hl.dsp.plus.select_edge("top"))
    hl.bind("Down",  hl.dsp.plus.select_edge("bottom"))
    hl.bind("Equal", hl.dsp.plus.move_edge({ direction = "out" }))
    hl.bind("Minus", hl.dsp.plus.move_edge({ direction = "in" }))
    hl.bind("SHIFT + Equal", hl.dsp.plus.move_edge({ direction = "out", step = "coarse" }))
    hl.bind("CTRL + Minus",  hl.dsp.plus.move_edge({ direction = "in", step = "fine" }))
    hl.bind("Return", hl.dsp.plus.commit())
    hl.bind("Escape", hl.dsp.plus.cancel())
end)
```

If the submap is not defined, the session still works with keys bound globally, and the log says so.

Once an edge has been moved, the tile is **edge-modified**. It remembers its custom width (left or right edge moves)
and height (top or bottom edge moves).

## Fit to Neighbors

`hl.dsp.plus.fit()` does two different things:

| Tile | Fit to Neighbors |
|---|---|
| Ordinary (Move Edge never used) | Fits the space the layout assigns, by the layout's own rules. No custom size is kept. |
| Edge-modified | Keeps the custom size, and moves the boundaries around it so the neighbours fill the rest. Other tiles are not made equal, and other custom sizes are kept where possible. |

With `plus:auto_restore` (on by default), edge-modified tiles get their custom size back after windows open, close or
move to another workspace, and after the monitor's resolution, scale or arrangement changes.

When custom sizes cannot all be met, the tile that asked first wins, and the others are told why, for example
"kitty is at its minimum width (120 px)". Restoring stops after a few rounds if it stops making progress, so two
custom sizes that fight each other never make the layout oscillate.

`hl.dsp.plus.reset()` turns an edge-modified tile back into an ordinary one, at the size it had before its first edge
move.

## How boundaries are found

The board is the workspace's tiled windows as rectangles. Two tiles are neighbours when their facing edges are within
the inner gaps of each other and share part of their length. Moving an edge moves its whole **boundary**: every tile
on either side of that line that really shares part of it, found by alternating sides. So:

- a small tile beside two stacked windows resizes both of them;
- two stacked windows beside one tile move together;
- two separate splits that only happen to line up are not moved together.

Edges at the work area's edge, with no tile beyond them, cannot be moved.

## Keep a tile's size: `plus_locked`

```lua
hl.window_rule({ match = { class = "^(mpv)$" }, plustile = true, plus_locked = true })
```

A locked tile keeps its current size during Fit to Neighbors and auto-restore: other tiles are resized around it.

## Navigation

`hl.dsp.plus.focus("left")` focuses the nearest tile in that direction by real geometry: the tile beside this one
that shares most of the edge, then the most centred. It works on unequal, asymmetric boards. With no tile that way it
falls back to the normal `movefocus`, which handles other monitors and floating windows. `plus:focus_preview` flashes
the target.

## Inspector

`hl.dsp.plus.inspect()` toggles the geometry inspector: every tile is outlined, and the focused tile's adjustable edges
and neighbours are marked. A notification, and `hyprctl plus inspect [window]` (`-j` for JSON), show:

- geometry, minimum and maximum size, and the custom size;
- the neighbours on each edge, and how much of the edge each shares;
- which other tiles move with each edge;
- the state: ordinary, edge-modified, locked;
- the edit session's pending moves;
- the last refusal, and why: a minimum size, the work area, or a layout that does not resize that way;
- overlaps and other board problems.

`hyprctl plus board` gives the same for every tile on the workspace.

## Older dispatchers

`hl.dsp.window.resize_edge({ edge, amount })` moves an edge at once, outside an edit session, and marks the tile
edge-modified. `hl.dsp.window.fit_plustile()` is Fit to Neighbors.

## Source

- The board, its boundaries, the solver and navigation: `src/plus/TileBoard.{hpp,cpp}`, tested in
  `tests/plus/TileBoard.cpp`
- The edit session and highlight geometry: `src/plus/EditSession.{hpp,cpp}`, tested in `tests/plus/EditSession.cpp`
- The compositor side (layout, events, rendering, inspector): `src/plus/PlusTiles.{hpp,cpp}`
- The Lua dispatchers: `hl.dsp.plus.*` in `src/config/lua/bindings/LuaBindingsDispatchers.cpp`
