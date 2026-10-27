---
weight: 30
title: Drag
---

## The grab point: `binds:drag_keep_grab_point`

Upstream centres a tiled or fullscreen window on the cursor when the drag makes it floating. The window jumps. This
happens with apps that draw their own top bar and ask Hyprland to move them, such as Firefox and Dolphin.

- **Type:** boolean. **Default:** `true` in hyprland-plus.
- **`false`:** upstream's behaviour. The window is centred on the cursor.
- **`true`:** the window keeps the exact pixel offset between the cursor and the window. If that offset is larger than
  the new, floating window, it is clamped to the window's edge.

```lua
hl.config({ binds = { drag_keep_grab_point = true } })
```

```
binds {
    drag_keep_grab_point = true
}
```

`drag_keep_grab_point` takes precedence over `drag_center_window`, which is upstream's centring option.

## The guide: `binds:drag_guide`

While a tiled window is dragged, the tile under the cursor is highlighted: filled with `plus:feedback:placement_color`
and outlined with it at full strength. That is the tile the dragged window swaps with when it is dropped. The
highlight is drawn above the windows, in the monitor's own coordinates (offset and scale), follows workspace
animations, and goes away as soon as the drag ends or the cursor leaves the tile.

- **Type:** boolean. **Default:** `true`.

Earlier versions never showed it: a tiled window becomes floating as soon as it is dragged, and the guide was skipped
for floating windows. It now checks whether the dragged window was tiled before the drag.

```lua
hl.config({ binds = { drag_guide = false } })   -- turn the tint off
```

Not done yet: a half-tile guide that splits the tile on drop. Dropping still swaps the two tiles.

## Dropping at the top: `binds:drag_fullscreen_edge`

Drop a window at the top edge of the screen and it becomes fullscreen, as in most desktops' window management.

- **Type:** integer, in pixels. **Default:** `0`, which is off.
- When a move drag (`bindm` with `movewindow`) ends with the cursor within this many pixels of the top of the monitor
  under it, the window is made fullscreen. A tiled window is tiled again first, so leaving fullscreen puts it back.
- The drag must have started below that strip and moved at least 8 pixels, so a click, or a window that already
  sits at the top of the screen, is not fullscreened by letting go of it.
- Resize drags are not affected.

```lua
hl.config({ binds = { drag_fullscreen_edge = 8 } })
```

## Source

- `binds:drag_keep_grab_point`, `binds:drag_guide` and `binds:drag_fullscreen_edge`: `src/config/values/ConfigValues.cpp`
- The drop: `droppedAtTopEdge` and `CDragStateController::dragEnd` in `src/layout/supplementary/DragController.cpp`
- The guide: `CPlusTiles::renderDragGuide` in `src/plus/PlusTiles.cpp`
- `floatingGrab`: `src/layout/supplementary/DragController.cpp`
- The guide: `renderDragGuide` in `src/render/Renderer.cpp`

## The wall between monitors: `plus:monitor_wall`

Drag a window against an edge it shares with another monitor and it stops there, as if against something elastic.
Push on, and once the window would be this many pixels past the edge, the wall gives and the window goes through.
Pull it back inside its monitor and the wall stands again. Edges with no monitor beyond them have no wall.

- **Type:** integer, in pixels. **Default:** `120`. `0` turns the wall off.
- Move drags of floating windows (and of tiled windows once they float for the drag) only.
- While a window is held, the `monitorwall` [event](../../ipc/) says where it would come in on the other monitor, so
  a shell can draw the wall bulging there: `MONITORNAME,EDGE,START,LENGTH,STRENGTH`. `EDGE` is that monitor's edge
  (`left`, `right`, `top`, `bottom`), `START` and `LENGTH` where along it the window's side is, in the monitor's own
  logical pixels, and `STRENGTH` how far into the wall it is, from `0` to `1`. `MONITORNAME,none,0,0,0` when the
  push ends, and `monitorwallbreak` (`MONITORNAME`) when the wall gives. Halcyon draws the bulge.

```lua
hl.config({ plus = { monitor_wall = 160 } })   -- a firmer wall
```
