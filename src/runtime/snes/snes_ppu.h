#pragma once

#include <stdint.h>
#include <stdbool.h>

enum { SNES_VISIBLE_LINES = 224 };

typedef struct {
    uint8_t registers[0x34];
    uint8_t vram[0x10000];
    uint8_t oam[0x220];
    uint16_t cgram[0x100];
    uint16_t line_cgram[SNES_VISIBLE_LINES][0x100];
    bool line_cgram_active;
    bool hdma_writing;
    uint8_t visible_display;
    uint16_t vram_address;
    uint16_t vram_latch;
    uint16_t oam_address;
    uint8_t oam_latch;
    uint8_t cgram_address;
    uint8_t cgram_latch;
    uint8_t cgram_high_byte;
    uint16_t scroll[4][2];
    uint8_t scroll_latch;
    uint8_t horizontal_scroll_latch;
    uint8_t mode7_latch;
    uint16_t mode7_a;
    uint16_t scanline;
    uint16_t latched_scanline;
    uint16_t dot;
    uint16_t latched_dot;
    bool read_scanline_high;
    bool read_dot_high;
} SnesPpu;

#ifdef __cplusplus
void snes_ppu_begin_vblank(SnesPpu &ppu);
bool snes_ppu_write(SnesPpu &ppu, uint16_t address, uint8_t value);
bool snes_ppu_read(SnesPpu &ppu, uint16_t address, uint8_t &value);
#endif
