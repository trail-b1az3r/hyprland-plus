---
weight: 10
title: Install
---

## One command on Arch

hyprland-plus is built from this repository's `halcyon-0.56.2` branch. The helper `hyprctl-lite` installs or updates it.
Run these two commands:

```sh
curl -fsSL https://raw.githubusercontent.com/trail-b1az3r/dots/claude/youthful-gates-rt1csq-v2/tools/hyprctl-lite/hyprctl-lite | sudo tee /usr/local/bin/hyprctl-lite >/dev/null && sudo chmod 755 /usr/local/bin/hyprctl-lite
sudo hyprctl-lite install full
```

`hyprctl-lite install full` does the following, in order:

1. Clones hyprland-plus into `~/hyprland-plus` of the account that ran `sudo`, or updates that clone.
2. Installs the build dependencies listed in `packaging/aur/PKGBUILD`.
3. Builds hyprland-plus as that account, not as root.
4. Moves the packaged `hyprland` aside, kept as `/usr/bin/hyprland.official`.
5. Installs to `/usr/local`, and removes `hyprctl-lite`.

If a step fails, `hyprctl-lite` stays in `/usr/local/bin`, so you can run the second command again.

The `curl` command runs a script from GitHub as root. Read it first if you prefer:
`curl -fsSL <the same URL> | less`.

Log out and back in, or restart Hyprland, to run the new build.

## Build by hand

Any distribution works if the dependencies are installed. Clone the branch with its submodules, then build:

```sh
git clone --branch halcyon-0.56.2 --recurse-submodules https://github.com/trail-b1az3r/hyprland-plus
cd hyprland-plus
make all            # cmake configure and build, in ./build
sudo make install   # installs to /usr/local
```

The dependencies are the ones in `packaging/aur/PKGBUILD`, in its `depends` and `makedepends` lists. On Arch the
`libeis-1.0` pkg-config name comes from the `libei` package.

The build uses `glaze` for JSON. If the system copy is missing, CMake fetches glaze 7.2.0 at build time. Accepted
versions are 7 and 8.

## The AUR package

`packaging/aur/` holds the `PKGBUILD` and `.SRCINFO` for an AUR package named `hyprland-plus`. It provides and
conflicts with `hyprland`. Run `namcap` and `makepkg -s` on Arch before publishing.

## Halcyon

The Halcyon dotfiles build and install hyprland-plus themselves, on a separate console, during `./setup install`. See the
dots repository's README for that. If the `hyprland-plus` package is already installed from the AUR, the installer skips
the build.
