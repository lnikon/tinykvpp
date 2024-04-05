{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };
      tinykvpp = (with pkgs; stdenv.mkDerivation {
        name = "tinykvpp";
        src = fetchgit {
          url = "https://github.com/lnikon/tinykvpp";
          sha256 = "76EuOwIbxshtkaf8z50hmmUyFj0TTdF+HJbnNlvQDrY=";
          fetchSubmodules = true;
        };
        nativeBuildInputs = [
          cmake
          clang
          boost
          grpc
          protobuf
          spdlog
          fmt
          re2
          abseil-cpp
          cmake
          vim
          catch2_3
          systemd
          openssl
        ];
        buildPhase = "make -j $NIX_BUILD_CORES";
      }
      );
    in
    rec {
      defaultApp = flake-utils.lib.mkApp {
        drv = defaultPackage;
      };
      defaultPackage = tinykvpp;
      devShell = pkgs.mkShell {
        buildInputs = [
          tinykvpp
        ];
      };
    }
  );
}
