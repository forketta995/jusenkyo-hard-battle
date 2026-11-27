#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "wave_output.h"

#include <algorithm>

WaveOutput::~WaveOutput() {
    close();
}

bool WaveOutput::open() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = SnesAudio::channels;
    format.nSamplesPerSec = SnesAudio::sample_rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = SnesAudio::channels * sizeof(int16_t);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (waveOutOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        device_ = nullptr;
        return false;
    }
    for (auto &buffer : buffers_) {
        buffer.header = {};
        buffer.header.lpData = reinterpret_cast<char *>(buffer.samples.data());
        buffer.header.dwBufferLength = sizeof(buffer.samples);
        if (waveOutPrepareHeader(device_, &buffer.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            close();
            return false;
        }
    }
    prefill_ = waveOutPause(device_) == MMSYSERR_NOERROR ? 3 : 0;
    return true;
}

void WaveOutput::close() {
    if (!device_) {
        return;
    }
    waveOutReset(device_);
    for (auto &buffer : buffers_) {
        if (buffer.header.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(device_, &buffer.header, sizeof(WAVEHDR));
        }
    }
    waveOutClose(device_);
    device_ = nullptr;
}

void WaveOutput::submit(std::span<const int16_t> samples) {
    if (!device_ || samples.empty() || samples.size() > SnesAudio::buffer_samples) {
        return;
    }
    const auto available = std::ranges::find_if(buffers_, [](const Buffer &buffer) {
        return (buffer.header.dwFlags & WHDR_INQUEUE) == 0;
    });
    if (available == buffers_.end()) {
        return;
    }
    auto &buffer = *available;
    std::ranges::copy(samples, buffer.samples.begin());
    buffer.header.dwBufferLength = static_cast<DWORD>(samples.size_bytes());
    if (waveOutWrite(device_, &buffer.header, sizeof(WAVEHDR)) == MMSYSERR_NOERROR &&
        prefill_ != 0 && --prefill_ == 0) {
        waveOutRestart(device_);
    }
}
