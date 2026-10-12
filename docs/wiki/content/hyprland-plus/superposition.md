---
weight: 50
title: Superposition windows
---

A superposition window stays tiled, but it is drawn above a fullscreen floating window instead of being hidden by it.
Use it for a small tiled window that should stay visible while another window is fullscreen, such as a music player.

## The rule

```lua
hl.window_rule({ match = { class = "^([Ss]potify)$" }, superposition = true })
```

## Where it goes

A superposition window is kept out of the tile tree. It sits on a thin strip at one edge of the screen. All the
superposition windows on a workspace share the strip, stacked along the edge.

## Options

| Option | Default | Meaning |
|---|---|---|
| `superposition:edge` | `1` | Which edge the strip is on: `0` left, `1` right, `2` top, `3` bottom. Changing it re-lays the layout. |
| `superposition:size` | `0.12` | The strip's width (left or right) or height (top or bottom), as a fraction of the screen, from `0.02` to `0.5`. |
| `superposition:reserve` | `false` | `false` draws the strip over the tiles. `true` takes the strip out of the tiles' area, so they make room for it. |

```lua
hl.config({ superposition = { edge = 1, size = 0.12, reserve = true } })
```

## Source

- `superpositionStrip`, `CSpace::placeSuperpositions` and the related code in `src/layout/space/Space.cpp`
- `isAllowedOverFullscreen` in `src/desktop/view/Window.cpp`
- The rule's effect: `src/desktop/rule/windowRule/`
