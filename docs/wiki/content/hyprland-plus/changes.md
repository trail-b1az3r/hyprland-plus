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
| `SUPER + ALT + F` | Fullscreen | Still fullscreen. The plustile fit is on `SUPER + CTRL + F` in the Halcyon dotfiles. |
| `hyprland --version` | `hyprland 0.56.2` | `hyprland-plus 0.56.2.p1`, from the `PLUS_VERSION` file |

## New in hyprland-plus

- **Mini windows**: `hl.dsp.window.mini`, `binds:mini_width`, `binds:mini_height`, the `mini` socket event, and `"mini"` in
  `hyprctl clients -j`. See [Mini windows](../mini-windows/).
- **Superposition windows**: the `superposition` window rule, and the `superposition:*` options. See
  [Superposition](../superposition/).
- **Plustile windows**: the `plustile` window rule, `hl.dsp.window.resize_edge` and `hl.dsp.window.fit_plustile`. See
  [Plustile](../plustile/).
- **Drag options**: `binds:drag_keep_grab_point` and `binds:drag_guide`. See [Drag](../drag/).
- **hyprctl**: `dots`, `store` and `load`. See [hyprctl additions](../hyprctl/).
- **Packaging**: an AUR package in `packaging/aur/`, and the `hyprctl-lite` installer. See [Install](../install/).

## Version numbers

Two numbers are kept on purpose:

- `VERSION` is `0.56.2`. CMake's `project()` needs a numeric version, so it stays numeric.
- `PLUS_VERSION` is `0.56.2.p1`. It is the version hyprland-plus reports.

## Build changes

- glaze is accepted at versions 7 and 8. nixpkgs ships 8, and the build uses only `glz::read_json` and `glz::generic`.
- `libeis-1.0` is the `libei` package on Arch.
- `WINDOW_RULE_EFFECT_LAST_STATIC` is 60, because the superposition and plustile effects were added after
  `sync_fullscreen`.

## Config compatibility

Existing configs keep working. The new options have defaults, and the new window rules and dispatchers are additions.
A Lua config that uses them won't load on stock Hyprland. To keep one config for both, check for the dispatcher first:

```lua
if hl.dsp and hl.dsp.window and hl.dsp.window.fit_plustile then
    hl.config({ superposition = { edge = 1, size = 0.12 } })
end
```

The binary, the config directory names and the IPC socket are unchanged.
