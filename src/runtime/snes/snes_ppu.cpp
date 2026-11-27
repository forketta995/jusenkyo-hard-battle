#include "snes_ppu.h"

namespace {

bool forced_blank(const SnesPpu &ppu) {
    return (ppu.registers[0] & 0x80) != 0;
}

bool video_memory_accessible(const SnesPpu &ppu) {
    return forced_blank(ppu) || ppu.scanline >= 225;
}

unsigned mapped_vram_byte(const SnesPpu &ppu) {
    const auto mode = ppu.registers[0x15];
    auto address = ppu.vram_address;
    const unsigned mapping = (mode >> 2) & 3;
    if (mapping != 0) {
        const unsigned shift = mapping + 4;
        const auto low_mask = (1u << shift) - 1;
        const auto bank_mask = (1u << (shift + 3)) - 1;
        address = (address & ~bank_mask) | ((address & low_mask) << 3) |
                  ((address >> shift) & 7);
    }
    return (address & 0x7FFF) * 2;
}

void latch_vram(SnesPpu &ppu) {
    const auto address = mapped_vram_byte(ppu);
    ppu.vram_latch = ppu.vram[address] | (ppu.vram[address + 1] << 8);
}

void advance_vram(SnesPpu &ppu, bool high) {
    const auto mode = ppu.registers[0x15];
    if (high == ((mode & 0x80) != 0)) {
        constexpr uint16_t increments[] = {1, 32, 128, 128};
        ppu.vram_address += increments[mode & 3];
    }
}

void write_oam(SnesPpu &ppu, uint8_t value) {
    const auto address = ppu.oam_address;
    ppu.oam_address = (address + 1) & 0x3FF;
    if (address >= 0x200) {
        ppu.oam[0x200 | (address & 0x1F)] = value;
    } else if (address & 1) {
        ppu.oam[address - 1] = ppu.oam_latch;
        ppu.oam[address] = value;
    } else {
        ppu.oam_latch = value;
    }
}

void reset_oam_address(SnesPpu &ppu) {
    ppu.oam_address = (ppu.registers[2] << 1) | ((ppu.registers[3] & 1) << 9);
}

} // namespace

void snes_ppu_begin_vblank(SnesPpu &ppu) {
    if (!forced_blank(ppu)) {
        reset_oam_address(ppu);
    }
}

bool snes_ppu_write(SnesPpu &ppu, uint16_t address, uint8_t value) {
    // Access during active display needs scanline timing, which the boot runtime does not model.
    switch (address) {
    case 0x2104:
    case 0x2118:
    case 0x2119:
    case 0x2122:
        if (!video_memory_accessible(ppu) && !(address == 0x2122 && ppu.hdma_writing)) {
            return false;
        }
        break;
    }

    const auto index = address - 0x2100;
    if (address == 0x2100 && forced_blank(ppu) && ppu.scanline == 225) {
        reset_oam_address(ppu);
    }
    ppu.registers[index] = value;
    if (address == 0x210D || address == 0x210E || (address >= 0x211B && address <= 0x2120)) {
        if (address == 0x211B) {
            ppu.mode7_a = (value << 8) | ppu.mode7_latch;
        }
        ppu.mode7_latch = value;
    }
    if (address >= 0x210D && address <= 0x2114) {
        const unsigned scroll_register = address - 0x210D;
        const bool vertical = scroll_register & 1;
        const uint8_t low = vertical ? ppu.scroll_latch
            : (ppu.scroll_latch & 0xF8) | (ppu.horizontal_scroll_latch & 7);
        ppu.scroll[scroll_register >> 1][vertical] = (value << 8) | low;
        ppu.scroll_latch = value;
        if (!vertical) {
            ppu.horizontal_scroll_latch = value;
        }
        return true;
    }
    switch (address) {
    case 0x2102:
    case 0x2103:
        reset_oam_address(ppu);
        break;
    case 0x2104:
        write_oam(ppu, value);
        break;
    case 0x2116:
        ppu.vram_address = (ppu.vram_address & 0xFF00) | value;
        latch_vram(ppu);
        break;
    case 0x2117:
        ppu.vram_address = (ppu.vram_address & 0x00FF) | (value << 8);
        latch_vram(ppu);
        break;
    case 0x2118:
    case 0x2119: {
        const bool high = address == 0x2119;
        ppu.vram[mapped_vram_byte(ppu) + high] = value;
        advance_vram(ppu, high);
        break;
    }
    case 0x2121:
        ppu.cgram_address = value;
        ppu.cgram_high_byte = 0;
        break;
    case 0x2122:
        if (ppu.cgram_high_byte) {
            ppu.cgram[ppu.cgram_address++] = ppu.cgram_latch | ((value & 0x7F) << 8);
        } else {
            ppu.cgram_latch = value;
        }
        ppu.cgram_high_byte ^= 1;
        break;
    }
    return true;
}

bool snes_ppu_read(SnesPpu &ppu, uint16_t address, uint8_t &value) {
    if (address >= 0x2134 && address <= 0x2136) {
        const int32_t result = static_cast<int16_t>(ppu.mode7_a) *
                               static_cast<int8_t>(ppu.registers[0x1C]);
        value = static_cast<uint32_t>(result) >> ((address - 0x2134) * 8);
        return true;
    }
    if (address == 0x2137) {
        ppu.latched_scanline = ppu.scanline;
        ppu.latched_dot = ppu.dot;
        value = 0;
        return true;
    }
    if (address == 0x2138) {
        const auto index = ppu.oam_address;
        ppu.oam_address = (ppu.oam_address + 1) & 0x3FF;
        value = video_memory_accessible(ppu) ? ppu.oam[index >= 0x200 ? 0x200 | (index & 0x1F) : index] : 0;
        return true;
    }
    if (address == 0x213C) {
        value = ppu.read_dot_high ? ppu.latched_dot >> 8 : ppu.latched_dot;
        ppu.read_dot_high = !ppu.read_dot_high;
        return true;
    }
    if (address == 0x213D) {
        value = ppu.read_scanline_high ? ppu.latched_scanline >> 8 : ppu.latched_scanline;
        ppu.read_scanline_high = !ppu.read_scanline_high;
        return true;
    }
    if (address == 0x213F) {
        ppu.read_scanline_high = false;
        ppu.read_dot_high = false;
        value = 3;
        return true;
    }
    if ((address != 0x2139 && address != 0x213A) || !video_memory_accessible(ppu)) {
        return false;
    }
    const bool high = address == 0x213A;
    value = ppu.vram_latch >> (high ? 8 : 0);
    if (high == ((ppu.registers[0x15] & 0x80) != 0)) {
        latch_vram(ppu);
        advance_vram(ppu, high);
    }
    return true;
}
