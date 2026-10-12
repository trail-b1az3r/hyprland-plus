<div align = center>

<img src="https://raw.githubusercontent.com/hyprwm/Hyprland/main/assets/header.svg" width="750" height="300" alt="banner">

<br>

[![Badge Workflow]][Workflow]
[![Badge License]][License] 
![Badge Language] 
[![Badge Pull Requests]][Pull Requests] 
[![Badge Issues]][Issues] 
![Badge Hi Mom]<br>

<br>

Hyprland is a 100% independent, dynamic tiling Wayland compositor that doesn't sacrifice on its looks.

It provides the latest Wayland features, is highly customizable, has all the eyecandy, the most powerful plugins,
easy IPC, much more QoL stuff than other compositors and more...
<br>
<br>

---

**[<kbd> <br> Install <br> </kbd>][Install]** 
**[<kbd> <br> Quick Start <br> </kbd>][Quick Start]** 
**[<kbd> <br> Configure <br> </kbd>][Configure]** 
**[<kbd> <br> Contribute <br> </kbd>][Contribute]**

---

<br>

</div>

# This fork (Halcyon)

This is Hyprland 0.56.0 with a change to how a tiled window that is dragged becomes floating. Upstream
centres the window on the cursor when it floats, so the window jumps. This happens with apps that draw
their own top bar and ask Hyprland to move them, such as Firefox and Dolphin.

## New option: `binds:drag_keep_grab_point`

- Type: boolean, default `false`. Upstream behaviour is unchanged unless you turn it on.
- When on, a tiled or fullscreen window that becomes floating while dragged keeps the exact pixel offset
  between the cursor and the window, so the grab point does not move. If the offset is larger than the new
  window, it is clamped to the window's edge.
- It takes precedence over `binds:drag_center_window`.

Lua (`hl.config`):

```lua
hl.config({ binds = { drag_keep_grab_point = true, drag_center_window = false } })
```

Hyprlang:

```
binds {
    drag_keep_grab_point = true
}
```

Source: `src/config/values/ConfigValues.cpp` (option declaration) and
`src/layout/supplementary/DragController.cpp` (`updateDragWindow`, `MAPDRAGHOTSPOT`).

## Mini windows: `hl.dsp.window.mini`

A mini window is a very small floating window. It can be moved anywhere on the screen, so it can be fitted
around other windows, and it stays small while it is moved. A press and release on it without moving it
brings it back to its full size.

- `hl.dsp.window.mini({ action = "toggle" | "enable" | "disable", window = "address:..." })`. With no table,
  it toggles the active window. Mini windows are floating; a tiled window becomes floating first.
- Size: `binds:mini_width` and `binds:mini_height` (pixels, default 96 x 54).
- The top-left corner stays where the window was, pulled back onto the monitor if the mini size would leave
  it. Restoring puts the window back at its previous place and size and it stays floating.
- Moving a mini window uses the normal move paths: SUPER + drag, or any move dispatch. The existing
  `general:snap:enabled` setting also snaps mini windows to the edges and to other windows while they are dragged.
- `hyprctl clients -j` now has a `"mini"` boolean for each window. A `mini` socket2 event is sent with
  `address,state` when a window changes.

Lua:

```lua
hl.config({ binds = { mini_width = 96, mini_height = 54 } })
hl.bind("SUPER + CTRL + N", hl.dsp.window.mini(), { description = "Window: Make mini, or bring back to full size" })
```

Source: `miniWindow` in `src/config/shared/actions/ConfigActions.cpp`, the click handling in
`processMouseDownNormal` (`src/managers/input/InputManager.cpp`), `isMini` and `m_mini` in
`src/desktop/view/window/Window.hpp`, and the Lua binding in `src/config/lua/bindings/LuaBindingsDispatchers.cpp`.

### Status

Not built or run yet. In the session where this was written, the build dependencies (hyprutils, aquamarine,
hyprlang, hyprgraphics, hyprcursor, wayland-server, libinput) were not installed, so the files were not
compiled. Build it and test a drag and a click on a mini window before relying on it.

# Features

- All of the eyecandy: gradient borders, many types of blur, animations, glow, shadows and much more
- A lot of customization
- 100% independent, no wlroots, no libweston, no kwin, no mutter.
- Custom bezier and spring curves for the best animations
- Powerful plugin support
- Built-in plugin manager
- Tearing support for better gaming performance
- Easily expandable and readable codebase
- Fast and active development
- Not afraid to provide bleeding-edge features
- Config reloaded instantly upon saving
- Global keybinds passed to your apps of choice
- Tiling/pseudotiling/floating/fullscreen windows
- Special workspaces (scratchpads)
- Window groups (tabbed mode)
- Powerful window/monitor/layer rules
- Socket-based IPC
- Native IME and Input Panels Support
- Fully dynamic workspaces
- Extensive layout support
    - Dwindle
    - Scrolling
    - Master
    - Monocle
    - Custom layout with Lua
    - Custom layouts with plugins
- Per Workspace Layouts
- and much more...

<br>
<br>

<div align = center>

# Gallery

<br>

![Preview A]

<br>

![Preview B]

<br>

![Preview C]

<br>
<br>

</div>

# Special Thanks

<br>

**[wlroots]** - *For powering Hyprland in the past*

**[tinywl]** - *For showing how 2 do stuff*

**[Sway]** - *For showing how 2 do stuff the overkill way*

**[Vivarium]** - *For showing how 2 do stuff the simple way*

**[dwl]** - *For showing how 2 do stuff the hacky way*

**[Wayfire]** - *For showing how 2 do some graphics stuff*


<!----------------------------------------------------------------------------->

[Configure]: https://wiki.hypr.land/configuring/
[Stars]: https://starchart.cc/hyprwm/Hyprland
[Hypr]: https://github.com/hyprwm/Hypr

[Pull Requests]: https://github.com/hyprwm/Hyprland/pulls
[Issues]: https://github.com/hyprwm/Hyprland/issues
[Todo]: https://github.com/hyprwm/Hyprland/projects?type=beta

[Contribute]: https://wiki.hypr.land/contributing-and-debugging/
[Install]: https://wiki.hypr.land/getting-started/installation/
[Quick Start]: https://wiki.hypr.land/getting-started/master-tutorial/
[Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml
[License]: LICENSE


<!----------------------------------{ Thanks }--------------------------------->

[Vivarium]: https://github.com/inclement/vivarium
[WlRoots]: https://gitlab.freedesktop.org/wlroots/wlroots
[Wayfire]: https://github.com/WayfireWM/wayfire
[TinyWl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/tinywl/tinywl.c
[Sway]: https://github.com/swaywm/sway
[DWL]: https://codeberg.org/dwl/dwl

<!----------------------------------{ Images }--------------------------------->

[Preview A]: ./assets/prev1.png
[Preview B]: ./assets/prev2.png
[Preview C]: ./assets/prev3.png


<!----------------------------------{ Badges }--------------------------------->

[Badge Workflow]: https://github.com/hyprwm/Hyprland/actions/workflows/ci.yaml/badge.svg

[Badge Issues]: https://img.shields.io/github/issues/hyprwm/Hyprland
[Badge Pull Requests]: https://img.shields.io/github/issues-pr/hyprwm/Hyprland
[Badge Language]: https://img.shields.io/github/languages/top/hyprwm/Hyprland
[Badge License]: https://img.shields.io/github/license/hyprwm/Hyprland
[Badge Lines]: https://img.shields.io/tokei/lines/github/hyprwm/Hyprland
[Badge Hi Mom]: https://img.shields.io/badge/Hi-mom!-ff69b4
