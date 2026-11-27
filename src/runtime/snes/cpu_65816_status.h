#pragma once

#include "cpu_65816.h"

namespace cpu65816 {

inline void set_flag(Cpu65816 *cpu, uint8_t flag, bool value) {
    cpu->status = value ? cpu->status | flag : cpu->status & ~flag;
}

inline void normalize_status(Cpu65816 *cpu) {
    if (cpu->emulation) {
        cpu->status |= CPU_INDEX_8 | CPU_ACCUMULATOR_8;
        cpu->stack = 0x0100 | (cpu->stack & 0xFF);
    }
    if (cpu->status & CPU_INDEX_8) {
        cpu->x &= 0xFF;
        cpu->y &= 0xFF;
    }
}

inline uint16_t accumulator_mask(const Cpu65816 *cpu) {
    return (cpu->status & CPU_ACCUMULATOR_8) ? 0xFF : 0xFFFF;
}

inline void set_nz(Cpu65816 *cpu, uint16_t value, uint16_t mask) {
    set_flag(cpu, CPU_ZERO, (value & mask) == 0);
    set_flag(cpu, CPU_NEGATIVE, (value & ((mask + 1) >> 1)) != 0);
}

} // namespace cpu65816
