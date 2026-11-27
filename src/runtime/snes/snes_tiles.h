#pragma once

#include "snes_ppu.h"

namespace snes_tiles {

inline uint16_t vram_word(const SnesPpu &ppu, unsigned address) {
    const unsigned offset = (address & 0x7FFF) * 2;
    return ppu.vram[offset] | (ppu.vram[offset + 1] << 8);
}

inline unsigned planar_pixel(uint16_t low_planes, uint16_t high_planes, unsigned bit) {
    const unsigned low = low_planes >> bit;
    const unsigned high = high_planes >> bit;
    return (low & 1) | ((low >> 7) & 2) | ((high & 1) << 2) | ((high >> 5) & 8);
}

} // namespace snes_tiles
