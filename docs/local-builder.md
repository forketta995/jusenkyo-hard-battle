# Hard Battle local builder

The Windows builder verifies a local Hard Battle ROM by SHA-256, selects its
edition profile, lifts its code, compiles the preview and checks that it boots.
It installs into a new directory. The ROM stays at its original path and is
required to play. The builder accepts USA revision 0 and the experimental
Japan revision 0 profile.

This developer preview requires Python 3.11 or later, CMake 3.20 or later,
Ninja, and a 64-bit MinGW-w64 GCC toolchain with C++20 support. The tools can
be on `PATH` or selected as a group with `--toolchain-bin`.

Run from the source checkout or the extracted builder ZIP:

```powershell
python tools/build_hard_battle.py --rom 'C:\Games\Hard Battle (USA).sfc' --out 'C:\Games\Hard Battle Preview'
```

On success, the builder prints the path to `hard_battle_preview.exe`. Keep the
executable, `ramma_spc.dll` and the generated `hard_battle_rom_path.txt`
together. The builder uses a separate temporary build tree and publishes
the installation after the build and boot check pass.

To try the Japanese title graphic with a USA base, add
`--title-graphic-rom 'C:\Games\Hard Battle (Japan).smc'` and use a separate
installation directory. The preview reads the graphic from the local donor
ROM and uses the USA game's own graphics loader to render it.

To make a builder ZIP, run from the source checkout:

```powershell
python tools/package_builder.py --out dist/jusenkyo-hard-battle-builder-v0.1.0-alpha.1-windows.zip
```

The ZIP includes the lifter, game configuration, runtime sources, and the
SPC library with its license. It requires the Windows tools described above.
