---
weight: 90
title: hyprland-plus
---

> [!NOTE]
> This section is about **hyprland-plus**, the fork this wiki ships with. The rest of the wiki describes upstream
> Hyprland 0.56.2, and it still applies here, except where a page below says otherwise.

hyprland-plus is Hyprland 0.56.2 with the additions the Halcyon desktop uses. It keeps the same binary name, config
directories and IPC, so existing configs and session files keep working. The additions are:

| Addition | Where | What it does |
|---|---|---|
| Drag grab point | [Drag](drag/) | A tiled window that starts floating during a drag keeps the grab point, so it doesn't jump. |
| Drag guide | [Drag](drag/) | The tile under the cursor is tinted while a tiled window is dragged. It is the tile the window swaps with. |
| Mini windows | [Mini windows](mini-windows/) | A window shrinks to a small floating tile. A click brings it back. |
| Superposition windows | [Superposition](superposition/) | A tiled window kept on a thin strip at one screen edge, drawn above fullscreen windows. |
| Plustile windows | [Plustile](plustile/) | A tiled window that can be resized from each of its four edges, and fitted to its neighbours. |
| `hyprctl dots`, `store`, `load` | [hyprctl](hyprctl/) | Installs dot sets, and saves and loads them as `.dhyprsave` files. |
| Version string | [Changes](changes/) | `hyprland --version` reports `hyprland-plus 0.56.2.p1`. |

Start with [Install](install/). The list of every difference from upstream is on [Changes from upstream](changes/).
