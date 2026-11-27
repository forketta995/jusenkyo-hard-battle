#pragma once

#include "cpu_65816.h"

namespace snes_hdma {

inline constexpr uint8_t port_offsets[8][4] = {
    {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
    {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
};

void begin_frame(Cpu65816 *cpu);
void end_scanline(Cpu65816 *cpu);

} // namespace snes_hdma
