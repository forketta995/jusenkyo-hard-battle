#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <span>

#include "snes_apu.h"

struct SNES_SPC;

struct SnesAudio {
    static constexpr unsigned sample_rate = 32000;
    static constexpr unsigned channels = 2;
    static constexpr unsigned buffer_samples = 2048;

    SnesAudio();
    ~SnesAudio();
    void attach(SnesApu &apu);
    bool start(const SnesApu &apu);
    void advance(unsigned dots);
    uint8_t read(unsigned port);
    void write(unsigned port, uint8_t value);
    std::span<const int16_t> take_samples();

private:
    std::unique_ptr<SNES_SPC> chip_;
    std::array<std::array<int16_t, buffer_samples>, 2> pcm_{};
    unsigned output_ = 0;
    uint32_t dot_remainder_ = 0;
    int clocks_ = 0;
    bool started_ = false;
};
