#pragma once

#include "cpu_65816.h"
#include "snes_program.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t hard_battle_rom_size;
extern const uint32_t hard_battle_rom_crc;
BootResult hard_battle_boot(Cpu65816 *cpu);
BootResult hard_battle_continue(Cpu65816 *cpu);

#ifdef __cplusplus
}

BootResult hard_battle_preview_advance(Cpu65816 *cpu);
#endif
