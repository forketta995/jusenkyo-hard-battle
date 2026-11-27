#include "hard_battle_program.h"
#include "hard_battle_rom.h"
#include "snes_audio.h"
#include "snes_frame.h"

#include <algorithm>
#include <iostream>
#include <span>

namespace {

unsigned scene_callback(const Cpu65816 &cpu) {
    const unsigned slot = cpu.work_ram[0x160];
    return cpu.work_ram[0x162 + slot] | (cpu.work_ram[0x163 + slot] << 8);
}

bool advance(Cpu65816 &cpu) {
    const auto result = hard_battle_preview_advance(&cpu);
    if (result == BOOT_FRAME) {
        return true;
    }
    std::cerr << "Japanese preview stopped with result " << result << " at 0x" << std::hex
              << cpu.pc << " after 0x" << cpu.last_block << ", scene 0x" << scene_callback(cpu) << '\n';
    return false;
}

bool press_and_wait(Cpu65816 &cpu, SnesAudio &audio, uint16_t button, unsigned frames) {
    for (unsigned i = 0; i < frames; ++i) {
        cpu.joypad_input[0] = i == 0 ? button : 0;
        if (!advance(cpu)) {
            return false;
        }
        audio.take_samples();
    }
    return true;
}

unsigned audible_count(std::span<const int16_t> samples) {
    return std::ranges::count_if(samples, [](int16_t sample) { return sample > 32 || sample < -32; });
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }
    const auto rom = load_hard_battle_rom(argv[1]);
    if (rom.empty()) {
        return 1;
    }
    Cpu65816 cpu;
    cpu_reset(&cpu);
    cpu.rom = rom.data();
    cpu.rom_size = static_cast<uint32_t>(rom.size());
    cpu.apply_preview_patches = true;
    SnesAudio audio;
    audio.attach(cpu.apu);
    if (hard_battle_boot(&cpu) != BOOT_CHECKPOINT || cpu.ppu.registers[0] != 15) {
        std::cerr << "Japanese boot stopped at 0x" << std::hex << cpu.pc << '\n';
        return 1;
    }
    audio.take_samples();

    unsigned title_frames = 0;
    unsigned audible_samples = 0;
    for (unsigned i = 0; i < 2000 && title_frames < 540; ++i) {
        if (!advance(cpu)) {
            return 1;
        }
        const auto samples = audio.take_samples();
        if (scene_callback(cpu) == 0x95D3 && cpu.ppu.registers[0] == 15) {
            ++title_frames;
            audible_samples += audible_count(samples);
        }
    }
    SnesFrame frame;
    if (title_frames != 540 || audible_samples < 32000 || !snes_render_mode1(cpu.ppu, frame) ||
        std::ranges::all_of(frame.pixels, [&frame](uint32_t pixel) { return pixel == frame.pixels.front(); })) {
        std::cerr << "Japanese title did not render and play audio after waiting.\n";
        return 1;
    }

    if (!press_and_wait(cpu, audio, JOYPAD_START, 48) || scene_callback(cpu) != 0x9693) {
        std::cerr << "Start did not reach the Japanese menu. Scene 0x" << std::hex << scene_callback(cpu) << '\n';
        return 1;
    }
    SnesFrame next;
    if (!press_and_wait(cpu, audio, JOYPAD_START, 700) || scene_callback(cpu) != 0xB717 ||
        !snes_render_mode1(cpu.ppu, next) || next.pixels == frame.pixels) {
        std::cerr << "The Japanese 1P introduction did not reach its dialogue.\n";
        return 1;
    }
    if (!press_and_wait(cpu, audio, JOYPAD_START, 450) || scene_callback(cpu) != 0x6EB6 ||
        !snes_render_mode1(cpu.ppu, frame) || !cpu.ppu.line_cgram_active || frame.pixels == next.pixels) {
        std::cerr << "Japanese character selection did not render.\n";
        return 1;
    }
    const auto cursor = [&cpu] { return cpu.work_ram[0x0A92] | (cpu.work_ram[0x0A94] << 8); };
    const auto initial_cursor = cursor();
    if (!press_and_wait(cpu, audio, JOYPAD_RIGHT, 7) || cursor() == initial_cursor ||
        !press_and_wait(cpu, audio, JOYPAD_LEFT, 7) || cursor() != initial_cursor) {
        std::cerr << "The character cursor did not move.\n";
        return 1;
    }
    if (!press_and_wait(cpu, audio, JOYPAD_A, 280) || scene_callback(cpu) != 0xB73E ||
        cpu.work_ram[0x0D14] != 0 || !snes_render_mode1(cpu.ppu, next) || next.pixels == frame.pixels) {
        std::cerr << "Ranma's Japanese cutscene did not render.\n";
        return 1;
    }
    cpu.joypad_input[0] = JOYPAD_START;
    for (unsigned i = 0; i < 850 && scene_callback(cpu) != 0xA7E7; ++i) {
        if (!advance(cpu)) {
            return 1;
        }
        audio.take_samples();
        cpu.joypad_input[0] = 0;
    }
    if (scene_callback(cpu) != 0xA7E7 || cpu.work_ram[0x0D16] != 9) {
        std::cerr << "The Japanese preview did not enter Ranma's first fight against Akane.\n";
        return 1;
    }
    const auto ranma_x = [&cpu] { return cpu.work_ram[0x072E] | (cpu.work_ram[0x072F] << 8); };
    const auto start_x = ranma_x();
    unsigned fight_samples = 0;
    for (unsigned i = 0; i < 100; ++i) {
        cpu.joypad_input[0] = i < 50 ? JOYPAD_RIGHT : (i == 52 || i == 85 ? JOYPAD_B : 0);
        if (!advance(cpu)) {
            return 1;
        }
        fight_samples += audible_count(audio.take_samples());
    }
    if (ranma_x() <= start_x || fight_samples < 32000 || !snes_render_mode1(cpu.ppu, frame) ||
        frame.pixels == next.pixels) {
        std::cerr << "The first Japanese fight did not update the player, video, and audio.\n";
        return 1;
    }
    for (unsigned i = 0; i < 29; ++i) {
        cpu.joypad_input[0] = i == 0 || i == 20 ? JOYPAD_START : 0;
        if (!advance(cpu)) {
            return 1;
        }
        audio.take_samples();
        if ((i == 19 && scene_callback(cpu) != 0xAE16) ||
            (i == 28 && scene_callback(cpu) != 0xA7E7)) {
            std::cerr << "The Japanese fight did not pause and resume.\n";
            return 1;
        }
    }
    for (unsigned i = 0; i < 4200 && scene_callback(cpu) != 0xAE34; ++i) {
        if (!advance(cpu)) {
            return 1;
        }
        audio.take_samples();
    }
    if (scene_callback(cpu) != 0xAE34) {
        std::cerr << "The Japanese match did not reach the Continue path.\n";
        return 1;
    }
    if (!press_and_wait(cpu, audio, 0, 75) || cpu.ppu.registers[0] != 15 ||
        !snes_render_mode1(cpu.ppu, next) || next.pixels == frame.pixels) {
        std::cerr << "The Japanese Continue screen did not become visible.\n";
        return 1;
    }
    cpu.joypad_input[0] = JOYPAD_A;
    for (unsigned i = 0; i < 1200; ++i) {
        if (!advance(cpu)) {
            return 1;
        }
        audio.take_samples();
        cpu.joypad_input[0] = 0;
        if (scene_callback(cpu) == 0xA7E7) {
            return 0;
        }
    }
    std::cerr << "Confirming Continue did not start a new Japanese match.\n";
    return 1;
}
