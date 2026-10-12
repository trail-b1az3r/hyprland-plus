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

While a tiled window is dragged, the tile under the cursor is tinted. That is the tile the dragged window swaps with
when it is dropped.

- **Type:** boolean. **Default:** `true`.

```lua
hl.config({ binds = { drag_guide = false } })   -- turn the tint off
```

Not done yet: a half-tile guide that splits the tile on drop. Dropping still swaps the two tiles.

## Source

- `binds:drag_keep_grab_point` and `binds:drag_guide`: `src/config/values/ConfigValues.cpp`
- `floatingGrab`: `src/layout/supplementary/DragController.cpp`
- The guide: `renderDragGuide` in `src/render/Renderer.cpp`
