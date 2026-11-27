#include "hard_battle_content.h"
#include "hard_battle_program.h"
#include "hard_battle_rom.h"
#include "snes_audio.h"
#include "snes_frame.h"

#include <iostream>
#include <span>

namespace {

bool prepare_title(Cpu65816 &cpu, SnesAudio &audio, std::span<const uint8_t> rom, SnesFrame &frame) {
    cpu_reset(&cpu);
    cpu.rom = rom.data();
    cpu.rom_size = static_cast<uint32_t>(rom.size());
    cpu.apply_preview_patches = true;
    audio.attach(cpu.apu);
    return hard_battle_boot(&cpu) == BOOT_CHECKPOINT && cpu.ppu.registers[0] == 15 &&
           snes_render_mode1(cpu.ppu, frame);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        return 1;
    }
    auto rom = load_hard_battle_rom(argv[1]);
    Cpu65816 cpu;
    SnesAudio audio;
    SnesFrame normal;
    SnesFrame mixed;
    if (rom.empty() || !prepare_title(cpu, audio, rom, normal) ||
        apply_japanese_title_graphic(rom, argv[1]) || !apply_japanese_title_graphic(rom, argv[2]) ||
        !prepare_title(cpu, audio, rom, mixed)) {
        std::cerr << "The title graphic could not be composed from the two ROMs.\n";
        return 1;
    }
    unsigned changed_pixels = 0;
    for (size_t i = 0; i < normal.pixels.size(); ++i) {
        changed_pixels += normal.pixels[i] != mixed.pixels[i];
    }
    if (changed_pixels < 100) {
        std::cerr << "The Japanese title graphic did not change the rendered frame.\n";
        return 1;
    }
    audio.take_samples();
    for (unsigned i = 0; i < 240; ++i) {
        cpu.joypad_input[0] = i == 180 ? JOYPAD_START : 0;
        if (hard_battle_preview_advance(&cpu) != BOOT_FRAME) {
            return 1;
        }
        audio.take_samples();
    }
    const unsigned slot = cpu.work_ram[0x160];
    const unsigned callback = cpu.work_ram[0x162 + slot] | (cpu.work_ram[0x163 + slot] << 8);
    if (callback != 0xA1DB) {
        std::cerr << "The mixed USA title did not open the USA menu.\n";
        return 1;
    }
}
