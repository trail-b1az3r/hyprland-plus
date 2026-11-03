{ config, lib, pkgs, ... }:

let
  cfg = config.services.touchD;
  touchdctl = pkgs.writeShellApplication {
    name = "touchdctl";
    runtimeInputs = with pkgs; [
      coreutils
      gnugrep
      procps
      systemd
    ];
    text = ''
      export TOUCHD_PROFILE="${../iptsd-surface-pro-5.conf}"
      export TOUCHD_NIXOS_MANAGED=1
      ${builtins.readFile ../touchdctl}
    '';
  };
in
{
  options.services.touchD = {
    enable = lib.mkEnableOption
      "the touchD Surface IPTS profile (Surface Pro 4-6, Book, Laptop) and control utility";

    # `sudo touchdctl calibrate --apply` prints these for you on NixOS.
    calibration = lib.mkOption {
      type = lib.types.nullOr (lib.types.submodule {
        options = {
          SizeMin = lib.mkOption { type = lib.types.float; };
          SizeMax = lib.mkOption { type = lib.types.float; };
          AspectMin = lib.mkOption { type = lib.types.float; };
          AspectMax = lib.mkOption { type = lib.types.float; };
        };
      });
      default = null;
      example = { SizeMin = 0.21; SizeMax = 1.9; AspectMin = 1.0; AspectMax = 2.4; };
      description = "Contact sizes measured with touchdctl calibrate, written to /etc/iptsd.d/90-calibration.conf.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ touchdctl ];
    environment.etc."iptsd.d/80-touchd-surface-pro-5.conf".source =
      ../iptsd-surface-pro-5.conf;
    environment.etc."iptsd.d/90-calibration.conf" = lib.mkIf (cfg.calibration != null) {
      text = ''
        [Contacts]
        SizeMin = ${toString cfg.calibration.SizeMin}
        SizeMax = ${toString cfg.calibration.SizeMax}
        AspectMin = ${toString cfg.calibration.AspectMin}
        AspectMax = ${toString cfg.calibration.AspectMax}
      '';
    };
  };
}
