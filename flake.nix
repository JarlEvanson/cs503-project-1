{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: 
  let
    pkgs = nixpkgs.legacyPackages.x86_64-linux;
  in {
    devShells."x86_64-linux".default = pkgs.mkShellNoCC {
      packages = [
        # Build system
        pkgs.gnumake
        
        # Compilers
        pkgs.clang
        pkgs.aflplusplus

        # Utility used to automate setup of fuzzing
        pkgs.tmux
      ];
    };
  };
}
