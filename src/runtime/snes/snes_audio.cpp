#include "snes_audio.h"

#include <algorithm>
#include <string_view>

#include "SNES_SPC.h"

namespace {
constexpr uint32_t ntsc_dot_rate = 5369318;
constexpr int clocks_per_buffer = SnesAudio::buffer_samples / SnesAudio::channels *
                                  SNES_SPC::clocks_per_sample;
}

SnesAudio::SnesAudio() : chip_(std::make_unique<SNES_SPC>()) {
    chip_->init();
}

SnesAudio::~SnesAudio() = default;

void SnesAudio::attach(SnesApu &apu) {
    dot_remainder_ = 0;
    clocks_ = 0;
    started_ = false;
    apu.audio = this;
}

bool SnesAudio::start(const SnesApu &apu) {
    // The bus has already performed the IPL upload. Transfer the uploaded RAM
    // and entry point to the clocked SPC700 core through its snapshot API.
    constexpr unsigned ram_offset = 0x100;
    constexpr unsigned dsp_offset = ram_offset + 0x10000;
    std::array<uint8_t, 0x10200> snapshot{};
    constexpr std::string_view signature = "SNES-SPC700 Sound File Data v0.30\x1A\x1A";
    std::ranges::copy(signature, snapshot.begin());
    snapshot[0x25] = static_cast<uint8_t>(apu.entry_address);
    snapshot[0x26] = static_cast<uint8_t>(apu.entry_address >> 8);
    snapshot[0x2B] = 0xEF;
    auto ram = snapshot.begin() + ram_offset;
    std::ranges::copy(apu.ram, ram);
    ram[0xF0] = 0x0A;
    ram[0xF1] = 0;
    std::ranges::copy(apu.to_cpu, ram + 0xF4);
    std::ranges::copy(apu.dsp, snapshot.begin() + dsp_offset);
    started_ = chip_->load_spc(snapshot.data(), static_cast<long>(snapshot.size())) == nullptr;
    if (started_) {
        for (unsigned port = 0; port < 4; ++port) {
            chip_->write_port(0, port, apu.from_cpu[port]);
        }
        chip_->set_output(pcm_[output_].data(), pcm_[output_].size());
    }
    return started_;
}

void SnesAudio::advance(unsigned dots) {
    if (!started_) {
        return;
    }
    const uint64_t elapsed = dot_remainder_ + static_cast<uint64_t>(dots) * SNES_SPC::clock_rate;
    dot_remainder_ = elapsed % ntsc_dot_rate;
    const auto clocks = static_cast<int>(elapsed / ntsc_dot_rate);
    // During boot or a long step, keep the chip moving without overflowing the output buffer.
    if (clocks_ + clocks >= clocks_per_buffer) {
        take_samples();
    }
    clocks_ += clocks;
}

uint8_t SnesAudio::read(unsigned port) {
    return static_cast<uint8_t>(chip_->read_port(clocks_, port));
}

void SnesAudio::write(unsigned port, uint8_t value) {
    chip_->write_port(clocks_, port, value);
}

std::span<const int16_t> SnesAudio::take_samples() {
    if (!started_) {
        return {};
    }
    chip_->end_frame(clocks_);
    clocks_ = 0;
    const auto count = static_cast<size_t>(chip_->sample_count());
    const auto *samples = pcm_[output_].data();
    output_ = 1 - output_;
    chip_->set_output(pcm_[output_].data(), pcm_[output_].size());
    return {samples, count};
}
