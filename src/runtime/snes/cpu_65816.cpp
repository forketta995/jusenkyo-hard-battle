#include "cpu_65816.h"

#include "cpu_65816_status.h"
#include "snes_bus.h"

using snes_bus::read_byte;
using snes_bus::write_byte;
using cpu65816::accumulator_mask;
using cpu65816::normalize_status;
using cpu65816::set_flag;
using cpu65816::set_nz;

namespace {

void write_accumulator_width(Cpu65816 *cpu, uint32_t address, uint16_t value,
                             uint32_t next_address) {
    write_byte(cpu, address, value & 0xFF);
    if (!(cpu->status & CPU_ACCUMULATOR_8)) {
        write_byte(cpu, next_address, value >> 8);
    }
}

uint16_t read_accumulator_width(Cpu65816 *cpu, uint32_t address, uint32_t next_address) {
    uint16_t value = read_byte(cpu, address);
    if (!(cpu->status & CPU_ACCUMULATOR_8)) {
        value |= read_byte(cpu, next_address) << 8;
    }
    return value;
}

uint16_t direct_address(const Cpu65816 *cpu, uint8_t offset) {
    return cpu->direct_page + offset;
}

uint16_t direct_indexed_x(const Cpu65816 *cpu, uint8_t offset) {
    if (cpu->emulation && (cpu->direct_page & 0xFF) == 0) {
        return cpu->direct_page | static_cast<uint8_t>(offset + cpu->x);
    }
    return cpu->direct_page + offset + cpu->x;
}

uint16_t shift_left(Cpu65816 *cpu, uint16_t value, bool rotate) {
    const bool carry_in = rotate && (cpu->status & CPU_CARRY);
    const auto mask = accumulator_mask(cpu);
    set_flag(cpu, CPU_CARRY, (value & ((mask + 1) >> 1)) != 0);
    const uint16_t result = ((value << 1) | carry_in) & mask;
    set_nz(cpu, result, mask);
    return result;
}

void shift_memory_left(Cpu65816 *cpu, uint32_t address, uint32_t next, bool rotate) {
    const auto value = read_accumulator_width(cpu, address, next);
    write_accumulator_width(cpu, address, shift_left(cpu, value, rotate), next);
}

uint32_t direct_long_pointer(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    const uint32_t low = read_byte(cpu, address);
    const uint32_t middle = read_byte(cpu, static_cast<uint16_t>(address + 1));
    const uint32_t high = read_byte(cpu, static_cast<uint16_t>(address + 2));
    return low | (middle << 8) | (high << 16);
}

uint16_t direct_pointer(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    const auto high_address = cpu->emulation && !(cpu->direct_page & 0xFF)
        ? direct_address(cpu, static_cast<uint8_t>(offset + 1))
        : static_cast<uint16_t>(address + 1);
    const uint16_t low = read_byte(cpu, address);
    return low | (read_byte(cpu, high_address) << 8);
}

uint32_t absolute_address(const Cpu65816 *cpu, uint16_t offset) {
    return (static_cast<uint32_t>(cpu->data_bank) << 16) | offset;
}

uint16_t read_absolute_accumulator(Cpu65816 *cpu, uint16_t address) {
    return read_accumulator_width(cpu, absolute_address(cpu, address), absolute_address(cpu, address + 1));
}

uint16_t read_index_width(Cpu65816 *cpu, uint32_t address, uint32_t next_address) {
    uint16_t value = read_byte(cpu, address);
    if (!(cpu->status & CPU_INDEX_8)) {
        value |= read_byte(cpu, next_address) << 8;
    }
    return value;
}

} // namespace

void cpu_reset(Cpu65816 *cpu) {
    *cpu = {};
    cpu->status = CPU_INTERRUPT_DISABLE | CPU_INDEX_8 | CPU_ACCUMULATOR_8;
    cpu->emulation = true;
    cpu->stack = 0x01FF;
    snes_apu_reset(cpu->apu);
}

void cpu_clear_carry(Cpu65816 *cpu) {
    cpu->status &= ~CPU_CARRY;
}

void cpu_set_carry(Cpu65816 *cpu) {
    cpu->status |= CPU_CARRY;
}

void cpu_set_interrupt_disable(Cpu65816 *cpu) {
    cpu->status |= CPU_INTERRUPT_DISABLE;
}

