#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "snes_preview.h"

#include "hard_battle_content.h"
#include "hard_battle_program.h"
#include "hard_battle_rom.h"

namespace {

bool boot_preview(Cpu65816 &cpu) {
    cpu.apply_preview_patches = true;
    return hard_battle_boot(&cpu) == BOOT_CHECKPOINT && cpu.ppu.registers[0] == 15;
}

constexpr SnesPreviewProgram program{L"Ranma 1/2: Hard Battle", boot_preview, hard_battle_preview_advance};

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_mode) {
    const auto executable = preview_executable_path();
    if (executable.empty()) {
        return show_preview_error(program.name, L"Could not locate the preview executable.");
    }
    auto rom = load_hard_battle_rom(configured_hard_battle_rom(executable));
    if (rom.empty()) {
        return show_preview_error(program.name, L"Could not read the configured Hard Battle ROM.");
    }
    if (!apply_configured_hard_battle_content(rom, executable)) {
        return show_preview_error(program.name, L"The title graphic requires a USA base and a valid Japan revision 0 ROM.");
    }
    return run_snes_preview(instance, show_mode, program, rom);
}
