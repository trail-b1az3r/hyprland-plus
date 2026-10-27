#!/usr/bin/env bash
# Builds the hyprland-plus wiki, versioned: the current wiki at the site's root, and the wiki as it was at each
# release (every v* tag) under /<version>/, marked as an older version. The version selector lists them all.
#
#   scripts/build-wiki-versions.sh OUT_DIR BASE_URL [HUGO]
#
# Run from the repository root, with the tags fetched (the wiki workflow checks out with fetch-depth 0).
set -euo pipefail

out="$1"
base="${2%/}"
hugo="${3:-hugo}"
root="$(pwd)"
work="$(mktemp -d)"
trap 'git -C "$root" worktree prune >/dev/null 2>&1 || true; rm -rf "$work"' EXIT

# Newest first: v0.56.2.p10 before v0.56.2.p9.
mapfile -t versions < <(git tag -l 'v*' | sed 's/^v//' | sort -rV)

quoted=""
for v in "${versions[@]}"; do quoted+="\"$v\", "; done

# Settings for one build, read after config.toml, so they win.
params() { # NAME VERSION OUTDATED
  cat <<TOML
[params.version]
name     = "$1"
version  = "$2"
outdated = $3
plus     = [ ${quoted%, } ]
TOML
}

current="$(tr -d '[:space:]' < PLUS_VERSION)"
params "hyprland-plus $current (latest)" "" false > "$work/latest.toml"
(cd docs/wiki && "$hugo" --gc --minify --baseURL "$base/" --config "config.toml,$work/latest.toml" -d "$out")

for v in "${versions[@]}"; do
  tree="$work/tree-$v"
  git worktree add --detach "$tree" "v$v" >/dev/null 2>&1 || { echo "skipping $v: no checkout" >&2; continue; }
  if [[ ! -f "$tree/docs/wiki/config.toml" ]]; then
    echo "skipping $v: it has no wiki" >&2
    git worktree remove --force "$tree"
    continue
  fi
  params "hyprland-plus $v" "hyprland-plus $v" true > "$work/$v.toml"
  # An old tag's wiki may predate the plus list: the selector there still links back to the latest.
  (cd "$tree/docs/wiki" && "$hugo" --gc --minify --baseURL "$base/$v/" --config "config.toml,$work/$v.toml" -d "$out/$v")
  git worktree remove --force "$tree"
  echo "built $v"
done
