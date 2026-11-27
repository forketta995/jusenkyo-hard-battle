#pragma once

#include "snes_ppu.h"

#include <array>
#include <cstdint>

struct SnesFrame {
    static constexpr int width = 256;
    static constexpr int height = SNES_VISIBLE_LINES;
    std::array<std::uint32_t, width * height> pixels{};
};

bool snes_render_mode1(const SnesPpu &ppu, SnesFrame &frame);