void cpu_clear_interrupt_disable(Cpu65816 *cpu) {
    cpu->status &= ~CPU_INTERRUPT_DISABLE;
}

void cpu_exchange_carry_emulation(Cpu65816 *cpu) {
    const bool old_carry = cpu->status & CPU_CARRY;
    set_flag(cpu, CPU_CARRY, cpu->emulation);
    cpu->emulation = old_carry;
    normalize_status(cpu);
}

void cpu_clear_status(Cpu65816 *cpu, uint8_t mask) {
    cpu->status &= ~mask;
    normalize_status(cpu);
}

void cpu_set_status(Cpu65816 *cpu, uint8_t mask) {
    cpu->status |= mask;
    normalize_status(cpu);
}

void cpu_load_accumulator(Cpu65816 *cpu, uint16_t value) {
    const auto mask = accumulator_mask(cpu);
    cpu->accumulator = (cpu->accumulator & ~mask) | (value & mask);
    set_nz(cpu, value, mask);
}

void cpu_load_accumulator_long(Cpu65816 *cpu, uint32_t address) {
    cpu_load_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_load_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_load_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_load_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    cpu_load_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_load_accumulator_absolute(Cpu65816 *cpu, uint16_t address) {
    cpu_load_accumulator(cpu, read_accumulator_width(
        cpu, absolute_address(cpu, address), absolute_address(cpu, address + 1)));
}

void cpu_load_accumulator_indexed_x(Cpu65816 *cpu, uint16_t address) {
    const auto bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    cpu_load_accumulator_long(cpu, (bank + address + cpu->x) & 0xFFFFFF);
}

void cpu_load_accumulator_indexed_y(Cpu65816 *cpu, uint16_t address) {
    cpu_load_accumulator_long(cpu, (absolute_address(cpu, address) + cpu->y) & 0xFFFFFF);
}

void cpu_load_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address) {
    cpu_load_accumulator_long(cpu, (address + cpu->x) & 0xFFFFFF);
}

void cpu_load_accumulator_indirect_long_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    cpu_load_accumulator_long(cpu, (direct_long_pointer(cpu, offset) + cpu->y) & 0xFFFFFF);
}

void cpu_load_accumulator_indirect_long(Cpu65816 *cpu, uint8_t offset) {
    cpu_load_accumulator_long(cpu, direct_long_pointer(cpu, offset));
}

void cpu_push_indirect_word(Cpu65816 *cpu, uint8_t offset) {
    cpu_push_word(cpu, direct_pointer(cpu, offset));
}

void cpu_load_accumulator_indirect(Cpu65816 *cpu, uint8_t offset) {
    cpu_load_accumulator_absolute(cpu, direct_pointer(cpu, offset));
}

void cpu_load_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    cpu_load_accumulator_long(cpu, (bank + direct_pointer(cpu, offset) + cpu->y) & 0xFFFFFF);
}

void cpu_load_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    cpu_load_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

namespace {

void accumulator_arithmetic(Cpu65816 *cpu, uint16_t value, bool subtract) {
    const uint32_t mask = accumulator_mask(cpu);
    const uint32_t a = cpu->accumulator & mask;
    const uint32_t b = (subtract ? ~value : value) & mask;
    const uint32_t sign = (mask + 1) >> 1;
    unsigned carry = (cpu->status & CPU_CARRY) != 0;
    uint32_t sum = a + b + carry;
    uint32_t overflow_sum = sum;
    if (cpu->status & CPU_DECIMAL) {
        sum = 0;
        for (unsigned shift = 0; (mask >> shift) != 0; shift += 4) {
            int digit = ((a >> shift) & 15) + ((b >> shift) & 15) + carry;
            overflow_sum = sum | (static_cast<unsigned>(digit) << shift);
            if (subtract && digit <= 15) {
                digit -= 6;
            } else if (!subtract && digit > 9) {
                digit += 6;
            }
            carry = digit > 15;
            sum |= (static_cast<unsigned>(digit) & 15) << shift;
        }
    } else {
        carry = sum > mask;
    }
    set_flag(cpu, CPU_CARRY, carry);
    set_flag(cpu, CPU_OVERFLOW, (~(a ^ b) & (a ^ overflow_sum) & sign) != 0);
    cpu_load_accumulator(cpu, sum);
}

} // namespace

void cpu_add_accumulator(Cpu65816 *cpu, uint16_t value) {
    accumulator_arithmetic(cpu, value, false);
}

