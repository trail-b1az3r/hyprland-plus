---
weight: 40
title: Mini windows
---

A mini window is a very small floating window. It can be moved anywhere on the screen, and it stays small while it is
moved. A press and release on it without moving brings it back to its full size.

## The dispatcher: `hl.dsp.window.mini`

```lua
hl.dsp.window.mini({ action = "toggle" | "enable" | "disable", window = "address:..." })
```

- With no table, it toggles the active window.
- `action` defaults to `"toggle"`. `window` defaults to the active window.
- A tiled window becomes floating first.

```lua
hl.bind("SUPER + CTRL + N", hl.dsp.window.mini(), { description = "Window: Make mini, or bring back to full size" })
```

## Size

| Option | Default | Range |
|---|---|---|
| `binds:mini_width` | `96` | 16 to 8192 pixels |
| `binds:mini_height` | `54` | 16 to 8192 pixels |

```lua
hl.config({ binds = { mini_width = 96, mini_height = 54 } })
```

## Position

- The top-left corner stays where the window was. If the mini size would leave the monitor, the window is pulled back
  onto it.
- Restoring puts the window back at its previous place and size. It stays floating.
- Moving a mini window uses the normal move paths: SUPER and a drag, or any move dispatcher. The `general:snap`
  settings also snap a mini window to edges and to other windows while it is dragged.

## Clicking

A press and release on a mini window, without moving it, restores it. Moving it counts as a drag, so it doesn't restore.

## Reading the state

- `hyprctl clients -j` has a `"mini"` boolean for each window.
- The socket2 event `mini` is sent when a window changes. Its data is `address,state`, where the address is in hex with
  no `0x`, the same format as the `pin` event, and the state is `1` or `0`.

```
mini>>5a3f1c2e0,1
```

## Source

- `Actions::miniWindow` in `src/config/shared/actions/ConfigActions.cpp`
- The click handling in `processMouseDownNormal` in `src/managers/input/InputManager.cpp`
- `isMini` and `setMini` in `src/desktop/view/Window.hpp`
- The Lua binding in `src/config/lua/bindings/LuaBindingsDispatchers.cpp`
