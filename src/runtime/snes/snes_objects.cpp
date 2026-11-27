#include "snes_objects.h"
#include "snes_tiles.h"

namespace {

struct ObjectSize {
    unsigned width;
    unsigned height;
};

constexpr ObjectSize sizes[8][2] = {
    {{8, 8}, {16, 16}}, {{8, 8}, {32, 32}}, {{8, 8}, {64, 64}}, {{16, 16}, {32, 32}},
    {{16, 16}, {64, 64}}, {{32, 32}, {64, 64}}, {{16, 32}, {32, 64}}, {{16, 32}, {32, 32}},
};

struct VisibleObject {
    unsigned x;
    unsigned row;
    unsigned tiles_wide;
    uint8_t tile;
    uint8_t attributes;
};

unsigned select_objects(const SnesPpu &ppu, unsigned y, std::array<VisibleObject, 32> &objects) {
    const unsigned first = (ppu.registers[3] & 0x80) ? (ppu.oam_address >> 2) & 127 : 0;
    const unsigned size_mode = ppu.registers[1] >> 5;
    unsigned count = 0;
    for (unsigned offset = 0; offset < 128 && count < objects.size(); ++offset) {
        const unsigned index = (first + offset) & 127;
        const auto *entry = &ppu.oam[index * 4];
        const unsigned extra = ppu.oam[0x200 + index / 4] >> ((index & 3) * 2);
        const auto size = sizes[size_mode][(extra >> 1) & 1];
        const unsigned x = entry[0] | ((extra & 1) << 8);
        unsigned row = (y - entry[1]) & 255;
        if (row >= size.height || (x > 256 && x + size.width <= 512)) {
            continue;
        }
        if (entry[3] & 0x80) {
            // Rectangular objects flip each square half independently.
            row ^= size.width - 1;
        }
        objects[count++] = {x, row, size.width / 8, entry[2], entry[3]};
    }
    return count;
}

} // namespace

void snes_render_object_line(const SnesPpu &ppu, unsigned y, SnesObjectLine &line) {
    std::array<VisibleObject, 32> objects;
    unsigned count = select_objects(ppu, y, objects);
    const unsigned base = (ppu.registers[1] & 7) << 13;
    const unsigned name_offset = (1 + ((ppu.registers[1] >> 3) & 3)) << 12;
    unsigned tiles_remaining = 34;
    // Fetch in reverse order so earlier opaque OAM entries win overlaps.
    while (count != 0 && tiles_remaining != 0) {
        const auto &object = objects[--count];
        const unsigned tile_base = base + ((object.attributes & 1) ? name_offset : 0);
        const unsigned tile_row = ((object.tile >> 4) + (object.row >> 3)) & 15;
        const unsigned palette = 128 + ((object.attributes & 0x0E) << 3);
        const auto priority = static_cast<uint8_t>((object.attributes >> 4) & 3);
        const bool flip = object.attributes & 0x40;
        for (unsigned column = 0; column < object.tiles_wide && tiles_remaining != 0; ++column) {
            const unsigned x = (object.x + column * 8) & 511;
            if (object.x != 256 && x >= 256 && x + 7 < 512) {
                continue;
            }
            --tiles_remaining;
            const unsigned source_column = flip ? object.tiles_wide - 1 - column : column;
            const unsigned tile_column = (object.tile + source_column) & 15;
            const unsigned address = tile_base + (tile_row * 16 + tile_column) * 16 + (object.row & 7);
            const auto low = snes_tiles::vram_word(ppu, address);
            const auto high = snes_tiles::vram_word(ppu, address + 8);
            for (unsigned pixel = 0; pixel < 8; ++pixel) {
                const unsigned screen_x = (x + pixel) & 511;
                const unsigned color = snes_tiles::planar_pixel(low, high, flip ? pixel : 7 - pixel);
                if (screen_x < line.size() && color != 0) {
                    line[screen_x] = {static_cast<uint8_t>(palette + color), priority};
                }
            }
        }
    }
}