void cpu_subtract_accumulator(Cpu65816 *cpu, uint16_t value) {
    accumulator_arithmetic(cpu, value, true);
}

void cpu_add_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_add_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_add_accumulator_absolute(Cpu65816 *cpu, uint16_t address) {
    cpu_add_accumulator(cpu, read_accumulator_width(
        cpu, absolute_address(cpu, address), absolute_address(cpu, address + 1)));
}

void cpu_add_accumulator_long(Cpu65816 *cpu, uint32_t address) {
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_subtract_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_subtract_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_subtract_accumulator_absolute(Cpu65816 *cpu, uint16_t address) {
    cpu_subtract_accumulator(cpu, read_accumulator_width(
        cpu, absolute_address(cpu, address), absolute_address(cpu, address + 1)));
}

void cpu_add_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_add_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_add_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_add_accumulator_indirect(Cpu65816 *cpu, uint8_t offset) {
    cpu_add_accumulator_absolute(cpu, direct_pointer(cpu, offset));
}

void cpu_add_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto address = (absolute_address(cpu, direct_pointer(cpu, offset)) + cpu->y) & 0xFFFFFF;
    cpu_add_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_subtract_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    cpu_subtract_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_and_accumulator(Cpu65816 *cpu, uint16_t value) {
    cpu_load_accumulator(cpu, cpu->accumulator & value);
}

void cpu_xor_accumulator(Cpu65816 *cpu, uint16_t value) {
    cpu_load_accumulator(cpu, cpu->accumulator ^ value);
}

void cpu_test_accumulator(Cpu65816 *cpu, uint16_t value) {
    set_flag(cpu, CPU_ZERO, (cpu->accumulator & value & accumulator_mask(cpu)) == 0);
}

void cpu_increment_accumulator(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, cpu->accumulator + 1);
}

void cpu_decrement_accumulator(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, cpu->accumulator - 1);
}

void cpu_shift_accumulator_left(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, shift_left(cpu, cpu->accumulator, false));
}

void cpu_rotate_accumulator_left(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, shift_left(cpu, cpu->accumulator, true));
}

void cpu_rotate_accumulator_right(Cpu65816 *cpu) {
    const auto mask = accumulator_mask(cpu);
    const auto carry_in = (cpu->status & CPU_CARRY) ? ((mask + 1) >> 1) : 0;
    set_flag(cpu, CPU_CARRY, cpu->accumulator & 1);
    cpu_load_accumulator(cpu, ((cpu->accumulator & mask) >> 1) | carry_in);
}

void cpu_shift_direct_left(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    shift_memory_left(cpu, address, static_cast<uint16_t>(address + 1), false);
}

void cpu_shift_indexed_x_left(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    shift_memory_left(cpu, address, (address + 1) & 0xFFFFFF, false);
}

void cpu_rotate_direct_left(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    shift_memory_left(cpu, address, static_cast<uint16_t>(address + 1), true);
}

void cpu_rotate_direct_indexed_x_left(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    shift_memory_left(cpu, address, static_cast<uint16_t>(address + 1), true);
}

void cpu_shift_accumulator_right(Cpu65816 *cpu) {
    set_flag(cpu, CPU_CARRY, cpu->accumulator & 1);
    cpu_load_accumulator(cpu, (cpu->accumulator & accumulator_mask(cpu)) >> 1);
}

static void shift_memory_right(Cpu65816 *cpu, uint32_t address, uint32_t next) {
    const auto value = read_accumulator_width(cpu, address, next);
    set_flag(cpu, CPU_CARRY, value & 1);
    set_nz(cpu, value >> 1, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value >> 1, next);
}

void cpu_shift_direct_right(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    shift_memory_right(cpu, address, static_cast<uint16_t>(address + 1));
}

void cpu_shift_indexed_x_right(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    shift_memory_right(cpu, address, (address + 1) & 0xFFFFFF);
}

void cpu_compare_accumulator(Cpu65816 *cpu, uint16_t value) {
    const auto mask = accumulator_mask(cpu);
    const auto accumulator = cpu->accumulator & mask;
    const auto operand = value & mask;
    set_flag(cpu, CPU_CARRY, accumulator >= operand);
    set_nz(cpu, accumulator - operand, mask);
}

void cpu_compare_accumulator_long(Cpu65816 *cpu, uint32_t address) {
    cpu_compare_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_compare_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address) {
    cpu_compare_accumulator_long(cpu, (address + cpu->x) & 0xFFFFFF);
}

