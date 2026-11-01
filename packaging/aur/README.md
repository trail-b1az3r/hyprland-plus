# AUR package: hyprland-plus

This directory is the source of truth for the AUR package `hyprland-plus`.
The AUR itself is a separate Git repository; this GitHub repository stores the
maintained `PKGBUILD` and the release workflow.

## Initial package setup (one time)

1. Create an account at https://aur.archlinux.org/ if you do not already have one.
2. Add an SSH public key under **My Account → SSH Public Keys**.
3. Open https://aur.archlinux.org/packages/hyprland-plus and choose **Submit** to register
   the package name. The first push must be made manually.
4. On an Arch Linux machine with `base-devel` and `git` installed:

   ```sh
   git clone https://github.com/trail-b1az3r/hyprland-plus.git ~/src/hyprland-plus
   cd ~/src/hyprland-plus/packaging/aur
   makepkg --printsrcinfo > .SRCINFO
   namcap PKGBUILD
   makepkg -s
   ```

5. Clone the newly registered AUR Git repository, copy in the two package files,
   then make the initial AUR commit and push:

   ```sh
   git clone ssh://aur@aur.archlinux.org/hyprland-plus.git ~/src/hyprland-plus-aur
   cp PKGBUILD .SRCINFO ~/src/hyprland-plus-aur/
   cd ~/src/hyprland-plus-aur
   git config user.name "Your AUR username"
   git config user.email "your verified email"
   git add PKGBUILD .SRCINFO
   git commit -m "Initial import: hyprland-plus"
   git push origin master
   ```



## Automatic release publishing

The workflow `.github/workflows/publish-aur.yml` runs when a GitHub Release is
published. The release tag (with an optional leading `v`) must match
`PLUS_VERSION`. It pins the PKGBUILD source to the exact release commit, updates
`pkgver`, regenerates `.SRCINFO` using Arch's `makepkg`, and pushes the two files
to the existing AUR Git repository.

Add these **repository Actions secrets** under GitHub → Settings → Secrets and
variables → Actions:

- `AUR_SSH_PRIVATE_KEY`: the private key whose public half is registered in your AUR account.
- `AUR_COMMIT_EMAIL`: the email address to use for AUR commits.

Never commit the private key. The first AUR registration and initial commit are
manual; subsequent published releases are automatic.

## Local validation

On Arch Linux:

```sh
namcap PKGBUILD
makepkg --printsrcinfo > .SRCINFO
makepkg -s
```

The package provides and conflicts with `hyprland`, so installing it replaces
the stock compositor package. Review dependency names against the current Arch
repositories before submitting changes.
