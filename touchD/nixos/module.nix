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
  options.services.touchD.enable = lib.mkEnableOption
    "the touchD Surface Pro 5 IPTS profile and control utility";

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ touchdctl ];
    environment.etc."iptsd.d/80-touchd-surface-pro-5.conf".source =
      ../iptsd-surface-pro-5.conf;
    environment.variables = {
      TOUCHD_PROFILE = "\${../iptsd-surface-pro-5.conf}";
      TOUCHD_NIXOS_MANAGED = "1";
    };
  };
}