void cpu_compare_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    cpu_compare_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_compare_accumulator_absolute(Cpu65816 *cpu, uint16_t offset) {
    cpu_compare_accumulator(cpu, read_accumulator_width(
        cpu, absolute_address(cpu, offset), absolute_address(cpu, offset + 1)));
}

void cpu_compare_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_compare_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_compare_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    cpu_compare_accumulator(cpu, read_accumulator_width(
        cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_load_x(Cpu65816 *cpu, uint16_t value) {
    const uint16_t mask = (cpu->status & CPU_INDEX_8) ? 0xFF : 0xFFFF;
    cpu->x = value & mask;
    set_nz(cpu, cpu->x, mask);
}

void cpu_load_y(Cpu65816 *cpu, uint16_t value) {
    const uint16_t mask = (cpu->status & CPU_INDEX_8) ? 0xFF : 0xFFFF;
    cpu->y = value & mask;
    set_nz(cpu, cpu->y, mask);
}

void cpu_load_x_absolute(Cpu65816 *cpu, uint16_t address) {
    cpu_load_x(cpu, read_index_width(cpu, absolute_address(cpu, address),
                                    absolute_address(cpu, address + 1)));
}

void cpu_load_x_indexed_y(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_load_x(cpu, read_index_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_load_x_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_load_x(cpu, read_index_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_load_y_absolute(Cpu65816 *cpu, uint16_t address) {
    cpu_load_y(cpu, read_index_width(cpu, absolute_address(cpu, address),
                                    absolute_address(cpu, address + 1)));
}

void cpu_load_y_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_load_y(cpu, read_index_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_load_y_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_load_y(cpu, read_index_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_increment_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, cpu->x + 1);
}

void cpu_increment_y(Cpu65816 *cpu) {
    cpu_load_y(cpu, cpu->y + 1);
}

void cpu_decrement_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, cpu->x - 1);
}

void cpu_decrement_y(Cpu65816 *cpu) {
    cpu_load_y(cpu, cpu->y - 1);
}

void cpu_compare_x(Cpu65816 *cpu, uint16_t value) {
    const uint16_t mask = (cpu->status & CPU_INDEX_8) ? 0xFF : 0xFFFF;
    const auto operand = value & mask;
    set_flag(cpu, CPU_CARRY, cpu->x >= operand);
    set_nz(cpu, cpu->x - operand, mask);
}

void cpu_compare_x_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_compare_x(cpu, read_index_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_compare_y(Cpu65816 *cpu, uint16_t value) {
    const uint16_t mask = (cpu->status & CPU_INDEX_8) ? 0xFF : 0xFFFF;
    const auto operand = value & mask;
    set_flag(cpu, CPU_CARRY, cpu->y >= operand);
    set_nz(cpu, cpu->y - operand, mask);
}

void cpu_compare_y_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_compare_y(cpu, read_index_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_transfer_x_to_stack(Cpu65816 *cpu) {
    cpu->stack = cpu->x;
    normalize_status(cpu);
}

void cpu_transfer_x_to_y(Cpu65816 *cpu) {
    cpu_load_y(cpu, cpu->x);
}

void cpu_transfer_y_to_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, cpu->y);
}

void cpu_transfer_stack_to_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, cpu->stack);
}

void cpu_transfer_x_to_accumulator(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, cpu->x);
}

void cpu_transfer_y_to_accumulator(Cpu65816 *cpu) {
    cpu_load_accumulator(cpu, cpu->y);
}

void cpu_transfer_accumulator_to_x(Cpu65816 *cpu) {
    cpu_load_x(cpu, cpu->accumulator);
}

void cpu_transfer_accumulator_to_y(Cpu65816 *cpu) {
    cpu_load_y(cpu, cpu->accumulator);
}

void cpu_transfer_stack_to_accumulator(Cpu65816 *cpu) {
    cpu->accumulator = cpu->stack;
    set_nz(cpu, cpu->accumulator, 0xFFFF);
}

void cpu_transfer_accumulator_to_stack(Cpu65816 *cpu) {
    cpu->stack = cpu->accumulator;
    normalize_status(cpu);
}

void cpu_transfer_accumulator_to_direct_page(Cpu65816 *cpu) {
    cpu->direct_page = cpu->accumulator;
    set_nz(cpu, cpu->direct_page, 0xFFFF);
}

