#pragma once

#include "snes_frame.h"

struct SnesObjectPixel {
    uint8_t palette_index;
    uint8_t priority;
};

using SnesObjectLine = std::array<SnesObjectPixel, SnesFrame::width>;

void snes_render_object_line(const SnesPpu &ppu, unsigned y, SnesObjectLine &line);
