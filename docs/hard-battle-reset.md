# Hard Battle title preview

The first target is the unmodified SNES USA revision 0. In the Jusenkyo
workspace the ROM stays in `../roms/` and is checked against the SHA-256 in
`config/hard_battle_usa/rom.toml`.
The Japanese revision 0 can be compiled with the same tools and its own
`config/hard_battle_jpn/` profile. Use a different build directory to keep
the Japanese and USA previews available side by side.
See [Runtime architecture](runtime-architecture.md) for the shared SNES code,
the Windows host, and the per-game integration.

## Build and run on Windows

Use Python 3.11 or later, CMake, Ninja, and a C/C++ compiler. Run these commands
from `jusenkyo-hard-battle/`. For a standalone checkout, set `$hardBattleRom`
to the absolute path of your local ROM.

```powershell
$hardBattleRom = (Get-Item -LiteralPath '../roms/Ranma 1-2 - Hard Battle (USA).sfc').FullName
cmake -B build -G Ninja "-DRAMMA_HARD_BATTLE_ROM=$hardBattleRom"
cmake --build build
.\build\ramma_recomp.exe
```

On Windows, open `build/hard_battle_preview.exe`. It reads the same configured
ROM as the headless executable. Wait for **PUSH START BUTTON**, then press Enter
to open the title menu. The preview keeps the title active while idle. Use
the arrow keys to move the selection and Enter to
confirm 1P. Press Enter during the opening dialogue to move on to **PLAYER
SELECT**. Use Left and Right to cycle through the ten portraits and X to
confirm. Choose the portrait at the top to see Ranma's first cutscene.
The dialogue advances automatically. Enter skips the rest of the cutscene.
Wait for the first round, then use Left and Right to move and Z for the
SNES B attack. Enter pauses and resumes the fight. After losing the match,
use Up and Down on **CONTINUE?**
and press X to confirm. **YES** restarts the match. **NO** opens the final
screen, from which X returns to the title. Press F5 to restart from the
title or Escape to close the window.

For Windows playtesting, keep `hard_battle_preview.exe` and `ramma_spc.dll`
in the same folder. Each tester puts the absolute path of their own USA
revision 0 ROM on one line in `hard_battle_rom_path.txt` beside the executable.
The SPC library source and license are in `third_party/snes_spc/`.

The build verifies the ROM, reads its reset vector, and translates the reachable
startup blocks through the configured checkpoints in
`build/src/generated/hard_battle_usa/reset.c`. It compiles this C alongside the C++
runtime. At launch, the program reads the ROM for data and checks its size and
CRC-32 against the ROM used to build the executable. Pass a ROM path as the
first argument to override the path recorded by CMake.

With zero-initialized work RAM, the executable should report:

```text
Hard Battle
1. ROM verified.
2. Startup memory initialized.
3. Audio program copied to the sound chip (5490 bytes).
   The audio driver initialized its DSP registers and timers.
   The audio driver acknowledged 14173 command and data packets.
4. Opening scene graphics prepared (167776 DMA bytes, including VRAM clearing).
5. Headless boot test complete.
```

On cold boot, the generated code prepares work RAM and loads the initial
video and CPU registers from cartridge tables. The game's native upload
routine sends its audio program through the four APU ports into sound RAM.
The Windows preview runs the uploaded driver on the clocked SPC700 and DSP
from `snes_spc`. The main CPU exchanges commands and sample data through the
four ports, and the DSP renders 32 kHz stereo PCM from the cartridge's sound
banks. The preview queues the samples to the default Windows audio output.
Headless boot diagnostics use the port-driven SPC700 startup path to report
program upload and packet acknowledgments.

The game's native video command queue clears VRAM and uploads opening-scene
tiles and tilemaps. The code follows the game's stack-based scene and
decompression dispatches. The raster model delivers timer interrupts and
executes the game's title fade. The preview resumes the translated code on
each frame and feeds keyboard input through the SNES controller registers.
The Mode 1 renderer composes each visible frame from VRAM, CGRAM, and the
active registers. The renderer also composites the main-screen sprites from
OAM, their VRAM tiles, and their CGRAM palettes. The original title menu
and 1P dialogue run through the game's code. The original character select
screen then loads its portraits, cursor and graphics. Horizontal DMA applies
its palette changes by scanline. The renderer samples the other display
state for the whole frame. Raster timing is approximate at native block
boundaries and the compositor uses the Mode 1 main screen.

Ranma's opening cutscene uses the same 1P script runner. The translated game
loads each portrait and types dialogue through its original text callbacks,
then scrolls and fades the scene before the match. The test covers the full
sequence from character confirmation through arena entry and movement.
The Windows preview selects the original opponent lineup that starts with
Akane for Ranma, so the first fight uses the verified arena and opponent.
The fight runs the cartridge's movement, animation, AI, and damage routines.
With the clocked SPC700 attached, the arena waits for the announcer through
the game's original sound protocol.
Round results, the rematch and the continue menu use the original game code.
A win shows the victory screen and the preview stops before the next
opponent.

The object layout and draw order were checked against the
[ares object implementation](https://github.com/ares-emulator/ares/blob/master/ares/sfc/ppu/object.cpp).
The horizontal DMA table handling follows the behavior documented in the
[ares DMA implementation](https://github.com/ares-emulator/ares/blob/master/ares/sfc/cpu/dma.cpp).

The block move implementation follows the operand encoding in the
[W65C816S datasheet](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf).
The video register behavior can be cross-checked in the
[ares PPU implementation](https://github.com/ares-emulator/ares/blob/master/ares/sfc/ppu/io.cpp).
The port handshakes are described in
[Booting the SPC700](https://snes.nesdev.org/wiki/Booting_the_SPC700).
