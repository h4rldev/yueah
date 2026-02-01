{pkgs ? import <nixpkgs> {}}:
with pkgs;
  mkShell {
    name = "yueah";
    description = "A small blogging backend";

    buildInputs = [
      cmake
      yyjson
      zlib
      libuv
      libbsd
      openssl
      pkg-config
      brotli
      wslay
      libcap
      sqlite
    ];

    packages = [
      file
      bear
      gcc
      clang-tools
      valgrind
      just
      gf
    ];
  }
