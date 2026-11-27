#include "hard_battle_program.h"
#include "hard_battle_rom.h"
#include "snes_audio.h"
#include "snes_frame.h"

#include <algorithm>
#include <iostream>

namespace {
bool advance_frames(Cpu65816 &cpu, int count, uint16_t buttons = 0) {
    cpu.joypad_input[0] = buttons;
    for (int i = 0; i < count; ++i) {
        if (hard_battle_continue(&cpu) != BOOT_FRAME) {
            std::cerr << "Execution stopped before the next frame at 0x" << std::hex << cpu.pc
                      << " after block 0x" << cpu.last_block
                      << " (opponent " << static_cast<unsigned>(cpu.work_ram[0x0D16])
                      << ", stage " << static_cast<unsigned>(cpu.work_ram[0x72]) << ").\n";
            return false;
        }
        if ((cpu.ppu.registers[5] & 7) != 1) {
            std::cerr << "The preview reached an unsupported background mode.\n";
            return false;
        }
    }
    return true;
}

uint16_t scene_callback(const Cpu65816 &cpu) {
    const unsigned slot = cpu.work_ram[0x160];
    return cpu.work_ram[0x162 + slot] | (cpu.work_ram[0x163 + slot] << 8);
}

bool has_dialogue_text(const SnesFrame &frame) {
    const auto start = frame.pixels.begin() + 144 * SnesFrame::width;
    return std::count_if(start, frame.pixels.end(), [](uint32_t pixel) { return pixel != 0; }) > 100;
}

bool start_fight_replay(Cpu65816 &cpu, int cutscene_frames, SnesAudio *audio = nullptr) {
    const auto *rom = cpu.rom;
    const auto rom_size = cpu.rom_size;
    cpu_reset(&cpu);
    cpu.rom = rom;
    cpu.rom_size = rom_size;
    cpu.apply_preview_patches = true;
    if (audio) {
        audio->attach(cpu.apu);
    }
    if (hard_battle_boot(&cpu) != BOOT_CHECKPOINT) {
        return false;
    }

    struct InputSpan {
        int frames;
        uint16_t buttons;
    };
    // Replay the inputs that triggered Akane's close-range counter.
    constexpr InputSpan approach[] = {
        {180, 0}, {4, JOYPAD_START}, {27, 0}, {1, JOYPAD_RIGHT}, {12, 0}, {1, JOYPAD_LEFT},
        {35, 0}, {1, JOYPAD_START}, {669, 0}, {1, JOYPAD_START}, {269, 0}, {1, JOYPAD_A},
    };
    for (const auto &input : approach) {
        if (!advance_frames(cpu, input.frames, input.buttons)) {
            return false;
        }
    }
    return advance_frames(cpu, cutscene_frames) && advance_frames(cpu, 1, JOYPAD_START);
}

bool replay_close_range(Cpu65816 &cpu) {
    if (!start_fight_replay(cpu, 149) || !advance_frames(cpu, 454) ||
        !advance_frames(cpu, 57, JOYPAD_RIGHT)) {
        return false;
    }
    for (unsigned attack = 0; attack < 19; ++attack) {
        if (!advance_frames(cpu, 3, JOYPAD_B) || !advance_frames(cpu, 18)) {
            return false;
        }
    }
    const auto ranma_health = cpu.work_ram[0x0CC6] | (cpu.work_ram[0x0CC7] << 8);
    return scene_callback(cpu) == 0x66DA && ranma_health > 0 && ranma_health < 0x6000;
}

bool replay_retreat_during_exchange(Cpu65816 &cpu) {
    if (!start_fight_replay(cpu, 187)) {
        return false;
    }
    for (int remaining = 600; remaining > 0 &&
         !(scene_callback(cpu) == 0x36EC && cpu.ppu.registers[0] == 15); --remaining) {
        if (!advance_frames(cpu, 1)) {
            return false;
        }
    }
    if (scene_callback(cpu) != 0x36EC || !advance_frames(cpu, 49, JOYPAD_RIGHT) ||
        !advance_frames(cpu, 3)) {
        return false;
    }

    // Alternating attack and retreat reproduces the state that stopped at C03E71.
    constexpr uint16_t inputs[] = {
        JOYPAD_DOWN, JOYPAD_B, 0, JOYPAD_LEFT, JOYPAD_UP, 0, JOYPAD_UP,
        JOYPAD_RIGHT | JOYPAD_B, JOYPAD_RIGHT, JOYPAD_RIGHT | JOYPAD_A, JOYPAD_DOWN | JOYPAD_B,
        0, JOYPAD_RIGHT, JOYPAD_RIGHT, 0, JOYPAD_LEFT, JOYPAD_LEFT, 0, JOYPAD_LEFT, JOYPAD_B,
        0, JOYPAD_DOWN | JOYPAD_B, JOYPAD_A, JOYPAD_RIGHT, JOYPAD_DOWN, JOYPAD_RIGHT,
    };
    bool reacted_to_hit = false;
    for (const auto input : inputs) {
        for (int frame = 0; frame < 13; ++frame) {
            if (!advance_frames(cpu, 1, input)) {
                return false;
            }
            const auto state = cpu.work_ram[0x0D20] | (cpu.work_ram[0x0D21] << 8);
            reacted_to_hit |= state == 0x949B;
        }
    }
    return reacted_to_hit && scene_callback(cpu) == 0x66DA;
}

bool replay_akane_fighter_states(Cpu65816 &cpu, SnesAudio &audio) {
    if (!start_fight_replay(cpu, 149, &audio)) {
        return false;
    }
    for (int remaining = 760; remaining > 0 && scene_callback(cpu) != 0x66DA; --remaining) {
        if (!advance_frames(cpu, 1)) {
            return false;
        }
    }
    if (scene_callback(cpu) != 0x66DA || cpu.work_ram[0x0D16] != 9) {
        return false;
    }

    if (!advance_frames(cpu, 2, JOYPAD_START) || !advance_frames(cpu, 60) ||
        scene_callback(cpu) != 0x6D0E || !advance_frames(cpu, 2, JOYPAD_START) ||
        !advance_frames(cpu, 10) || scene_callback(cpu) != 0x66DA) {
        std::cerr << "Start did not pause and resume the first fight.\n";
        return false;
    }

    // Start each cartridge state on its first update. States C1:946B and
    // C1:947D use C0:3EFB on entry and C0:3F11 on following updates.
    constexpr uint16_t states[] = {0x946B, 0x947D, 0x9435, 0x9447, 0x9459};
    for (const auto state : states) {
        cpu.work_ram[0x0D20] = static_cast<uint8_t>(state);
        cpu.work_ram[0x0D21] = static_cast<uint8_t>(state >> 8);
        cpu.work_ram[0x0D2C] = cpu.work_ram[0x0D2D] = 0;
        if (!advance_frames(cpu, 24) || scene_callback(cpu) != 0x66DA) {
            std::cerr << "Akane's state at C1:" << std::hex << state << " did not finish updating.\n";
            return false;
        }
    }
    return true;
}

bool complete_and_retry_match(Cpu65816 &cpu, SnesFrame &frame) {
    if (!start_fight_replay(cpu, 149) || !advance_frames(cpu, 454) ||
        !advance_frames(cpu, 52, JOYPAD_RIGHT) || !advance_frames(cpu, 2) ||
        !advance_frames(cpu, 1, JOYPAD_B) || !advance_frames(cpu, 30) ||
        !advance_frames(cpu, 1, JOYPAD_B) || !advance_frames(cpu, 29) ||
        !advance_frames(cpu, 1, JOYPAD_UP)) {
        return false;
    }

    constexpr unsigned akane_rounds = 0x1900;
    const auto ranma_health = [&cpu] {
        return cpu.work_ram[0x0CC6] | (cpu.work_ram[0x0CC7] << 8);
    };
    bool started_next_round = false;
    for (int remaining = 2800; remaining > 0 && scene_callback(cpu) != 0x6DDA; --remaining) {
        if (!advance_frames(cpu, 1)) {
            return false;
        }
        started_next_round |= cpu.work_ram[akane_rounds] == 1 && ranma_health() == 0x6000;
    }
    if (!started_next_round || scene_callback(cpu) != 0x6DDA || cpu.work_ram[akane_rounds] != 2 ||
        !advance_frames(cpu, 60) || !snes_render_mode1(cpu.ppu, frame) ||
        std::ranges::all_of(frame.pixels, [](uint32_t pixel) { return pixel == 0; }) ||
        !advance_frames(cpu, 1, JOYPAD_A)) {
        return false;
    }
    for (int remaining = 650; remaining > 0; --remaining) {
        if (!advance_frames(cpu, 1)) {
            return false;
        }
        if (scene_callback(cpu) == 0x36EC && cpu.work_ram[akane_rounds] == 0 &&
            ranma_health() == 0x6000) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }
    const auto rom = load_hard_battle_rom(argv[1]);
    if (rom.empty()) {
        std::cerr << "The test could not read the local ROM.\n";
        return 1;
    }

    Cpu65816 cpu;
    cpu_reset(&cpu);
    cpu.apply_preview_patches = true;
    cpu.rom = rom.data();
    cpu.rom_size = rom.size();
    std::ranges::fill(cpu.ppu.vram, 0xA5);
    if (hard_battle_boot(&cpu) != BOOT_CHECKPOINT || cpu.pc != 0xC005A1 ||
        cpu.delivered_irqs == 0 || cpu.ppu.registers[0] != 15) {
        std::cerr << "The main CPU did not reach the title fade's full-brightness frame.\n";
        return 1;
    }
    if (cpu.dma_bytes != 167776 ||
        !std::ranges::any_of(cpu.ppu.vram, cpu.ppu.vram + 0x4000,
                            [](uint8_t byte) { return byte != 0 && byte != 0xA5; }) ||
        !std::ranges::any_of(cpu.ppu.vram + 0xA000, cpu.ppu.vram + 0xC000,
                            [](uint8_t byte) { return byte != 0 && byte != 0xA5; })) {
        std::cerr << "The opening scene's tile and map data did not reach VRAM.\n";
        return 1;
    }
    if (!cpu.apu.driver_started || cpu.apu.driver_fault ||
        cpu.apu.acknowledged_packets != 14173 || cpu.apu.dsp[0x5D] != 0x1E) {
        std::cerr << "The SPC700 driver did not acknowledge the opening sound packets.\n";
        return 1;
    }
    SnesFrame frame;
    if (!snes_render_mode1(cpu.ppu, frame) ||
        std::ranges::count_if(frame.pixels, [](uint32_t pixel) { return pixel != 0; }) < 50000 ||
        std::ranges::all_of(frame.pixels, [&frame](uint32_t pixel) { return pixel == frame.pixels.front(); })) {
        std::cerr << "The title framebuffer does not contain the expected visible graphics.\n";
        return 1;
    }

    SnesFrame menu;
    if (!advance_frames(cpu, 180) || !advance_frames(cpu, 1, JOYPAD_START) ||
        !advance_frames(cpu, 30) || scene_callback(cpu) != 0xA1DB ||
        !snes_render_mode1(cpu.ppu, menu) || frame.pixels == menu.pixels) {
        std::cerr << "Start did not open the original title menu.\n";
        return 1;
    }
    constexpr unsigned menu_selection = 0xF2;
    if (!advance_frames(cpu, 1, JOYPAD_RIGHT) || !advance_frames(cpu, 12) ||
        cpu.work_ram[menu_selection] != 1 ||
        !advance_frames(cpu, 1, JOYPAD_LEFT) || !advance_frames(cpu, 35) ||
        cpu.work_ram[menu_selection] != 0) {
        std::cerr << "The controller did not move the selection between 1P and 2P.\n";
        return 1;
    }
    SnesFrame one_player;
    if (!advance_frames(cpu, 1, JOYPAD_START) || !advance_frames(cpu, 90) ||
        scene_callback(cpu) != 0xB321 || cpu.ppu.registers[0] != 15 ||
        !snes_render_mode1(cpu.ppu, one_player) || menu.pixels == one_player.pixels) {
        std::cerr << "Confirming 1P did not reach the next scene at full brightness.\n";
        return 1;
    }
    if (!advance_frames(cpu, 330) || scene_callback(cpu) != 0xB551) {
        std::cerr << "The 1P scene did not keep running during its timer callback.\n";
        return 1;
    }
    if (!advance_frames(cpu, 249) || scene_callback(cpu) != 0xB578 ||
        !snes_render_mode1(cpu.ppu, one_player)) {
        std::cerr << "The 1P scene did not reach the dialogue.\n";
        return 1;
    }
    const uint8_t screen_layers = cpu.ppu.registers[0x2C];
    cpu.ppu.registers[0x2C] &= ~0x10;
    const bool rendered_backgrounds = snes_render_mode1(cpu.ppu, frame);
    cpu.ppu.registers[0x2C] = screen_layers;
    if (!rendered_backgrounds || frame.pixels == one_player.pixels) {
        std::cerr << "The 1P dialogue did not composite a visible object from OAM.\n";
        return 1;
    }
    if (!advance_frames(cpu, 1, JOYPAD_START) || !advance_frames(cpu, 17) ||
        scene_callback(cpu) != 0xB382 || cpu.ppu.registers[0] != 0) {
        std::cerr << "Start did not advance the dialogue through its fade to black.\n";
        return 1;
    }
    if (!advance_frames(cpu, 247) || scene_callback(cpu) != 0x79FE ||
        cpu.ppu.visible_display != 15 || !cpu.ppu.line_cgram_active ||
        std::ranges::equal(cpu.ppu.line_cgram[16], cpu.ppu.line_cgram[208]) ||
        !snes_render_mode1(cpu.ppu, frame) ||
        std::ranges::all_of(frame.pixels, [](uint32_t pixel) { return pixel == 0; })) {
        std::cerr << "The character screen did not render with its scanline palettes.\n";
        return 1;
    }
    constexpr unsigned character_cursor = 0x0A94;
    if (cpu.work_ram[character_cursor] != 0 ||
        !advance_frames(cpu, 1, JOYPAD_RIGHT) || !advance_frames(cpu, 6) ||
        cpu.work_ram[character_cursor] != 1 || !snes_render_mode1(cpu.ppu, menu) ||
        frame.pixels == menu.pixels ||
        !advance_frames(cpu, 1, JOYPAD_LEFT) || !advance_frames(cpu, 6) ||
        cpu.work_ram[character_cursor] != 0) {
        std::cerr << "The character cursor did not move between portraits.\n";
        return 1;
    }
    for (unsigned movement = 1; movement <= 10; ++movement) {
        if (!advance_frames(cpu, 1, JOYPAD_RIGHT) || !advance_frames(cpu, 6) ||
            cpu.work_ram[character_cursor] != movement % 10) {
            std::cerr << "The character cursor did not cycle over all ten portraits.\n";
            return 1;
        }
    }
    if (!advance_frames(cpu, 1, JOYPAD_A) || !advance_frames(cpu, 2) ||
        cpu.work_ram[0x06BA] != 0x80 || cpu.work_ram[0x06BB] != 0x7F ||
        !advance_frames(cpu, 59) || scene_callback(cpu) != 0x7AA9) {
        std::cerr << "The selected character was not confirmed before the next scene.\n";
        return 1;
    }
    if (!advance_frames(cpu, 198) || cpu.work_ram[0x0D14] != 0 ||
        scene_callback(cpu) != 0xB578 || !snes_render_mode1(cpu.ppu, frame) ||
        !has_dialogue_text(frame)) {
        std::cerr << "The selected Ranma did not appear in his opening dialogue.\n";
        return 1;
    }
    const auto portrait_end = frame.pixels.begin() + 136 * SnesFrame::width;
    if (!advance_frames(cpu, 790) || !snes_render_mode1(cpu.ppu, menu) ||
        !has_dialogue_text(menu) || std::equal(frame.pixels.begin(), portrait_end, menu.pixels.begin())) {
        std::cerr << "The Principal did not appear and reply to Ranma.\n";
        return 1;
    }
    if (!advance_frames(cpu, 2060) || !snes_render_mode1(cpu.ppu, one_player) ||
        !has_dialogue_text(one_player) || menu.pixels == one_player.pixels) {
        std::cerr << "The cutscene did not advance to its final narration.\n";
        return 1;
    }
    for (unsigned remaining = 760; remaining > 0 && scene_callback(cpu) != 0x66DA; --remaining) {
        if (!advance_frames(cpu, 1, JOYPAD_RIGHT)) {
            return 1;
        }
    }
    if (scene_callback(cpu) != 0x66DA || cpu.work_ram[0x0D16] != 9 ||
        !snes_render_mode1(cpu.ppu, frame) || frame.pixels == one_player.pixels) {
        std::cerr << "The cutscene did not transition into the first fight (task 0x" << std::hex
                  << scene_callback(cpu) << ", opponent " << static_cast<unsigned>(cpu.work_ram[0x0D16])
                  << ", display " << static_cast<unsigned>(cpu.ppu.visible_display) << ").\n";
        return 1;
    }
    constexpr unsigned ranma_x = 0x072E;
    constexpr unsigned akane_x = 0x0730;
    const auto starting_x = cpu.work_ram[ranma_x];
    const auto opponent_x = cpu.work_ram[akane_x];
    if (!advance_frames(cpu, 18, JOYPAD_RIGHT) || cpu.work_ram[ranma_x] <= starting_x ||
        cpu.work_ram[akane_x] == opponent_x || !snes_render_mode1(cpu.ppu, menu) ||
        frame.pixels == menu.pixels) {
        std::cerr << "The round did not respond to the player's movement and update the opponent.\n";
        return 1;
    }
    if (!replay_close_range(cpu)) {
        std::cerr << "Akane's close-range counter stopped the fight before the next exchange.\n";
        return 1;
    }
    if (!replay_retreat_during_exchange(cpu)) {
        std::cerr << "The fight did not continue through Ranma's close-range hit reaction.\n";
        return 1;
    }
    SnesAudio audio;
    if (!replay_akane_fighter_states(cpu, audio)) {
        std::cerr << "The fight did not continue through Akane's fighter states.\n";
        return 1;
    }
    if (!complete_and_retry_match(cpu, frame)) {
        std::cerr << "The first match did not advance through two losses, Continue, and a fresh round.\n";
        return 1;
    }
}
