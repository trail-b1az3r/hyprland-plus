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
4. Keeps a copy of the packaged `hyprland` binary, if there is one, as `/usr/bin/hyprland.official`. The package
   itself stays installed, because other packages depend on it.
5. Installs to `/usr/local`, where it runs before anything in `/usr/bin`.
6. If nothing in pacman provides `hyprland` (you never installed the stock package), installs a small empty package,
   `hyprland-plus-local`, that does. Packages that depend on Hyprland are then satisfied by the fork, and pacman never
   pulls the stock `hyprland` in beside it. `sudo pacman -R hyprland-plus-local` takes it away.
7. Removes `hyprctl-lite`.

If a step fails, `hyprctl-lite` stays in `/usr/local/bin`, so you can run the second command again.

The `curl` command runs a script from GitHub as root. Read it first if you prefer:
`curl -fsSL <the same URL> | less`.

Log out and back in to run the new build.

{{< callout type="info" >}}
hyprland-plus is not a separate command. It **is** Hyprland, built with the additions, and it takes the place of the
`Hyprland` program: start it the way you always start Hyprland. To check what is running, use `hyprctl version`, which
says `hyprland-plus` in its first line. With the Halcyon dotfiles, `halcyon hyprland` also tells you whether the build
is new enough for Plus Tiles.
{{< /callout >}}

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

The build uses `glaze` for JSON. If the system copy is missing, CMake fetches glaze 7.2.0 at build time. Any glaze from
version 7 up is accepted (Arch ships 9).

## The AUR package

`packaging/aur/` holds the `PKGBUILD` and `.SRCINFO` for an AUR package named `hyprland-plus`. It provides and
conflicts with `hyprland`. Run `namcap` and `makepkg -s` on Arch before publishing.

## Halcyon

The Halcyon dotfiles build and install hyprland-plus themselves, on a separate console, during `./setup install`. See the
[Halcyon docs](https://trail-b1az3r.github.io/dots/hyprland-plus.html) for that. The installer finds hyprland-plus however
it was installed: the AUR package, or a build in `/usr/local` from `hyprctl-lite` or an earlier install. An up-to-date
build is not built again.
