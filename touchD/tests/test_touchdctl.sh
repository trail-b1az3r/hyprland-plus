#!/usr/bin/env bash
# `cond && ok || fail` is safe here: ok only echoes. The functions under test are read from touchdctl itself.
# shellcheck disable=SC2015,SC1090
# touchdctl against stand-ins for the iptsd tools and a fake DMI table. Run: touchD/tests/test_touchdctl.sh
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
ctl="$here/../touchdctl"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/bin" "$tmp/dmi" "$tmp/etc"
fails=0

ok() { echo "ok   $1"; }
fail() { echo "FAIL $1"; fails=$((fails + 1)); }
expect() { # expect NAME EXIT-CODE COMMAND...
  local name="$1" want="$2"; shift 2
  set +e; out="$("$@" 2>&1)"; got=$?; set -e
  if [[ "$got" == "$want" ]]; then ok "$name"; else fail "$name (exit $got, wanted $want): $out"; fi
}

export PATH="$tmp/bin:$PATH" TOUCHD_DMI="$tmp/dmi" TOUCHD_CONFIG="$tmp/etc/80.conf" \
  TOUCHD_CALIBRATION="$tmp/etc/90-calibration.conf" TOUCHD_PROFILE="$here/../iptsd-surface-pro-5.conf"

# Subcommands are matched at all (they used to fall through to usage).
expect "help" 0 "$ctl" --help
expect "unknown command" 2 "$ctl" frobnicate
expect "pen-buttons" 0 "$ctl" pen-buttons
grep -q eraser_button_override <<<"$out" && ok "pen-buttons prints the guide" || fail "pen-buttons output"

# Model detection.
echo "Surface Pro" > "$tmp/dmi/product_name"
expect "status runs" 0 "$ctl" status
grep -q "Model: surface-pro-5" <<<"$out" && ok "the 2017 Surface Pro is a Pro 5" || fail "model detection: $out"
echo "Surface Book 2" > "$tmp/dmi/product_name"
grep -q "Model: surface-book" <<<"$("$ctl" status)" && ok "Surface Book 2" || fail "Surface Book 2"
echo "XPS 13" > "$tmp/dmi/product_name"
grep -q "Model: unknown (XPS 13)" <<<"$("$ctl" status)" && ok "not a Surface" || fail "unknown model"

# Calibration: the last values iptsd-calibrate printed are written as a [Contacts] drop-in.
source <(sed -n '/^last_value()/,/^}/p; /^write_calibration()/,/^}/p' "$ctl")
CALIBRATION="$tmp/etc/90-calibration.conf"
printf 'SizeMin: 0.100\nSizeMax: 1.000\nAspectMin = 0.9\nAspectMax: 2.0\nSizeMin: 0.180\nSizeMax: 1.950\n' > "$tmp/log"
mkdir -p "$tmp/work-old" "$tmp/work-new"
write_calibration "$tmp/log" "$tmp/work-old" >/dev/null
grep -q "SizeMin = 0.180" "$CALIBRATION" && grep -q "SizeMax = 1.950" "$CALIBRATION" && grep -q "^\[Contacts\]" "$CALIBRATION" \
  && ok "calibration keeps the final values" || fail "calibration file: $(cat "$CALIBRATION")"
printf 'SizeMin: 0.1\n' > "$tmp/short"
if write_calibration "$tmp/short" "$tmp/work-old" 2>/dev/null; then fail "partial calibration written"; else ok "partial calibration refused"; fi

# Newer iptsd-calibrate prints only summaries and writes snippets: the 2 mm one is used.
printf 'Size:    0.500 (Min: 0.200; Max: 0.900)\n' > "$tmp/newlog"
for slack in 0mm:0.250 2mm:0.150 10mm:0.050; do
  printf '[Device]\nVendor = 0x1B96\n\n[Contacts]\nSizeMin = %s\nSizeMax = 1.700\nAspectMin = 1.000\nAspectMax = 2.400\n' \
    "${slack#*:}" > "$tmp/work-new/iptsd_calib_1B96_001F_1_${slack%%:*}.conf"
done
write_calibration "$tmp/newlog" "$tmp/work-new" >/dev/null
grep -q "SizeMin = 0.150" "$CALIBRATION" && grep -q "AspectMax = 2.400" "$CALIBRATION" \
  && ok "calibration takes the recommended snippet" || fail "snippet calibration: $(cat "$CALIBRATION")"

# model: prints the profile name only on a supported Surface.
echo "Surface Pro" > "$tmp/dmi/product_name"
expect "model on a Surface" 0 "$ctl" model
[[ "$out" == surface-pro-5 ]] && ok "model name" || fail "model output: $out"
echo "XPS 13" > "$tmp/dmi/product_name"
expect "model elsewhere" 1 "$ctl" model

# The shipped profile does not switch every finger off when a palm is seen.
grep -q "^DisableOnPalm = false" "$here/../iptsd-surface-pro-5.conf" && ok "profile keeps fingers with a palm" || fail "DisableOnPalm"

# doctor reports what is missing and fails.
expect "doctor without iptsd" 1 "$ctl" doctor
grep -q "FIX   iptsd installed" <<<"$out" && ok "doctor names iptsd" || fail "doctor output: $out"

(( fails == 0 )) && echo "all passed" || { echo "$fails failed"; exit 1; }
