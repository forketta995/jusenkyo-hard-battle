#include "cpu_65816.h"

#include "cpu_65816_status.h"
#include "snes_bus.h"

using snes_bus::read_byte;
using snes_bus::write_byte;
using cpu65816::normalize_status;
using cpu65816::set_nz;

namespace {

void push(Cpu65816 *cpu, uint8_t value) {
    write_byte(cpu, cpu->stack, value);
    --cpu->stack;
    normalize_status(cpu);
}

uint8_t pull(Cpu65816 *cpu) {
    ++cpu->stack;
    normalize_status(cpu);
    return read_byte(cpu, cpu->stack);
}

uint16_t pull_word(Cpu65816 *cpu) {
    uint16_t value = pull(cpu);
    value |= pull(cpu) << 8;
    return value;
}

} // namespace

void cpu_push_accumulator(Cpu65816 *cpu) {
    if (!(cpu->status & CPU_ACCUMULATOR_8)) {
        push(cpu, cpu->accumulator >> 8);
    }
    push(cpu, cpu->accumulator & 0xFF);
}

void cpu_pull_accumulator(Cpu65816 *cpu) {
    uint16_t value = pull(cpu);
    if (!(cpu->status & CPU_ACCUMULATOR_8)) {
        value |= pull(cpu) << 8;
    }
    cpu_load_accumulator(cpu, value);
}

void cpu_push_data_bank(Cpu65816 *cpu) {
    push(cpu, cpu->data_bank);
}

void cpu_push_program_bank(Cpu65816 *cpu, uint8_t bank) {
    push(cpu, bank);
}

void cpu_push_word(Cpu65816 *cpu, uint16_t value) {
    push(cpu, value >> 8);
    push(cpu, value & 0xFF);
}

void cpu_push_x(Cpu65816 *cpu) {
    if (cpu->status & CPU_INDEX_8) {
        push(cpu, cpu->x);
    } else {
        cpu_push_word(cpu, cpu->x);
    }
}

void cpu_push_y(Cpu65816 *cpu) {
    if (cpu->status & CPU_INDEX_8) {
        push(cpu, cpu->y);
    } else {
        cpu_push_word(cpu, cpu->y);
    }
}

void cpu_pull_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, (cpu->status & CPU_INDEX_8) ? pull(cpu) : pull_word(cpu));
}

void cpu_pull_y(Cpu65816 *cpu) {
    cpu_load_y(cpu, (cpu->status & CPU_INDEX_8) ? pull(cpu) : pull_word(cpu));
}

void cpu_push_direct_page(Cpu65816 *cpu) {
    cpu_push_word(cpu, cpu->direct_page);
}

void cpu_pull_direct_page(Cpu65816 *cpu) {
    cpu->direct_page = pull_word(cpu);
    set_nz(cpu, cpu->direct_page, 0xFFFF);
}

void cpu_push_status(Cpu65816 *cpu) {
    push(cpu, cpu->status);
}

void cpu_pull_status(Cpu65816 *cpu) {
    cpu->status = pull(cpu);
    normalize_status(cpu);
}

void cpu_pull_data_bank(Cpu65816 *cpu) {
    cpu->data_bank = pull(cpu);
    set_nz(cpu, cpu->data_bank, 0xFF);
}

void cpu_move_block_forward(Cpu65816 *cpu, uint16_t banks) {
    cpu->data_bank = banks & 0xFF;
    const uint32_t destination_bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    const uint32_t source_bank = static_cast<uint32_t>(banks & 0xFF00) << 8;
    // A forward block move can intentionally copy overlapping ranges.
    do {
        const auto value = read_byte(cpu, source_bank | cpu->x);
        write_byte(cpu, destination_bank | cpu->y, value);
        ++cpu->x;
        ++cpu->y;
    } while (cpu->accumulator-- != 0 && !cpu->unmapped_access);
}

void cpu_call_subroutine(Cpu65816 *cpu, uint32_t target, uint16_t return_address) {
    cpu_push_word(cpu, return_address);
    cpu->pc = target;
}

void cpu_return_subroutine(Cpu65816 *cpu, uint32_t program_bank) {
    cpu->pc = program_bank | static_cast<uint16_t>(pull_word(cpu) + 1);
}

void cpu_call_long_subroutine(Cpu65816 *cpu, uint32_t target, uint32_t return_address) {
    push(cpu, return_address >> 16);
    cpu_call_subroutine(cpu, target, return_address);
}

void cpu_return_long_subroutine(Cpu65816 *cpu) {
    const uint16_t address = pull_word(cpu) + 1;
    cpu->pc = (static_cast<uint32_t>(pull(cpu)) << 16) | address;
}

void cpu_return_interrupt(Cpu65816 *cpu) {
    cpu_pull_status(cpu);
    const uint32_t address = pull_word(cpu);
    const uint32_t bank = cpu->emulation ? 0 : pull(cpu);
    cpu->pc = (bank << 16) | address;
}

void cpu_deliver_irq(Cpu65816 *cpu) {
    if (!cpu->timer_irq || (cpu->status & CPU_INTERRUPT_DISABLE)) {
        return;
    }
    if (!cpu->emulation) {
        push(cpu, cpu->pc >> 16);
    }
    cpu_push_word(cpu, cpu->pc);
    cpu_push_status(cpu);
    cpu->status = (cpu->status | CPU_INTERRUPT_DISABLE) & ~CPU_DECIMAL;
    const auto vector = cpu->emulation ? 0xFFFE : 0xFFEE;
    const uint16_t low = read_byte(cpu, vector);
    cpu->pc = low | (read_byte(cpu, vector + 1) << 8);
    ++cpu->delivered_irqs;
}

void cpu_jump_indexed(Cpu65816 *cpu, uint32_t table) {
    const auto bank = table & 0xFF0000;
    const auto offset = static_cast<uint16_t>(table + cpu->x);
    const uint16_t low = read_byte(cpu, bank | offset);
    const uint16_t high = read_byte(cpu, bank | static_cast<uint16_t>(offset + 1));
    cpu->pc = bank | (high << 8) | low;
}

void cpu_call_indexed(Cpu65816 *cpu, uint32_t table, uint16_t return_address) {
    cpu_push_word(cpu, return_address);
    cpu_jump_indexed(cpu, table);
}
