#include "hard_battle_diagnostics.h"
#include "hard_battle_program.h"
#include "hard_battle_rom.h"

#include <filesystem>
#include <iostream>

int run_hard_battle_diagnostics(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "Usage: ramma_recomp [path-to-rom]\n";
        return 1;
    }
    std::filesystem::path rom_path;
    if (argc == 2) {
        rom_path = argv[1];
    } else {
        rom_path = configured_hard_battle_rom(argv[0]);
        if (rom_path.empty()) {
            std::cerr << "ROM path missing. Pass the path to your Hard Battle USA ROM.\n";
            return 1;
        }
    }

    const auto rom = load_hard_battle_rom(rom_path);
    if (rom.empty()) {
        std::cerr << "Could not read the configured Hard Battle ROM. Check the path and revision.\n";
        return 1;
    }

    Cpu65816 cpu;
    cpu_reset(&cpu);
    cpu.rom = rom.data();
    cpu.rom_size = hard_battle_rom_size;
    std::cout << "Hard Battle\n"
              << "1. ROM verified.\n";
    switch (hard_battle_boot(&cpu)) {
    case BOOT_CHECKPOINT:
        if (cpu.apu.phase != APU_PROGRAM_LOADED || cpu.dma_bytes < sizeof(cpu.ppu.vram)) {
            std::cerr << "Boot stopped before the audio upload and video memory initialization finished.\n";
            return 1;
        }
        if (!spc700_start_driver(cpu.apu)) {
            std::cerr << "SPC700 startup stopped at $" << std::hex << cpu.apu.driver.pc
                      << " (opcode $" << static_cast<unsigned>(cpu.apu.driver_opcode) << ").\n";
            return 1;
        }
        std::cout << "2. Startup memory initialized.\n"
                  << "3. Audio program copied to the sound chip ("
                  << cpu.apu.uploaded_bytes << " bytes).\n"
                  << "   The audio driver initialized its DSP registers and timers.\n"
                  << "   The audio driver acknowledged " << cpu.apu.acknowledged_packets
                  << " command and data packets.\n"
                  << "4. Opening scene graphics prepared (" << cpu.dma_bytes
                  << " DMA bytes, including VRAM clearing).\n"
                  << "5. Headless boot test complete.\n";
        break;
    case BOOT_UNMAPPED_ACCESS:
        std::cerr << "Boot stopped at an unmapped bus access ($" << std::hex
                  << cpu.unmapped_address << ").\n";
        return 1;
    case BOOT_AUDIO_FAULT:
        std::cerr << "SPC700 could not complete its command at $" << std::hex
                  << cpu.apu.driver.pc << " (opcode $"
                  << static_cast<unsigned>(cpu.apu.driver_opcode) << ").\n";
        return 1;
    case BOOT_UNKNOWN_BLOCK:
        std::cerr << "Boot reached a block without native code ($" << std::hex << cpu.pc << ").\n";
        return 1;
    case BOOT_STEP_LIMIT:
        std::cerr << "Boot exceeded the block execution limit.\n";
        return 1;
    case BOOT_FRAME:
        return 1;
    }
    return 0;
}
