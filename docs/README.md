# hyprland-plus docs

| Path | What it is |
| --- | --- |
| [`wiki/`](wiki/) | The Hyprland wiki, with a section for the hyprland-plus additions. Hugo site (hextra theme). |
| [`wiki/content/hyprland-plus/`](wiki/content/hyprland-plus/_index.md) | The fork's pages: [install](wiki/content/hyprland-plus/install.md), [changes from upstream](wiki/content/hyprland-plus/changes.md), [drag](wiki/content/hyprland-plus/drag.md), [mini windows](wiki/content/hyprland-plus/mini-windows.md), [superposition](wiki/content/hyprland-plus/superposition.md), [plustile](wiki/content/hyprland-plus/plustile.md), [hyprctl additions](wiki/content/hyprland-plus/hyprctl.md). |
| [`wiki/ORIGIN.md`](wiki/ORIGIN.md) | Where the wiki came from, and what changed in it. |
| `Hyprland.1.rst`, `hyprctl.1.rst` | The man pages. They are built into `Hyprland.1` and `hyprctl.1`. |
| [`ISSUE_GUIDELINES.md`](ISSUE_GUIDELINES.md) | Upstream's guidelines for filing issues. |

## Build and preview the wiki

The wiki needs Hugo (extended) and Go, to fetch the theme module:

```sh
cd docs/wiki
hugo server
```

Then open the address Hugo prints.
