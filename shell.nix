{ pkgs ? import <nixpkgs> { } }:

with pkgs;

mkShell {
  packages = [
    arduino-cli
    (python311.withPackages (ps: [
      ps.pyserial
    ]))
  ];
  shellHook = ''
  '';
}
