# AUR package: hyprland-plus

`PKGBUILD` and `.SRCINFO` for the AUR package `hyprland-plus`, maintained by ray0rfire. The AUR is a
separate git repository, so these files are copied there when the package is published.

Before publishing, on Arch:

```sh
cd packaging/aur
namcap PKGBUILD          # checks the dependency names against the repositories
makepkg -s               # builds and installs the build dependencies
makepkg --printsrcinfo > .SRCINFO
```

The source is the `halcyon-0.56.2` branch. Pin a tag or commit in `source` once one exists, so a rebuild
gives the same code.

The package provides and conflicts with `hyprland`, so it replaces the packaged compositor. The Halcyon
installer (`./setup install` in the dots repository) detects it with `pacman -Q hyprland-plus` and does not
build it again.
