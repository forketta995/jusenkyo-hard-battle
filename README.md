# Jusenkyo Hard Battle

A static recompilation preview for the SNES fighting game *Ranma ½: Hard
Battle*. The Windows builder generates and compiles the game code on the player's
PC from a local ROM.

## Preview scope

| Edition | Verified path |
| --- | --- |
| USA revision 0 | Title, menu, character select, Ranma's opening cutscene, and the first match against Akane. |
| Japan revision 0 | The same opening path through the first match, pause, Continue, and rematch. |

Both editions use the cartridge's original sound driver. The USA build can
optionally load the Japanese title graphic from a second local ROM. PAL
editions and assets swapping are planned.

## Build and play

Use Python 3.11 or later, CMake 3.20 or later, Ninja, and 64-bit MinGW-w64.
Run the builder from a checkout or an extracted builder ZIP.

```powershell
python tools/build_hard_battle.py --rom 'C:\Games\Ranma Hard Battle (USA).sfc' --out 'build/preview-usa'
```

Open `build/preview-usa/hard_battle_preview.exe`. Use Enter for Start, the
arrow keys for movement, and Z and X for the SNES B and A buttons. Escape
closes the preview. F5 returns to the startup sequence.

### macOS and Linux (SDL3)

Install CMake, Ninja, Python 3.11+ and SDL3 (`brew install cmake ninja sdl3`),
then build with CMake directly:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAMMA_HARD_BATTLE_ROM="/path/to/Ranma 1-2 - Hard Battle (USA).sfc"
cmake --build build
./build/hard_battle_preview
```

The keyboard layout matches Windows. A gamepad also works: D-pad, Start,
South for B, East for A, Back to restart. F12 saves the current frame as a
BMP beside the executable. `cmake --install build --prefix <dir> --component
HardBattlePreview` copies a self-contained preview folder.

`SNES_PREVIEW_SCRIPT="frame:command;..."` drives the preview without a
keyboard: a hex joypad mask held from that frame, `shot` to save the
presented window, or `quit`.

- [Local builder and requirements](docs/local-builder.md)
- [Developer build and controls](docs/hard-battle-reset.md)
- [Runtime architecture](docs/runtime-architecture.md)
- [Release preparation](docs/release.md)

The builder package contains source and configuration files. Supply your own
ROM to build and run the preview. The bundled SPC700 and DSP library comes
with its own [license and attribution](third_party/snes_spc/README.md).
