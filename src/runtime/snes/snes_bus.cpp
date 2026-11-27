#include "snes_bus.h"
#include "snes_hdma.h"

namespace snes_bus {
namespace {

uint8_t *mapped_wram(Cpu65816 *cpu, uint32_t address) {
    const uint32_t bank = address >> 16;
    const uint16_t offset = address & 0xFFFF;
    if (bank == 0x7E || bank == 0x7F) {
        return &cpu->work_ram[address - 0x7E0000];
    }
    if ((bank & 0x40) == 0 && offset < 0x2000) {
        return &cpu->work_ram[offset];
    }
    return nullptr;
}

void record_unmapped_access(Cpu65816 *cpu, uint32_t address) {
    if (!cpu->unmapped_access) {
        cpu->unmapped_access = true;
        cpu->unmapped_address = address;
    }
}

uint8_t *mapped_dma_register(Cpu65816 *cpu, uint16_t offset) {
    if (offset < 0x4300 || offset > 0x437F) {
        return nullptr;
    }
    const unsigned reg = offset & 0xF;
    if (reg > 0xB && reg != 0xF) {
        return nullptr;
    }
    return &cpu->dma[(offset >> 4) & 7][reg == 0xF ? 0xB : reg];
}

void run_dma(Cpu65816 *cpu, uint8_t channels) {
    for (unsigned index = 0; index < 8 && !cpu->unmapped_access; ++index) {
        if ((channels & (1u << index)) == 0) {
            continue;
        }
        auto &channel = cpu->dma[index];
        const auto mode = channel[0];
        const uint32_t bank = static_cast<uint32_t>(channel[4]) << 16;
        uint16_t offset = channel[2] | (channel[3] << 8);
        uint16_t remaining = channel[5] | (channel[6] << 8);
        const int increment = (mode & 8) ? 0 : ((mode & 0x10) ? -1 : 1);
        unsigned transferred = 0;
        do {
            const uint32_t cpu_address = bank | offset;
            const uint32_t port = 0x2100 | static_cast<uint8_t>(
                channel[1] + snes_hdma::port_offsets[mode & 7][transferred & 3]);
            const bool to_cpu = mode & 0x80;
            const auto byte = read_byte(cpu, to_cpu ? port : cpu_address);
            if (cpu->unmapped_access) {
                break;
            }
            write_byte(cpu, to_cpu ? cpu_address : port, byte);
            if (cpu->unmapped_access) {
                break;
            }
            offset += increment;
            ++transferred;
        } while (--remaining != 0);
        cpu->dma_bytes += transferred;
        channel[2] = offset & 0xFF;
        channel[3] = offset >> 8;
        channel[5] = remaining & 0xFF;
        channel[6] = remaining >> 8;
    }
}

} // namespace

uint8_t read_byte(Cpu65816 *cpu, uint32_t address) {
    if (const auto *byte = mapped_wram(cpu, address)) {
        return *byte;
    }
    const auto offset = static_cast<uint16_t>(address);
    if ((address & 0x400000) == 0) {
        if (offset >= 0x2140 && offset <= 0x217F) {
            return snes_apu_read(cpu->apu, offset & 3);
        }
        if (const auto *reg = mapped_dma_register(cpu, offset)) {
            return *reg;
        }
        if (offset == 0x4211) {
            const auto irq = cpu->timer_irq;
            cpu->timer_irq = false;
            return irq ? 0x80 : 0;
        }
        if (offset == 0x4212) {
            const bool hblank = cpu->ppu.dot >= 274 || cpu->ppu.dot == 0;
            return (hblank ? 0x40 : 0) | (cpu->ppu.scanline >= 225 ? 0x80 : 0) |
                   static_cast<uint8_t>(cpu->joypad_busy);
        }
        if (offset == 0x4213) {
            return cpu->cpu_io[1];
        }
        if (offset >= 0x4218 && offset <= 0x421F) {
            const unsigned index = offset - 0x4218;
            return cpu->joypad_latch[index / 2] >> ((index & 1) * 8);
        }
        if (offset == 0x4214 || offset == 0x4215) {
            return cpu->division_quotient >> ((offset - 0x4214) * 8);
        }
        if (offset == 0x4216 || offset == 0x4217) {
            return cpu->multiply_or_divide_remainder >> ((offset - 0x4216) * 8);
        }
        uint8_t value = 0;
        if (snes_ppu_read(cpu->ppu, offset, value)) {
            return value;
        }
    }
    const uint32_t rom_offset = address & 0x3FFFFF;
    if (((address & 0x400000) || (address & 0x8000)) && rom_offset < cpu->rom_size) {
        return cpu->rom[rom_offset];
    }
    record_unmapped_access(cpu, address);
    return 0;
}

void write_byte(Cpu65816 *cpu, uint32_t address, uint8_t value) {
    if (auto *byte = mapped_wram(cpu, address)) {
        *byte = value;
        return;
    }
    if ((address & 0x400000) == 0) {
        const auto offset = static_cast<uint16_t>(address);
        if (offset >= 0x2140 && offset <= 0x217F) {
            snes_apu_write(cpu->apu, offset & 3, value);
            return;
        }
        if (offset >= 0x2100 && offset <= 0x2133 && snes_ppu_write(cpu->ppu, offset, value)) {
            return;
        }
        if (auto *reg = mapped_dma_register(cpu, offset)) {
            *reg = value;
            return;
        }
        if (offset == 0x420B) {
            run_dma(cpu, value);
            return;
        }
        if (offset >= 0x4200 && offset <= 0x420D) {
            cpu->cpu_io[offset - 0x4200] = value;
            if (offset == 0x4200 && !(value & 0x30)) {
                cpu->timer_irq = false;
            }
            if (offset == 0x4203) {
                cpu->division_quotient = (value << 8) | cpu->cpu_io[2];
                cpu->multiply_or_divide_remainder = cpu->cpu_io[2] * value;
            }
            if (offset == 0x4206) {
                // The arithmetic unit currently completes within the calling native block.
                const uint16_t dividend = cpu->cpu_io[4] | (cpu->cpu_io[5] << 8);
                cpu->division_quotient = value ? dividend / value : 0xFFFF;
                cpu->multiply_or_divide_remainder = value ? dividend % value : dividend;
            }
            return;
        }
    }
    record_unmapped_access(cpu, address);
}

} // namespace snes_bus

