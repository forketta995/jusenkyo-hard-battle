#pragma once

#include <windows.h>
#include <mmsystem.h>

#include "snes_audio.h"

class WaveOutput {
public:
    WaveOutput() = default;
    ~WaveOutput();
    WaveOutput(const WaveOutput &) = delete;
    WaveOutput &operator=(const WaveOutput &) = delete;

    bool open();
    void close();
    void submit(std::span<const int16_t> samples);

private:
    struct Buffer {
        std::array<int16_t, SnesAudio::buffer_samples> samples{};
        WAVEHDR header{};
    };

    HWAVEOUT device_ = nullptr;
    std::array<Buffer, 8> buffers_{};
    unsigned prefill_ = 0;
};
