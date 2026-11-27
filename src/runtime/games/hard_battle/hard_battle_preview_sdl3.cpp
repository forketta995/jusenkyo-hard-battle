#include "snes_preview.h"

#include "hard_battle_content.h"
#include "hard_battle_program.h"
#include "hard_battle_rom.h"

namespace {

bool boot_preview(Cpu65816 &cpu) {
    cpu.apply_preview_patches = true;
    return hard_battle_boot(&cpu) == BOOT_CHECKPOINT && cpu.ppu.registers[0] == 15;
}

constexpr SnesPreviewProgram program{"Ranma 1/2: Hard Battle", boot_preview, hard_battle_preview_advance};

} // namespace

int main(int argc, char **argv) {
    const auto directory = preview_executable_directory();
    if (directory.empty()) {
        return show_preview_error(program.name, "Could not locate the preview executable.");
    }
    // the content helpers resolve their selection files next to the executable
    const auto executable = directory / "hard_battle_preview";
    const auto rom_path = argc > 1 ? std::filesystem::path(argv[1]) : configured_hard_battle_rom(executable);
    auto rom = load_hard_battle_rom(rom_path);
    if (rom.empty()) {
        return show_preview_error(program.name, "Could not read the configured Hard Battle ROM.");
    }
    if (!apply_configured_hard_battle_content(rom, executable)) {
        return show_preview_error(program.name,
                                  "The title graphic requires a USA base and a valid Japan revision 0 ROM.");
    }
    return run_snes_preview(program, rom);
}
