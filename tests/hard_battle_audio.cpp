#include "hard_battle_program.h"
#include "hard_battle_rom.h"
#include "snes_audio.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

bool boot_with_audio(Cpu65816 &cpu, SnesAudio &audio, std::span<const uint8_t> rom) {
    cpu_reset(&cpu);
    cpu.apply_preview_patches = true;
    cpu.rom = rom.data();
    cpu.rom_size = rom.size();
    audio.attach(cpu.apu);
    const auto boot = hard_battle_boot(&cpu);
    if (boot != BOOT_CHECKPOINT || cpu.ppu.registers[0] != 15) {
        std::cerr << "Audio boot stopped: " << boot << " at " << std::hex << cpu.pc
                  << " after " << std::dec << cpu.frame_count << " frames\n";
        return false;
    }
    audio.take_samples();
    return true;
}

constexpr unsigned cursor_move_frame = 211;
constexpr unsigned effect_end_frame = cursor_move_frame + 12;

unsigned scene_callback(const Cpu65816 &cpu) {
    const unsigned slot = cpu.work_ram[0x160];
    return cpu.work_ram[0x162 + slot] | (cpu.work_ram[0x163 + slot] << 8);
}

bool wait_on_title_then_start(Cpu65816 &cpu, SnesAudio &audio) {
    for (unsigned frame = 0; frame < 1300; ++frame) {
        if (hard_battle_preview_advance(&cpu) != BOOT_FRAME ||
            (frame > 60 && (scene_callback(cpu) != 0xA116 || cpu.ppu.registers[0] != 15))) {
            std::cerr << "The title stopped responding while idle at " << std::hex << cpu.pc << '\n';
            return false;
        }
        audio.take_samples();
    }
    cpu.joypad_input[0] = JOYPAD_START;
    for (unsigned frame = 0; frame < 32; ++frame) {
        if (hard_battle_preview_advance(&cpu) != BOOT_FRAME) {
            return false;
        }
        audio.take_samples();
        cpu.joypad_input[0] = 0;
    }
    return scene_callback(cpu) == 0xA1DB;
}

std::vector<int16_t> capture_idle_menu(Cpu65816 &cpu, SnesAudio &audio) {
    std::vector<int16_t> samples;
    for (unsigned frame = 0; frame < effect_end_frame; ++frame) {
        cpu.joypad_input[0] = frame == 180 ? JOYPAD_START : 0;
        if (hard_battle_preview_advance(&cpu) != BOOT_FRAME) {
            return {};
        }
        const auto block = audio.take_samples();
        if (frame >= cursor_move_frame - 1) {
            samples.insert(samples.end(), block.begin(), block.end());
        }
    }
    return samples;
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
    SnesAudio audio;
    if (!boot_with_audio(cpu, audio, rom)) {
        return 1;
    }

    unsigned title_samples = 0;
    unsigned menu_samples = 0;
    unsigned fight_samples = 0;
    bool reached_fight = false;
    bool reached_continue = false;
    unsigned fight_frames = 0;
    std::vector<int16_t> menu_effect;
    size_t before_effect = 0;
    for (unsigned frame = 0; frame < 6000 && !reached_continue; ++frame) {
        switch (frame) {
        case 180: case 260: case 930: case 1350: cpu.joypad_input[0] = JOYPAD_START; break;
        case cursor_move_frame: cpu.joypad_input[0] = JOYPAD_RIGHT; break;
        case 224: cpu.joypad_input[0] = JOYPAD_LEFT; break;
        case 1200: cpu.joypad_input[0] = JOYPAD_A; break;
        default: cpu.joypad_input[0] = 0; break;
        }
        if (reached_fight) {
            if (fight_frames < 45) {
                cpu.joypad_input[0] = JOYPAD_RIGHT;
            } else if (fight_frames == 47 || fight_frames == 78) {
                cpu.joypad_input[0] = JOYPAD_B;
            }
            ++fight_frames;
        }
        const auto result = hard_battle_preview_advance(&cpu);
        if (result != BOOT_FRAME) {
            std::cerr << "Audio playback stopped: " << result << " at " << std::hex << cpu.pc
                      << " after " << std::dec << frame << " preview frames\n";
            return 1;
        }
        const unsigned scene = scene_callback(cpu);
        reached_fight |= scene == 0x66DA && cpu.work_ram[0x0D16] == 9;
        reached_continue = reached_fight && scene == 0x6DDA && cpu.work_ram[0x1900] == 2;
        const auto samples = audio.take_samples();
        if (frame >= cursor_move_frame - 1 && frame < effect_end_frame) {
            menu_effect.insert(menu_effect.end(), samples.begin(), samples.end());
            if (frame == cursor_move_frame - 1) {
                before_effect = samples.size();
            }
        }
        const auto audible = std::ranges::count_if(samples, [](int16_t sample) {
            return std::abs(static_cast<int>(sample)) > 32;
        });
        if (frame < 180) {
            title_samples += audible;
        } else if (frame >= 200 && frame < 260) {
            menu_samples += audible;
        } else if (reached_fight) {
            fight_samples += audible;
        }
    }
    if (!reached_continue || title_samples < 32000 || menu_samples < 8000 || fight_samples < 32000) {
        std::cerr << "Audio output did not cover the title, menu and fight: " << title_samples
                  << ", " << menu_samples << ", " << fight_samples
                  << "; reached Continue: " << reached_continue << '\n';
        return 1;
    }
    if (!boot_with_audio(cpu, audio, rom)) {
        return 1;
    }
    const auto idle_menu = capture_idle_menu(cpu, audio);
    if (!before_effect || idle_menu.size() <= before_effect || idle_menu.size() != menu_effect.size() ||
        !std::equal(menu_effect.begin(), menu_effect.begin() + before_effect, idle_menu.begin()) ||
        idle_menu == menu_effect) {
        std::cerr << "Moving the menu cursor did not produce a distinct sound effect.\n";
        return 1;
    }
    if (!boot_with_audio(cpu, audio, rom) || !wait_on_title_then_start(cpu, audio)) {
        std::cerr << "Start did not open the original menu after waiting on the title.\n";
        return 1;
    }
}
