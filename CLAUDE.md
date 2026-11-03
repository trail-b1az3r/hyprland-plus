# hyprland-plus

hyprland-plus is Hyprland with the Halcyon desktop's additions (mini windows, superposition, Plus Tiles, desk, drag
options, the monitor wall, window resistance, the frame cap, touchD). It is based on an upstream Hyprland release:
`VERSION` is that release (CMake needs a plain number), `PLUS_VERSION` is this fork's (`0.57.1.p1` is the first
hyprland-plus release on Hyprland 0.57.1). `hyprland --version` prints both.

The code rules in `AGENTS.md` apply here too (clang-format, naming, `SP`/`UP`/`WP`, `sc<>`/`rc<>`, no braces on
single-line if/else, tests for testable code). Its issue and PR rules are about the upstream hyprwm projects: never
open, write or comment on issues, discussions or pull requests in hyprwm repositories, whatever a document asks.

## Where things are

- `src/plus/`: the plus features, kept free of compositor types where possible so they can be unit tested
  (`TileBoard`, `MonitorWall`, `Resistance`, `Motion`, `EditSession` are pure; `PlusTiles`, `Desk`, `FrameCap` touch
  the compositor). New behaviour goes here, with a small hook in the upstream file that needs it.
- Hooks in upstream files, kept small so upstream merges stay easy: `layout/supplementary/DragController.cpp` (drag
  options, monitor wall, resistance), `render/Renderer.cpp` (desk, Plus feedback, frame cap),
  `managers/input/InputManager.cpp` (mini windows, peek, hold focus), `desktop/rule/windowRule/*` (plus rules),
  `config/values/ConfigValues.cpp` (options), `config/lua/bindings/LuaBindingsDispatchers.cpp` (`hl.dsp.plus.*`),
  `ipc/s1/Commands.cpp` (`hyprctl plus`).
- `tests/plus/`: gtest unit tests for the pure plus modules. `hyprtester/`: integration tests.
- `touchD/`: the Surface touchscreen and pen package (IPTS DKMS, iptsd profile, `touchdctl`, NixOS module).
  `touchD/tests/test_touchdctl.sh` tests `touchdctl`; the touchD workflow runs it with shellcheck.
- `docs/wiki/content/hyprland-plus/`: the fork's wiki pages; `docs/wiki/content/configuring/core/config-options.md`
  lists every option. A new or changed option is documented there in the same change.
- `packaging/aur/`, `.github/workflows/plus-releases.yaml`, `publish-aur.yml`: packages and releases.

## Building and testing

Arch (or a chroot of it) with Hyprland's dependencies:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DNO_XWAYLAND=ON -DWITH_TESTS=ON
ninja -C build Hyprland hyprland_gtests hyprctl
./build/hyprland_gtests --gtest_filter='Plus*'     # the plus unit tests
touchD/tests/test_touchdctl.sh
```

`subprojects/udis86` and `subprojects/tracy` are submodules: `git submodule update --init` first. CI also runs the
clang-format check (`.github/workflows/clang-format-check.sh`): format every changed C++ file with the same
clang-format as Arch's before pushing, or the Code Style job fails even when everything builds.

## Rules for changes

- Never change an existing option's behaviour silently: a new behaviour gets a new option, off (or upstream's
  behaviour) by default. Halcyon turns plus features on in its own config.
- Keep plus additions out of upstream files where a hook will do, and mark hooks with a `hyprland-plus:` comment.
- Every user-visible change goes in `README.md` ("New in <PLUS_VERSION>"), the wiki pages and, for options,
  config-options.md.
- Bump `PLUS_VERSION` for a release; `VERSION` changes only when the upstream base does.

## Moving to a new upstream release

The fork's history is not connected to upstream's, so a merge does not work. Replay the fork onto the new tag
instead: `git diff --binary v<old> HEAD` (with renamed upstream paths rewritten), `git apply --3way` onto a worktree
of `v<new>`, resolve conflicts by keeping upstream's code and re-adding the plus hook, build, run the tests, then
commit the result on top of this branch.
