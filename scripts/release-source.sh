#!/usr/bin/env bash
# Packs this checkout, submodules included, into <outdir>/<name>.tar.gz, with a .sha256 file beside it.
# The archive has one top folder called <name>. Run it from the repository root, after
# `git submodule update --init --recursive`, so the submodules are populated.
#
# Usage: scripts/release-source.sh <name> <outdir>
set -euo pipefail

name="${1:?usage: scripts/release-source.sh <name> <outdir>}"
outdir="${2:?usage: scripts/release-source.sh <name> <outdir>}"

root="$(pwd)"
mkdir -p "$outdir"
tar --exclude=.git --owner=0 --group=0 --no-same-owner \
  --transform "s,^\./,$name/," --transform "s,^\.\$,$name," \
  -czf "$outdir/$name.tar.gz" -C "$root" .
(cd "$outdir" && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")
echo "$outdir/$name.tar.gz"
