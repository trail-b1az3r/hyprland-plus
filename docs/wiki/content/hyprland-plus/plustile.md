---
weight: 60
title: Plustile windows
---

A plustile window is a tiled window that can be resized from each of its four edges. Only plustile windows can be
resized this way, so every other tile behaves as before.

## The rule

```lua
hl.window_rule({ match = { class = "^(kitty)$" }, plustile = true })
```

## Resizing an edge: `hl.dsp.window.resize_edge`

```lua
hl.bind("SUPER + ALT + Right", hl.dsp.window.resize_edge({ edge = "right", amount = 40 }))
```

- `edge` is `left`, `right`, `top` or `bottom`. It moves that one edge outwards by `amount` pixels. The neighbouring
  tiles give up that space.
- A negative `amount` moves the edge inwards.
- The other edges don't move, and the window stays tiled.

## Fitting to the neighbours: `hl.dsp.window.fit_plustile`

```lua
hl.bind("SUPER + CTRL + F", hl.dsp.window.fit_plustile(), { description = "Plustile: Fit to neighbours" })
```

Each edge is pushed outwards a step at a time, until the tiles on that side can give no more. `window` defaults to the
active window.

## The Halcyon bindings

The Halcyon dotfiles bind these as follows. The grow and shrink steps are 40 pixels.

| Keys | Action |
|---|---|
| `SUPER + ALT + arrow` | Grow that edge |
| `SUPER + ALT + SHIFT + arrow` | Shrink that edge |
| `SUPER + CTRL + F` | Fit to the neighbours |

`SUPER + ALT + F` is upstream's fullscreen bind, so fitting is on `SUPER + CTRL + F`.

## Source

- The rule: `src/desktop/rule/windowRule/`
- `resizeEdge` and `plustileFit` in `src/config/shared/actions/ConfigActions.cpp`
- `hl.dsp.window.resize_edge` and `hl.dsp.window.fit_plustile` in `src/config/lua/bindings/LuaBindingsDispatchers.cpp`
