{
  description = "C/C++ Development Environment with wxWidgets and OpenSSL";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {

        # Tools needed to build the project
        nativeBuildInputs = with pkgs; [
          pkg-config
          cmake
          gnumake
        ];

        # Libraries needed for linking
        buildInputs = with pkgs; [
          lz4
          zstd
          openssl
          curl
          wxwidgets_3_2
        ];

        # Optional: A welcome message when you enter the shell
        shellHook = ''
          echo "C/C++ development shell active."
          echo "Compiler toolchain, CMake, and wxWidgets are ready."
        '';
      };
    };
}
