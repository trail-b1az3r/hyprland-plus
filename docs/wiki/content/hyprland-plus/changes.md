---
weight: 20
title: Changes from upstream
---

hyprland-plus is Hyprland 0.56.2 (upstream tag `v0.56.2`) with the changes below. Everything else is upstream.

## Behaviour that differs

| Area | Upstream 0.56.2 | hyprland-plus |
|---|---|---|
| Dragging a tiled window that floats | Centres it on the cursor | Keeps the grab point. Set `binds:drag_keep_grab_point = false` to get the old behaviour. |
| Tile under the cursor during a drag | Nothing | Tinted, showing the swap target. Off with `binds:drag_guide = false`. |
| `SUPER + ALT + F` | Fullscreen | Still fullscreen. Fit to Neighbors is on `SUPER + CTRL + F` in the Halcyon dotfiles. |
| `hyprland --version` | `Hyprland 0.56.2` | `hyprland-plus 0.56.2.p2 (Hyprland 0.56.2)`, from the `PLUS_VERSION` file |

## New in hyprland-plus

- **Mini windows**: `hl.dsp.window.mini`, `binds:mini_width`, `binds:mini_height`, the `mini` socket event, and `"mini"` in
  `hyprctl clients -j`. See [Mini windows](../mini-windows/).
- **Superposition windows**: the `superposition` window rule, and the `superposition:*` options, among them
  `superposition:over_fullscreen` to keep the strip out of fullscreen windows. See
  [Superposition](../superposition/).
- **Plus Tiles**: the `plustile` and `plus_locked` window rules, `hl.dsp.plus.*` (Move Edge, Fit to Neighbors,
  reset, geometric focus, inspector), the `plus:*` options and `hyprctl plus`. See [Plus Tiles](../plustile/).
- **Drag options**: `binds:drag_keep_grab_point`, `binds:drag_guide` and `binds:drag_fullscreen_edge` (drop a dragged
  window at the top of the screen to fullscreen it, off by default). See [Drag](../drag/).
- **hyprctl**: `dots`, `store` (or `save`) and `load`. See [hyprctl additions](../hyprctl/).
- **Desk** (0.56.2.p2): desktop peek (`hl.dsp.plus.peek`), locate pointer and shake to find
  (`hl.dsp.plus.locate_pointer`, `plus:shake_to_find`), and the `hold_focus` window rule with `plus:game_hold_focus`.
  See [Desk](../desk/).
- **Mini all** (0.56.2.p2): `hl.dsp.plus.mini_all` and `plus:mini_all_gap`. See [Mini windows](../mini-windows/).
- **Packaging**: an AUR package in `packaging/aur/`, and the `hyprctl-lite` installer. See [Install](../install/).

## Version numbers

Two numbers are kept on purpose:

- `VERSION` is `0.56.2`. CMake's `project()` needs a numeric version, so it stays numeric.
- `PLUS_VERSION` is `0.56.2.p2`. It is the version hyprland-plus reports.

## Build changes

- glaze 7 or newer is accepted (nixpkgs ships 8, Arch 9). The build uses only `glz::read_json` and `glz::generic`.
- `libeis-1.0` is the `libei` package on Arch.
- `WINDOW_RULE_EFFECT_LAST_STATIC` is 62, because the superposition, plustile, plus_locked and hold_focus effects were added after
  `sync_fullscreen`.

## Config compatibility

Existing configs keep working. The new options have defaults, and the new window rules and dispatchers are additions.
A Lua config that uses them won't load on stock Hyprland. To keep one config for both, check for the dispatcher first:

```lua
if hl.dsp and hl.dsp.plus then
    hl.config({ superposition = { edge = 1, size = 0.12 } })
end
```

The binary, the config directory names and the IPC socket are unchanged.
