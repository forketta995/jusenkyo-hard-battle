#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "snes_apu.h"
#include "snes_ppu.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CPU_CARRY = 0x01,
    CPU_ZERO = 0x02,
    CPU_INTERRUPT_DISABLE = 0x04,
    CPU_DECIMAL = 0x08,
    CPU_INDEX_8 = 0x10,
    CPU_ACCUMULATOR_8 = 0x20,
    CPU_OVERFLOW = 0x40,
    CPU_NEGATIVE = 0x80,
};

enum {
    JOYPAD_B = 0x8000,
    JOYPAD_START = 0x1000,
    JOYPAD_UP = 0x0800,
    JOYPAD_DOWN = 0x0400,
    JOYPAD_LEFT = 0x0200,
    JOYPAD_RIGHT = 0x0100,
    JOYPAD_A = 0x0080,
};

typedef struct {
    uint32_t pc;
    uint32_t last_block;
    uint16_t accumulator;
    uint16_t x;
    uint16_t y;
    uint16_t stack;
    uint16_t direct_page;
    uint8_t data_bank;
    uint8_t status;
    bool emulation;
    const uint8_t *rom;
    uint32_t rom_size;
    uint8_t work_ram[0x20000];
    SnesPpu ppu;
    SnesApu apu;
    uint8_t cpu_io[0x0E];
    uint16_t division_quotient;
    uint16_t multiply_or_divide_remainder;
    uint8_t dma[8][16];
    uint8_t hdma_active;
    uint8_t hdma_transfer;
    uint32_t dma_bytes;
    bool timer_irq;
    uint32_t delivered_irqs;
    uint32_t frame_count;
    uint16_t joypad_input[4];
    uint16_t joypad_latch[4];
    bool joypad_busy;
    bool running_frames;
    bool apply_preview_patches;
    bool unmapped_access;
    uint32_t unmapped_address;
} Cpu65816;

