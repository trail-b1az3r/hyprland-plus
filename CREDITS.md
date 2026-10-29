# Credits

hyprland-plus is Hyprland 0.57.0 with the Halcyon additions. Hyprland and the libraries it is built
on are the work of the people named here. The fork's own licence is [LICENSE](LICENSE) (BSD-3-Clause).

## Hyprland

Hyprland is by **vaxerski** and the Hyprland contributors, under the BSD-3-Clause licence
(`LICENSE`, Copyright 2022-2026 vaxerski). Everything in this fork that is not listed below comes from
upstream. Upstream's own thanks are in the README under *Special Thanks*.

## Bundled code

These are compiled into hyprland-plus, or fetched when the build needs them. Each keeps its own licence.

- **udis86**, the x86 disassembler, in `subprojects/udis86`. Copyright (c) 2002-2012 Vivek Thampi, BSD licence
  (`subprojects/udis86/LICENSE`). Built from the subproject when no system copy is found.
- **hyprland-protocols**, the Wayland protocol extensions Hyprland uses, in `subprojects/hyprland-protocols`.
  Copyright (c) 2022 Hypr Development, BSD-3-Clause (`subprojects/hyprland-protocols/LICENSE`).
- **Tracy**, the profiler client, in `subprojects/tracy`. Built only with the profiler option on. Its
  licence is in that submodule, by Bartosz Taudul (wolfpld).
- **glaze**, a JSON library by Stephen Berry, fetched at build time only if no system copy is installed.
  Its licence is MIT.

## Hypr libraries

Hyprland is built on libraries from the same project, which are installed as separate packages:
**aquamarine** (backends), **hyprlang** (config language), **hyprutils** (utilities), **hyprcursor**
(cursor themes), **hyprgraphics** (graphics), and **hyprwayland-scanner** (protocol code generation).
Each is by the Hypr developers (vaxerski and contributors); see each project for its licence.

## Halcyon additions

The mini windows, superposition windows, plustile windows, the drag grab-point fix, the drag guide and the
`hyprctl dots` command are the Halcyon additions in this fork. They are described in the README under
*What this fork adds*. The Halcyon desktop they are used in is built on end-4's illogical-impulse; see the
dots repository's CREDITS.md.

## Other projects that inspired Hyprland

wlroots, tinywl, Sway, Vivarium, dwl and Wayfire, as listed in the README under *Special Thanks*.
