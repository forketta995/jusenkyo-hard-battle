# Runtime architecture

The build keeps the SNES runtime, Windows preview, and game code in separate
targets. Hard Battle is the first game linked to the runtime.

| Location | Responsibility |
| --- | --- |
| `src/runtime/snes/` | 65C816 instructions and control flow, SNES bus and DMA, raster timing, PPU rendering, and SPC700 audio driver execution. |
| `src/runtime/platform/win32/` | The preview window, keyboard input, frame pacing, drawing, and queued PCM playback. The game supplies the boot and frame callbacks. |
| `src/runtime/games/hard_battle/` | Shared game entry points, ROM loading, boot diagnostics, and preview behavior. |
| `src/runtime/rom_file.*` | Loading the ROM path and validating a local file against the expected length and CRC. |
| `config/hard_battle_*/` | Per-edition ROM fingerprint, lift rules, and preview addresses. |
| `cmake/HardBattleTargets.cmake` | Hard Battle target setup and edition selection. |
| `<build>/src/generated/<edition>/` | Generated C for the ROM selected for this build. |
| `third_party/snes_spc/` | Shared library for the clocked SPC700 and DSP. |

The generated C includes the shared 65C816 and execution-result headers.
The TOML `symbol_prefix` controls the exported boot and resume function names
and ROM metadata. Each game can choose its own prefix and link its generated
object into a separate target alongside the shared runtime.

Hard Battle builds one edition per build directory. The builder and CMake
use `tools/hard_battle_editions.py` to identify the profile by SHA-256. The
same executable and runtime targets are available for each profile. Control
flow addresses belong to `rom.toml` and addresses used by the Windows preview
belong to the edition's `preview.cmake`.

`hard_battle_content` resolves the experimental title graphic from a local
donor ROM. It validates the source edition and packed data header, then
rebinds the specific graphics-table entry in the in-memory USA cartridge
view. Both cartridge files stay read-only. Content overrides are applied
after the base ROM has been validated and before the game boots.

To add another supported SNES cartridge, give it a directory under `config/`
with its ROM fingerprint, symbol prefix, and control flow configuration. Use
`target_add_snes_recompilation` to attach its generated C to a game target.
Add the game's ROM validation and entry point under `src/runtime/games/`. For
a Windows preview, link the shared host and pass the game's boot and resume
callbacks.

`SnesAudio` owns the sound chip state and converts raster dots into SPC700
clocks. The SNES bus hands off the cartridge's uploaded audio driver after
the IPL transfer. The Windows preview consumes a PCM block per video frame.

The current lifter and runtime cover the HiROM mapping and the hardware
operations exercised by Hard Battle. Another cartridge may need additional
instructions, memory mapping, or rendering support. Its lift configuration
and game-specific checkpoints belong in its own module.