void cpu_reset(Cpu65816 *cpu);
void cpu_clear_carry(Cpu65816 *cpu);
void cpu_set_carry(Cpu65816 *cpu);
void cpu_set_interrupt_disable(Cpu65816 *cpu);
void cpu_clear_interrupt_disable(Cpu65816 *cpu);
void cpu_exchange_carry_emulation(Cpu65816 *cpu);
void cpu_clear_status(Cpu65816 *cpu, uint8_t mask);
void cpu_set_status(Cpu65816 *cpu, uint8_t mask);
void cpu_load_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_load_accumulator_direct(Cpu65816 *cpu, uint8_t address);
void cpu_load_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_load_accumulator_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_load_accumulator_long(Cpu65816 *cpu, uint32_t address);
void cpu_load_accumulator_indexed_x(Cpu65816 *cpu, uint16_t address);
void cpu_load_accumulator_indexed_y(Cpu65816 *cpu, uint16_t address);
void cpu_add_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_subtract_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_subtract_accumulator_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_subtract_accumulator_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_add_accumulator_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_add_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset);
void cpu_add_accumulator_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_add_accumulator_long(Cpu65816 *cpu, uint32_t address);
void cpu_add_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_add_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_add_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset);
void cpu_add_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_add_accumulator_indirect(Cpu65816 *cpu, uint8_t offset);
void cpu_subtract_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_and_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_xor_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_test_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_increment_accumulator(Cpu65816 *cpu);
void cpu_decrement_accumulator(Cpu65816 *cpu);
void cpu_shift_accumulator_left(Cpu65816 *cpu);
void cpu_shift_accumulator_right(Cpu65816 *cpu);
void cpu_shift_direct_right(Cpu65816 *cpu, uint8_t offset);
void cpu_shift_indexed_x_right(Cpu65816 *cpu, uint16_t offset);
void cpu_shift_direct_left(Cpu65816 *cpu, uint8_t offset);
void cpu_shift_indexed_x_left(Cpu65816 *cpu, uint16_t offset);
void cpu_rotate_direct_left(Cpu65816 *cpu, uint8_t offset);
void cpu_rotate_direct_indexed_x_left(Cpu65816 *cpu, uint8_t offset);
void cpu_rotate_accumulator_left(Cpu65816 *cpu);
void cpu_rotate_accumulator_right(Cpu65816 *cpu);
void cpu_compare_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_compare_accumulator_long(Cpu65816 *cpu, uint32_t address);
void cpu_compare_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address);
void cpu_compare_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_compare_accumulator_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_compare_accumulator_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_compare_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset);
void cpu_load_x(Cpu65816 *cpu, uint16_t value);
void cpu_load_y(Cpu65816 *cpu, uint16_t value);
void cpu_load_x_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_load_x_indexed_y(Cpu65816 *cpu, uint16_t address);
void cpu_load_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address);
void cpu_load_accumulator_indirect_long_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_load_accumulator_indirect_long(Cpu65816 *cpu, uint8_t offset);
void cpu_load_accumulator_indirect(Cpu65816 *cpu, uint8_t offset);
void cpu_load_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_load_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset);
void cpu_load_x_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_load_y_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_load_y_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_load_y_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_increment_x(Cpu65816 *cpu);
void cpu_increment_y(Cpu65816 *cpu);
void cpu_decrement_x(Cpu65816 *cpu);
void cpu_decrement_y(Cpu65816 *cpu);
void cpu_compare_x(Cpu65816 *cpu, uint16_t value);
void cpu_compare_x_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_compare_y(Cpu65816 *cpu, uint16_t value);
void cpu_compare_y_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_transfer_x_to_stack(Cpu65816 *cpu);
void cpu_transfer_x_to_y(Cpu65816 *cpu);
void cpu_transfer_y_to_x(Cpu65816 *cpu);
void cpu_transfer_stack_to_x(Cpu65816 *cpu);
void cpu_transfer_x_to_accumulator(Cpu65816 *cpu);
void cpu_transfer_y_to_accumulator(Cpu65816 *cpu);
void cpu_transfer_accumulator_to_x(Cpu65816 *cpu);
void cpu_transfer_accumulator_to_y(Cpu65816 *cpu);
void cpu_transfer_stack_to_accumulator(Cpu65816 *cpu);
void cpu_transfer_accumulator_to_stack(Cpu65816 *cpu);
void cpu_transfer_accumulator_to_direct_page(Cpu65816 *cpu);
void cpu_exchange_accumulator_bytes(Cpu65816 *cpu);
void cpu_push_accumulator(Cpu65816 *cpu);
void cpu_pull_accumulator(Cpu65816 *cpu);
void cpu_push_data_bank(Cpu65816 *cpu);
void cpu_push_program_bank(Cpu65816 *cpu, uint8_t bank);
void cpu_push_word(Cpu65816 *cpu, uint16_t value);
void cpu_push_indirect_word(Cpu65816 *cpu, uint8_t offset);
void cpu_push_x(Cpu65816 *cpu);
void cpu_push_y(Cpu65816 *cpu);
void cpu_pull_x(Cpu65816 *cpu);
void cpu_pull_y(Cpu65816 *cpu);
void cpu_push_direct_page(Cpu65816 *cpu);
void cpu_pull_direct_page(Cpu65816 *cpu);
void cpu_push_status(Cpu65816 *cpu);
void cpu_pull_status(Cpu65816 *cpu);
void cpu_pull_data_bank(Cpu65816 *cpu);
void cpu_move_block_forward(Cpu65816 *cpu, uint16_t banks);
void cpu_call_subroutine(Cpu65816 *cpu, uint32_t target, uint16_t return_address);
void cpu_return_subroutine(Cpu65816 *cpu, uint32_t program_bank);
void cpu_call_long_subroutine(Cpu65816 *cpu, uint32_t target, uint32_t return_address);
void cpu_return_long_subroutine(Cpu65816 *cpu);
void cpu_return_interrupt(Cpu65816 *cpu);
void cpu_advance_raster(Cpu65816 *cpu, unsigned dots);
void cpu_deliver_irq(Cpu65816 *cpu);
void cpu_jump_indexed(Cpu65816 *cpu, uint32_t table);
void cpu_call_indexed(Cpu65816 *cpu, uint32_t table, uint16_t return_address);
void cpu_store_accumulator(Cpu65816 *cpu, uint16_t address);
void cpu_store_accumulator_indexed_y(Cpu65816 *cpu, uint16_t address);
void cpu_store_accumulator_indexed_x(Cpu65816 *cpu, uint16_t address);
void cpu_store_x(Cpu65816 *cpu, uint16_t address);
void cpu_store_x_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_store_accumulator_long(Cpu65816 *cpu, uint32_t address);
void cpu_store_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t address);
void cpu_store_zero(Cpu65816 *cpu, uint16_t address);
void cpu_store_zero_indexed_x(Cpu65816 *cpu, uint16_t address);
void cpu_test_and_set_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_store_accumulator_direct(Cpu65816 *cpu, uint8_t address);
void cpu_store_accumulator_direct_indexed_x(Cpu65816 *cpu, uint8_t offset);
void cpu_store_accumulator_indirect_long(Cpu65816 *cpu, uint8_t offset);
void cpu_store_accumulator_indirect_long_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_store_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_store_y_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_store_y_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_store_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_store_zero_direct(Cpu65816 *cpu, uint8_t address);
void cpu_store_zero_direct_indexed_x(Cpu65816 *cpu, uint8_t offset);
void cpu_increment_direct(Cpu65816 *cpu, uint8_t address);
void cpu_increment_absolute(Cpu65816 *cpu, uint16_t address);
void cpu_increment_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_decrement_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_or_accumulator_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_or_accumulator_direct(Cpu65816 *cpu, uint8_t offset);
void cpu_or_accumulator(Cpu65816 *cpu, uint16_t value);
void cpu_or_accumulator_stack_relative(Cpu65816 *cpu, uint8_t offset);
void cpu_and_accumulator_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_and_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_and_accumulator_long_indexed_x(Cpu65816 *cpu, uint32_t base);
void cpu_xor_accumulator_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_compare_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_compare_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset);
void cpu_compare_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_or_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset);
void cpu_or_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_and_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_or_accumulator_indirect(Cpu65816 *cpu, uint8_t offset);
void cpu_or_accumulator_indirect_indexed_y(Cpu65816 *cpu, uint8_t offset);
void cpu_subtract_accumulator_indexed_y(Cpu65816 *cpu, uint16_t offset);
void cpu_subtract_accumulator_indexed_x(Cpu65816 *cpu, uint16_t offset);
void cpu_store_accumulator_indirect(Cpu65816 *cpu, uint8_t offset);
void cpu_subtract_accumulator_indirect(Cpu65816 *cpu, uint8_t offset);
void cpu_decrement_absolute(Cpu65816 *cpu, uint16_t offset);
void cpu_decrement_direct(Cpu65816 *cpu, uint8_t address);

#ifdef __cplusplus
}
#endif