void cpu_advance_raster(Cpu65816 *cpu, unsigned dots) {
    snes_apu_advance(cpu->apu, dots);
    const auto enabled = cpu->cpu_io[0] & 0x30;
    const unsigned htime = cpu->cpu_io[7] | ((cpu->cpu_io[8] & 1) << 8);
    const unsigned vtime = cpu->cpu_io[9] | ((cpu->cpu_io[10] & 1) << 8);
    while (dots != 0) {
        const unsigned end = cpu->ppu.dot + dots;
        const unsigned trigger = (enabled & 0x10) ? htime + 1 : 1;
        if (enabled && (!(enabled & 0x20) || cpu->ppu.scanline == vtime) &&
            trigger < 341 && cpu->ppu.dot < trigger && end >= trigger) {
            cpu->timer_irq = true;
        }
        if (end < 341) {
            cpu->ppu.dot = end;
            break;
        }
        if (cpu->ppu.scanline <= SNES_VISIBLE_LINES) {
            snes_hdma::end_scanline(cpu);
        }
        dots = end - 341;
        cpu->ppu.dot = 0;
        cpu->ppu.scanline = (cpu->ppu.scanline + 1) % 262;
        if (cpu->ppu.scanline == 0) {
            snes_hdma::begin_frame(cpu);
        } else if (cpu->ppu.scanline == SNES_VISIBLE_LINES / 2) {
            // Sample the display before vertical blanking and video transfers.
            cpu->ppu.visible_display = cpu->ppu.registers[0];
            if (!cpu->ppu.line_cgram_active) {
                ++cpu->frame_count;
            }
        } else if (cpu->ppu.scanline == SNES_VISIBLE_LINES + 1) {
            if (cpu->ppu.line_cgram_active) {
                ++cpu->frame_count;
            }
            snes_ppu_begin_vblank(cpu->ppu);
            cpu->joypad_busy = (cpu->cpu_io[0] & 1) != 0;
        } else if (cpu->ppu.scanline == 228 && cpu->joypad_busy) {
            for (unsigned port = 0; port < 4; ++port) {
                cpu->joypad_latch[port] = cpu->joypad_input[port];
            }
            cpu->joypad_busy = false;
        }
    }
}
