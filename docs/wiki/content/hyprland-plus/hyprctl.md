---
weight: 70
title: hyprctl additions
---

The `hyprctl` in hyprland-plus has the upstream commands, and these additions.

## Dot sets: `hyprctl dots`

`hyprctl dots` installs dot sets from their upstream repositories. Each set is cloned as it is, nothing in it is edited,
and the config it replaces is backed up first.

| Command | Does |
|---|---|
| `hyprctl dots` | Lists the sets |
| `hyprctl dots <set>` | Describes what installing it would do |
| `hyprctl dots <set> --install` | Clones and installs it. Run this in a terminal, because the installers ask questions. |
| `hyprctl dots plus` | Shows the Halcyon checkout the installer recorded |
| `hyprctl dots plus --install` | Reinstalls Halcyon from that checkout, without building hyprland-plus again |

The sets are `ii` (illogical-impulse, the base Halcyon is built on), `jakoolit`, `mlfw` (ML4W OS) and `gdots`. Aliases
are accepted: `illogical-impulse`, `hyprland-dots`, `ml4w` and `gdot`.

Installing a set replaces the desktop's config in `~/.config`. Its backup is in
`~/.local/state/halcyon/backups/<set>-<time>`.

## Saves: `hyprctl store` and `hyprctl load`

A save is a gzipped tar named `<name>.dhyprsave`, in `~/.local/share/halcyon/saves`.

| Command | Does |
|---|---|
| `hyprctl store -n <name>` | Saves the dot files as they are now. `hyprctl save -n <name>` does the same. |
| `hyprctl load -l` | Lists the saves. The one in use is marked `(active)`. |
| `hyprctl load -n <name>` | Loads a save |
| `hyprctl load -a` | Loads the save from before the latest switch. Running it again goes back. |
| `sudo hyprctl load ...` | The same, for the user who ran `sudo`, not root |

A save holds the top-level entries of `~/.config` that any dot set installs, as they are now, and always the ones the
Halcyon dots install (`hypr`, `quickshell`, `illogical-impulse` and the rest), even when the dot sets aren't recorded.
It also holds the `.local` files the config refers to: scripts in `~/.local/bin`, fonts, icons, themes, Halcyon's state
files (`~/.local/state/halcyon/*.lua` and `active`), and the shell's own settings in `~/.local/state/quickshell/user`.
Nothing else in `~/.local` is included. `manifest.txt` inside lists what it holds.

An entry of `~/.config` that is a link (the installer links `hypr` and `quickshell` into its clone) is saved as the
files it points to. Before 0.56.2.p2 only the link was saved, so a save was about 10 KB and loaded nothing on another
computer. `store` prints how many files it saved and the size, and warns when a save holds fewer than 50 files.

Loading first backs up the files it replaces to `~/.local/state/halcyon/backups/before-load-<time>-<pid>`. Each dot set
install, and each `hyprctl dots plus --install`, stores the result, so `load -a` has something to go back to.

## Source

- `runDotsCommand`, `runStore` and `runLoad` in `hyprctl/src/main.cpp`
