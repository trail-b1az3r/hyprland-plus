---
weight: 55
title: Desk
---

Three additions in 0.56.2.p2 for working at the desktop: desktop peek, locate pointer, and windows that hold focus.

## Desktop peek: `hl.dsp.plus.peek`

```lua
hl.dsp.plus.peek({ action = "toggle" | "enable" | "disable" })
```

The windows on the focused monitor fade almost away, and the pointer goes through them to whatever is below: the
wallpaper and the widgets drawn on it (layer-shell `background` and `bottom` surfaces). The bar and other `top` and
`overlay` surfaces work as usual. Run it again to bring the windows back. Changing the workspace on that monitor also
ends it.

Keyboard focus doesn't move when peek starts, so the window that had it still has it. A click on a widget gives the
widget the click. Windows opened during a peek are faded too until it ends.

| Option | Default | Meaning |
|---|---|---|
| `plus:peek_opacity` | `0.08` | How opaque windows stay, from `0` (gone) to `1` |
| `plus:peek_duration` | `220` | How long the fade takes, in milliseconds, each way |

```lua
hl.bind("SUPER + D", hl.dsp.plus.peek(), { description = "Desktop: Peek at the desktop" })
```

## Locate pointer: `hl.dsp.plus.locate_pointer`

```lua
hl.dsp.plus.locate_pointer()
```

A ring closes in on the pointer from three times its size, follows it while it moves, and fades. With
`plus:shake_to_find`, shaking the mouse (four or more quick turns within half a second, going nowhere) does the same.
A slow back and forth, a long sweep, or a hand resting on the mouse doesn't.

| Option | Default | Meaning |
|---|---|---|
| `plus:shake_to_find` | `false` | Shaking the mouse draws the ring |
| `plus:locate_size` | `36` | The ring's radius once it has closed in, in pixels |
| `plus:locate_duration` | `900` | How long the ring stays, in milliseconds |
| `plus:feedback:locate_color` | `0xffffc842` | The ring's colour. A faint fill inside it uses the same colour. |

```lua
hl.config({ plus = { shake_to_find = true } })
hl.bind("SUPER + CTRL + period", hl.dsp.plus.locate_pointer(), { description = "Pointer: Show where it is" })
```

## Hold focus: the `hold_focus` rule

```lua
hl.window_rule({ match = { class = "^(steam_app_.*)$" }, hold_focus = true })
```

A window that holds focus keeps the keyboard when the pointer leaves it, as if `input:follow_mouse` were `2` while it
has focus. The windows the pointer passes over still get pointer events, but not the keyboard. A click on another
window, a bind that focuses one, or the window closing moves focus as usual. Many games pause or stop drawing when
they lose keyboard focus. Without this, moving the pointer onto a second screen, or past the edge of a game that isn't
fullscreen, looked like the game had frozen.

It only changes anything with `input:follow_mouse = 1`, the default. Bar and panel surfaces that take the keyboard on
hover don't take it from a window that holds focus either. Ones that ask for it exclusively (a lock screen, a launcher)
still do.

| Option | Default | Meaning |
|---|---|---|
| `plus:game_hold_focus` | `false` | Every window whose content type is `game` holds focus, without a rule |

`hl.dsp.window.set_prop({ prop = "hold_focus", value = true })` turns it on for one window, and
`hyprctl getprop <window> hold_focus` reads it.

`hold_focus` differs from `stay_focused`. A `stay_focused` window keeps focus whatever happens, even a click
elsewhere, until it closes. A window that holds focus only ignores the pointer moving away.

## Source

- `src/plus/Desk.cpp`: peek, the ring and hold focus
- `src/plus/Motion.cpp`: the fades, the ring's frames, shake recognition and the mini all row. Tested in
  `tests/plus/Motion.cpp`.
- `mouseMoveUnified` in `src/managers/input/InputManager.cpp`, and `renderWindow` and `renderMonitor` in
  `src/render/Renderer.cpp`, call them.