void cpu_exchange_accumulator_bytes(Cpu65816 *cpu) {
    const auto value = cpu->accumulator;
    cpu->accumulator = (value << 8) | (value >> 8);
    set_nz(cpu, cpu->accumulator, 0xFF);
}

void cpu_store_accumulator(Cpu65816 *cpu, uint16_t address) {
    const uint32_t bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    write_accumulator_width(cpu, bank | address, cpu->accumulator,
                            bank | static_cast<uint16_t>(address + 1));
}

void cpu_store_accumulator_indexed_y(Cpu65816 *cpu, uint16_t address) {
    const auto bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    cpu_store_accumulator_long(cpu, (bank + address + cpu->y) & 0xFFFFFF);
}

void cpu_store_accumulator_indexed_x(Cpu65816 *cpu, uint16_t address) {
    cpu_store_accumulator_long(cpu, (absolute_address(cpu, address) + cpu->x) & 0xFFFFFF);
}

void cpu_store_x(Cpu65816 *cpu, uint16_t address) {
    write_byte(cpu, absolute_address(cpu, address), cpu->x & 0xFF);
    if (!(cpu->status & CPU_INDEX_8)) {
        write_byte(cpu, absolute_address(cpu, address + 1), cpu->x >> 8);
    }
}

void cpu_store_x_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    write_byte(cpu, address, cpu->x & 0xFF);
    if (!(cpu->status & CPU_INDEX_8)) {
        write_byte(cpu, static_cast<uint16_t>(address + 1), cpu->x >> 8);
    }
}

void cpu_store_accumulator_long(Cpu65816 *cpu, uint32_t address) {
    write_accumulator_width(cpu, address, cpu->accumulator, (address + 1) & 0xFFFFFF);
}

void cpu_store_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address) {
    cpu_store_accumulator_long(cpu, (address + cpu->x) & 0xFFFFFF);
}

void cpu_store_zero(Cpu65816 *cpu, uint16_t address) {
    const uint32_t bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    write_accumulator_width(cpu, bank | address, 0,
                            bank | static_cast<uint16_t>(address + 1));
}

void cpu_store_zero_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    write_accumulator_width(cpu, address, 0, (address + 1) & 0xFFFFFF);
}

void cpu_test_and_set_absolute(Cpu65816 *cpu, uint16_t offset) {
    const auto address = absolute_address(cpu, offset);
    const auto next_address = absolute_address(cpu, offset + 1);
    const auto value = read_accumulator_width(cpu, address, next_address);
    set_flag(cpu, CPU_ZERO, (value & cpu->accumulator & accumulator_mask(cpu)) == 0);
    write_accumulator_width(cpu, address, value | cpu->accumulator, next_address);
}

void cpu_store_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    write_accumulator_width(cpu, address, cpu->accumulator, static_cast<uint16_t>(address + 1));
}

void cpu_store_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    write_accumulator_width(cpu, address, cpu->accumulator, static_cast<uint16_t>(address + 1));
}

void cpu_store_accumulator_indirect_long(Cpu65816 *cpu, uint8_t offset) {
    cpu_store_accumulator_long(cpu, direct_long_pointer(cpu, offset));
}

void cpu_store_accumulator_indirect_long_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    cpu_store_accumulator_long(cpu, (direct_long_pointer(cpu, offset) + cpu->y) & 0xFFFFFF);
}

void cpu_store_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    cpu_store_accumulator_long(cpu, (bank + direct_pointer(cpu, offset) + cpu->y) & 0xFFFFFF);
}

void cpu_store_y_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    write_byte(cpu, address, cpu->y & 0xFF);
    if (!(cpu->status & CPU_INDEX_8)) {
        write_byte(cpu, static_cast<uint16_t>(address + 1), cpu->y >> 8);
    }
}

void cpu_store_y_absolute(Cpu65816 *cpu, uint16_t offset) {
    write_byte(cpu, absolute_address(cpu, offset), cpu->y & 0xFF);
    if (!(cpu->status & CPU_INDEX_8)) {
        write_byte(cpu, absolute_address(cpu, offset + 1), cpu->y >> 8);
    }
}

void cpu_store_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    write_accumulator_width(cpu, address, cpu->accumulator, static_cast<uint16_t>(address + 1));
}

void cpu_store_zero_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    write_accumulator_width(cpu, address, 0, static_cast<uint16_t>(address + 1));
}

