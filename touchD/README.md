# touchD

touchD is the Surface-focused touchscreen integration shipped alongside
hyprland-plus. It packages the upstream Intel Precise Touch & Stylus (IPTS)
kernel module for DKMS and provides a small control utility around the
upstream iptsd userspace daemon.

It targets Surface Pro 4–6 class IPTS hardware, including Surface Pro 5.
The DKMS module is the upstream kernel transport driver; touchD does not fork
the hardware protocol or run a second event-processing daemon.

## Install on Arch

Build the package from this directory:

~~~sh
makepkg -si
~~~

Install a kernel with the required IPTS kernel patches (the linux-surface
kernel is recommended), matching kernel headers, and the iptsd package.
The DKMS build can fail on a generic kernel without the IPTS prerequisite
patches; touchD does not silently patch or replace your kernel.

If the running kernel already has IPTS built in, you do not need the DKMS
module. Do not load a second IPTS module over an in-tree driver.

## Setup

~~~sh
sudo touchdctl profile          # detects the model; or name it: surface-pro-4/5/6, surface-book, surface-laptop
sudo touchdctl restart
touchdctl doctor                # every piece touch and pen need, with how to fix what is missing
~~~

The Surface Pro 4–6, Surface Book 1–2 and Surface Laptop 1–2 share the same IPTS digitizer generation, so they use
the same profile; it was tuned on a Surface Pro 5, and calibrating fits it to your screen.

The profile is installed as
`/etc/iptsd.d/80-touchd-surface-pro-5.conf`, leaving your main
`/etc/iptsd.conf` intact. It tunes contact size/aspect bounds for angled
finger contacts, enables IPTSD palm classification, preserves finger input
while the pen is hovering, and raises the DFT pen-button magnitude threshold
to reduce accidental button events.

Run `sudo touchdctl calibrate --apply` to stop the detected IPTSD service, find the
touchscreen's hidraw device, and launch the upstream calibration tool. When you
press Ctrl+C, its final values are written to `/etc/iptsd.d/90-calibration.conf`
for you (the old file is backed up); on NixOS they are printed for
`services.touchD.calibration` instead. Touch each finger lightly and firmly,
then test multi-finger gestures; avoid resting your palm on the display during
calibration. The command restarts IPTSD on exit. Without `--apply`, copy the
final `SizeMin`, `SizeMax`, `AspectMin` and `AspectMax` values into
`/etc/iptsd.d/90-calibration.conf` under `[Contacts]` yourself. This later
drop-in overrides the broad defaults in the touchD profile.

## Pen controls and pressure

Hyprland Plus exposes per-tool eraser button mode, button override, and
pressure range. Run touchdctl pen-buttons for the configuration guide.

Version 0.57.0.p2 adds app-scoped pressure dynamics:

~~~ini
input {
    tablettool {
        pressure_curve = soft # linear, soft, firm, precision
        pressure_speed = 0.15 # 0–1; 0 disables speed modulation
        pressure_tilt = 0.10  # 0–1; 0 disables tilt modulation
    }
}
~~~

These adjustments apply only while the pen is focused over Krita, GIMP,
Inkscape, MyPaint or Blender. Other applications receive unmodified hardware
pressure and tilt. Speed is estimated from timestamped tablet-space motion;
tilt comes from the tablet tool's reported angle.

- linear: unchanged pressure
- soft: square-root response for lighter strokes
- firm: squared response for more deliberate strokes
- precision: steeper response for fine control
- pressure_speed: up to 25% less pressure response at fast stroke speeds
- pressure_tilt: up to 15% more pressure response at high pen tilt

Both dynamic controls default to 0 in the compositor. Halcyon opts into
subtle values; set either to 0 to turn it off.

Pen buttons are delivered by IPTSD/libinput and can also be mapped inside
individual drawing applications. The touchscreen driver cannot invent
physical buttons that the pen does not report.

## Handwriting to text

touchD exposes touchdctl handwriting as a launcher for an installed
Wayland handwriting overlay. The recommended Hyprland frontend is
[handwrite-input](https://gitea.edley.me/petere/handwrite-input): touchdctl
starts its daemon if needed, then opens its overlay with
handwrite-input-client. It uses TrOCR for local handwriting recognition and
types the result into the focused application. katip-ink is also detected as
an optional fallback.

These are optional recognition frontends, not kernel modules. handwrite-input
requires GTK3, gtk-layer-shell, PyTorch, Transformers and ydotool or wtype;
the first recognition downloads model weights. Its recognition runs on a
worker thread outside Hyprland's input/render loops so model inference does
not block compositor input handling.

### NixOS

Do not install the Arch DKMS package on NixOS. Use the Surface Pro Intel
module from nixos-hardware, which selects the patched Surface kernel and
enables IPTS support. Add these flake inputs:

~~~nix
inputs.nixos-hardware.url = "github:NixOS/nixos-hardware";
inputs.handwrite-input.url = "git+https://gitea.edley.me/petere/handwrite-input.git";
~~~

Import the modules in your NixOS configuration:

~~~nix
imports = [
  inputs.nixos-hardware.nixosModules.microsoft-surface-pro-intel
  inputs.handwrite-input.nixosModules.handwrite-input
  /path/to/hyprland-plus/touchD/nixos/module.nix
];

services.iptsd.enable = true;
services.touchD.enable = true;
services.handwrite-input.enable = true;
services.handwrite-input.users = [ "your-user" ];
~~~

The touchD module adds the Surface Pro 5 IPTSD drop-in and the `touchdctl`
utility. The Surface Pro Intel module supplies the patched kernel and
IPTSD service; no DKMS module is needed when the kernel already includes IPTS.
The handwriting module provides the TrOCR overlay. Log in again after adding
the user so ydotool group membership takes effect.

On NixOS, `touchdctl profile surface-pro-5` is intentionally disabled because
`/etc` is generated declaratively. Enable `services.touchD` instead. After
calibration, put the measured values in your NixOS configuration:

~~~nix
environment.etc."iptsd.d/90-calibration.conf".text = ''
  [Contacts]
  SizeMin = 0.42
  SizeMax = 0.86
  AspectMin = 1.02
  AspectMax = 2.32
'';
~~~

Replace those example values with the measurements printed by
`sudo touchdctl calibrate`, then rebuild the system.

## Responsiveness and failure handling

- The IPTS kernel module and iptsd remain the only hardware event path.
- touchD is not a resident poller and does not sit between input and Hyprland.
- Calibration and config changes are explicit user actions.
- If DKMS cannot build for the selected kernel, the package prints a warning
  and leaves the existing kernel and input stack untouched.
- No software can promise zero driver stalls on every firmware/kernel
  combination; inspect journalctl and dmesg when diagnosing missed contacts
  or resume failures.

## Upstream projects

- https://github.com/linux-surface/intel-precise-touch
- https://github.com/linux-surface/iptsd
- https://github.com/linux-surface/iptsd/wiki/Calibrating-iptsd
