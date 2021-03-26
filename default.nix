let
  sources = import ./nix/sources.nix {};
  pkgs = import sources."nixpkgs" { config = {}; overlays = []; };
  ttuegel = import sources."ttuegel" { inherit pkgs; };

  default = {
    solc= pkgs.callPackage ./nix/solc.nix {
      inherit (ttuegel) cleanGitSubtree;
    };
  };
in
  default
