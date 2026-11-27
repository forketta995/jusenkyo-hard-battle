#include "snes_hdma.h"
#include "snes_bus.h"

#include <algorithm>

namespace snes_hdma {
namespace {

uint16_t word(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8);
}

void store_word(uint8_t *bytes, uint16_t value) {
    bytes[0] = value & 0xFF;
    bytes[1] = value >> 8;
}

uint8_t read_next(Cpu65816 *cpu, uint8_t bank, uint8_t *pointer) {
    const auto offset = word(pointer);
    store_word(pointer, offset + 1);
    return snes_bus::read_byte(cpu, (static_cast<uint32_t>(bank) << 16) | offset);
}

void reload(Cpu65816 *cpu, unsigned index) {
    auto &channel = cpu->dma[index];
    const uint8_t bit = 1u << index;
    channel[10] = read_next(cpu, channel[4], &channel[8]);
    if (channel[10] == 0) {
        cpu->hdma_active &= ~bit;
        cpu->hdma_transfer &= ~bit;
        return;
    }
    cpu->hdma_transfer |= bit;
    if (channel[0] & 0x40) {
        channel[5] = read_next(cpu, channel[4], &channel[8]);
        channel[6] = read_next(cpu, channel[4], &channel[8]);
    }
}

void transfer(Cpu65816 *cpu, uint8_t *channel) {
    constexpr unsigned lengths[] = {1, 2, 2, 4, 4, 4, 2, 4};
    const unsigned mode = channel[0] & 7;
    const bool indirect = channel[0] & 0x40;
    const bool to_cpu = channel[0] & 0x80;
    const auto bank = channel[indirect ? 7 : 4];
    auto *pointer = &channel[indirect ? 5 : 8];
    for (unsigned i = 0; i < lengths[mode] && !cpu->unmapped_access; ++i) {
        const unsigned port = 0x2100 | static_cast<uint8_t>(channel[1] + port_offsets[mode][i]);
        if (to_cpu) {
            const uint16_t offset = word(pointer);
            store_word(pointer, offset + 1);
            const auto byte = snes_bus::read_byte(cpu, port);
            snes_bus::write_byte(cpu, (static_cast<uint32_t>(bank) << 16) | offset, byte);
        } else {
            const auto byte = read_next(cpu, bank, pointer);
            snes_bus::write_byte(cpu, port, byte);
        }
    }
}

} // namespace

void begin_frame(Cpu65816 *cpu) {
    cpu->hdma_active = cpu->cpu_io[0xC];
    cpu->hdma_transfer = 0;
    cpu->ppu.line_cgram_active = cpu->hdma_active != 0;
    for (unsigned index = 0; index < 8 && !cpu->unmapped_access; ++index) {
        if (cpu->hdma_active & (1u << index)) {
            auto &channel = cpu->dma[index];
            store_word(&channel[8], word(&channel[2]));
            reload(cpu, index);
        }
    }
}

void end_scanline(Cpu65816 *cpu) {
    auto &ppu = cpu->ppu;
    if (!ppu.line_cgram_active) {
        return;
    }
    if (ppu.scanline >= 1 && ppu.scanline <= SNES_VISIBLE_LINES) {
        // HBlank transfers take effect on the next visible line.
        std::ranges::copy(ppu.cgram, ppu.line_cgram[ppu.scanline - 1]);
    }
    const unsigned active = cpu->hdma_active & cpu->cpu_io[0xC];
    ppu.hdma_writing = true;
    for (unsigned index = 0; index < 8 && !cpu->unmapped_access; ++index) {
        const unsigned bit = 1u << index;
        if ((active & bit) == 0) {
            continue;
        }
        auto &channel = cpu->dma[index];
        if (cpu->hdma_transfer & bit) {
            transfer(cpu, channel);
        }
        --channel[10];
        if ((channel[10] & 0x7F) == 0) {
            reload(cpu, index);
        } else if ((channel[10] & 0x80) == 0) {
            cpu->hdma_transfer &= ~bit;
        }
    }
    ppu.hdma_writing = false;
}

} // namespace snes_hdma
