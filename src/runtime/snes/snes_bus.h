#pragma once

#include "cpu_65816.h"

namespace snes_bus {

uint8_t read_byte(Cpu65816 *cpu, uint32_t address);
void write_byte(Cpu65816 *cpu, uint32_t address, uint8_t value);

} // namespace snes_bus