void cpu_store_zero_direct_indexed_x(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_indexed_x(cpu, offset);
    write_accumulator_width(cpu, address, 0, static_cast<uint16_t>(address + 1));
}

void cpu_increment_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    const auto next_address = static_cast<uint16_t>(address + 1);
    const uint16_t value = read_accumulator_width(cpu, address, next_address) + 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next_address);
}

void cpu_or_accumulator_absolute(Cpu65816 *cpu, uint16_t offset) {
    cpu_or_accumulator(cpu, read_absolute_accumulator(cpu, offset));
}

void cpu_or_accumulator_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    cpu_or_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_or_accumulator(Cpu65816 *cpu, uint16_t value) {
    cpu_load_accumulator(cpu, cpu->accumulator | value);
}

void cpu_or_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset) {
    const uint16_t address = cpu->stack + offset;
    cpu_or_accumulator(cpu, read_accumulator_width(cpu, address, static_cast<uint16_t>(address + 1)));
}

void cpu_and_accumulator_absolute(Cpu65816 *cpu, uint16_t offset) {
    cpu_and_accumulator(cpu, read_absolute_accumulator(cpu, offset));
}

void cpu_xor_accumulator_absolute(Cpu65816 *cpu, uint16_t offset) {
    cpu_xor_accumulator(cpu, read_absolute_accumulator(cpu, offset));
}

void cpu_compare_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_compare_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_compare_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_compare_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_compare_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto bank = static_cast<uint32_t>(cpu->data_bank) << 16;
    const auto address = (bank + direct_pointer(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_compare_accumulator_long(cpu, address);
}

void cpu_decrement_absolute(Cpu65816 *cpu, uint16_t offset) {
    const auto address = absolute_address(cpu, offset);
    const auto next_address = absolute_address(cpu, offset + 1);
    const uint16_t value = read_accumulator_width(cpu, address, next_address) - 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next_address);
}

void cpu_or_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_or_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_or_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_or_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_subtract_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->y) & 0xFFFFFF;
    cpu_subtract_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_subtract_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_subtract_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_or_accumulator_indirect(Cpu65816 *cpu, uint8_t offset) {
    cpu_or_accumulator_absolute(cpu, direct_pointer(cpu, offset));
}

void cpu_or_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto address = (absolute_address(cpu, direct_pointer(cpu, offset)) + cpu->y) & 0xFFFFFF;
    cpu_or_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_and_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset) {
    const auto address = (absolute_address(cpu, direct_pointer(cpu, offset)) + cpu->y) & 0xFFFFFF;
    cpu_and_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_and_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t base) {
    const auto address = (base + cpu->x) & 0xFFFFFF;
    cpu_and_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_and_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    cpu_and_accumulator(cpu, read_accumulator_width(cpu, address, (address + 1) & 0xFFFFFF));
}

void cpu_store_accumulator_indirect(Cpu65816 *cpu, uint8_t offset) {
    cpu_store_accumulator(cpu, direct_pointer(cpu, offset));
}

void cpu_subtract_accumulator_indirect(Cpu65816 *cpu, uint8_t offset) {
    cpu_subtract_accumulator_absolute(cpu, direct_pointer(cpu, offset));
}

void cpu_increment_absolute(Cpu65816 *cpu, uint16_t offset) {
    const auto address = absolute_address(cpu, offset);
    const auto next_address = absolute_address(cpu, offset + 1);
    const uint16_t value = read_accumulator_width(cpu, address, next_address) + 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next_address);
}

void cpu_increment_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    const auto next = (address + 1) & 0xFFFFFF;
    const uint16_t value = read_accumulator_width(cpu, address, next) + 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next);
}

void cpu_decrement_indexed_x(Cpu65816 *cpu, uint16_t offset) {
    const auto address = (absolute_address(cpu, offset) + cpu->x) & 0xFFFFFF;
    const auto next = (address + 1) & 0xFFFFFF;
    const uint16_t value = read_accumulator_width(cpu, address, next) - 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next);
}

void cpu_decrement_direct(Cpu65816 *cpu, uint8_t offset) {
    const auto address = direct_address(cpu, offset);
    const auto next_address = static_cast<uint16_t>(address + 1);
    const uint16_t value = read_accumulator_width(cpu, address, next_address) - 1;
    set_nz(cpu, value, accumulator_mask(cpu));
    write_accumulator_width(cpu, address, value, next_address);
}
