---
weight: 90
title: hyprland-plus
---

> [!NOTE]
> This section is about **hyprland-plus**, the fork this wiki ships with. The rest of the wiki describes upstream
> Hyprland 0.57.0, and it still applies here, except where a page below says otherwise.

hyprland-plus is Hyprland 0.57.0 with the additions the Halcyon desktop uses. It keeps the same binary name, config
directories and IPC, so existing configs and session files keep working. The additions are:

| Addition | Where | What it does |
|---|---|---|
| Drag grab point | [Drag](drag/) | A tiled window that starts floating during a drag keeps the grab point, so it doesn't jump. |
| Drag guide | [Drag](drag/) | The tile under the cursor is tinted while a tiled window is dragged. It is the tile the window swaps with. |
| Mini windows | [Mini windows](mini-windows/) | A window shrinks to a small floating tile. A click brings it back. |
| Superposition windows | [Superposition](superposition/) | A tiled window kept on a thin strip at one screen edge, drawn above fullscreen windows. |
| Plus Tiles | [Plus Tiles](plustile/) | Ordinary tiles until Move Edge is used on them; then a custom size that the neighbours fit around. With an inspector and geometric navigation. |
| Desk | [Desk](desk/) | Desktop peek, locate pointer (and shake to find), and windows that hold focus, for games. |
| Mini all | [Mini windows](mini-windows/) | Every window on the workspace shrinks to a mini window in a row along the bottom, and back. |
| `hyprctl dots`, `store`, `load` | [hyprctl](hyprctl/) | Installs dot sets, and saves and loads them as `.dhyprsave` files. |
| Version string | [Changes](changes/) | `hyprland --version` reports `hyprland-plus 0.57.0.p1`. |

Start with [Install](install/). The list of every difference from upstream is on [Changes from upstream](changes/).
