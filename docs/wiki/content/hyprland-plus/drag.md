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

## Source

- `binds:drag_keep_grab_point` and `binds:drag_guide`: `src/config/values/ConfigValues.cpp`
- The guide: `CPlusTiles::renderDragGuide` in `src/plus/PlusTiles.cpp`
- `floatingGrab`: `src/layout/supplementary/DragController.cpp`
- The guide: `renderDragGuide` in `src/render/Renderer.cpp`
