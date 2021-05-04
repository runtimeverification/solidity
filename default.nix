let
  sources = import ./nix/sources.nix {};
  pkgs = import sources."nixpkgs" { config = {}; overlays = []; };
  ttuegel = import sources."ttuegel" { inherit pkgs; };
  inherit (pkgs) lib;
  iele-semantics =
    let
      tag = lib.fileContents ./ext/kiele_release;
      url = "https://github.com/runtimeverification/iele-semantics/releases/download/${tag}/release.nix";
      args = import (builtins.fetchurl { inherit url; });
      src = pkgs.fetchgit args;
    in import src {};

  default = {
    isolc = pkgs.callPackage ./nix/isolc.nix {
      inherit (ttuegel) cleanGitSubtree cleanSourceWith;
      inherit (iele-semantics) kiele;
      stdenv = pkgs.gccStdenv;
      boost = pkgs.boost17x.override { stdenv = pkgs.gccStdenv; };
    };
  };
in
  default
