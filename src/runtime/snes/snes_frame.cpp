#include "snes_frame.h"
#include "snes_objects.h"
#include "snes_tiles.h"

namespace {

unsigned tilemap_address(const SnesPpu &ppu, unsigned layer, unsigned x, unsigned y) {
    const auto control = ppu.registers[0x07 + layer];
    const bool wide = control & 1;
    const bool tall = control & 2;
    x &= wide ? 63 : 31;
    y &= tall ? 63 : 31;
    const unsigned screen = (x >> 5) + (y >> 5) * (wide ? 2 : 1);
    return ((control & 0xFC) << 8) + screen * 1024 + (y & 31) * 32 + (x & 31);
}

struct BackgroundPixel {
    uint8_t palette_index;
    bool high_priority;
};

BackgroundPixel background_pixel(const SnesPpu &ppu, unsigned layer, unsigned x, unsigned y) {
    const bool large = (ppu.registers[0x05] & (0x10 << layer)) != 0;
    const unsigned shift = large ? 4 : 3;
    x += ppu.scroll[layer][0] & 0x3FF;
    y += ppu.scroll[layer][1] & 0x3FF;
    const auto tile = snes_tiles::vram_word(ppu, tilemap_address(ppu, layer, x >> shift, y >> shift));
    if (tile & 0x4000) x ^= (1 << shift) - 1;
    if (tile & 0x8000) y ^= (1 << shift) - 1;

    const auto bases = ppu.registers[0x0B + layer / 2];
    const unsigned base = ((bases >> ((layer % 2) * 4)) & 0xF) << 12;
    unsigned character = tile & 0x3FF;
    if (large) {
        character = (character + ((x >> 3) & 1) + (((y >> 3) & 1) * 16)) & 0x3FF;
    }
    const unsigned depth = layer == 2 ? 2 : 4;
    const unsigned row = base + character * depth * 4 + (y & 7);
    const unsigned bit = 7 - (x & 7);
    const auto low_planes = snes_tiles::vram_word(ppu, row);
    const auto high_planes = depth == 4 ? snes_tiles::vram_word(ppu, row + 8) : 0;
    unsigned color = snes_tiles::planar_pixel(low_planes, high_planes, bit);
    if (color != 0) {
        color += ((tile >> 10) & 7) << depth;
    }
    return {static_cast<uint8_t>(color), (tile & 0x2000) != 0};
}

uint32_t rgb32(uint16_t color, unsigned brightness) {
    const auto channel = [brightness](unsigned value) {
        return (value * brightness * 255) / (31 * 15);
    };
    return (channel(color & 31) << 16) | (channel((color >> 5) & 31) << 8) |
           channel((color >> 10) & 31);
}

} // namespace

bool snes_render_mode1(const SnesPpu &ppu, SnesFrame &frame) {
    if ((ppu.registers[0x05] & 7) != 1) {
        return false;
    }
    const auto display = ppu.line_cgram_active ? ppu.visible_display : ppu.registers[0];
    const auto brightness = (display & 0x80) ? 0 : display & 15;
    const auto enabled = ppu.registers[0x2C];
    const bool bg3_top = ppu.registers[0x05] & 8;
    constexpr unsigned background_ranks[2][3][2] = {
        {{6, 9}, {5, 8}, {1, 3}},
        {{5, 8}, {4, 7}, {1, 10}},
    };
    constexpr unsigned object_ranks[2][4] = {{2, 4, 7, 10}, {2, 3, 6, 9}};
    for (int y = 0; y < SnesFrame::height; ++y) {
        const auto *palette = ppu.line_cgram_active ? ppu.line_cgram[y] : ppu.cgram;
        SnesObjectLine objects{};
        if ((enabled & 0x10) && brightness != 0) {
            snes_render_object_line(ppu, y, objects);
        }
        for (int x = 0; x < SnesFrame::width; ++x) {
            const auto object = objects[x];
            unsigned highest_priority = object.palette_index ? object_ranks[bg3_top][object.priority] : 0;
            unsigned color_index = object.palette_index;
            for (unsigned layer = 0; layer < 3; ++layer) {
                if ((enabled & (1 << layer)) == 0) {
                    continue;
                }
                const auto pixel = background_pixel(ppu, layer, x, y);
                const auto rank = background_ranks[bg3_top][layer][pixel.high_priority];
                if (pixel.palette_index != 0 && rank > highest_priority) {
                    color_index = pixel.palette_index;
                    highest_priority = rank;
                }
            }
            frame.pixels[y * SnesFrame::width + x] = rgb32(palette[color_index], brightness);
        }
    }
    return true;
}
