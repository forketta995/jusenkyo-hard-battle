#include "hard_battle_program.h"

BootResult hard_battle_preview_advance(Cpu65816 *cpu) {
    constexpr unsigned scheduler_slot = 0x0160;
    constexpr unsigned scheduler_callback = 0x0162;
    constexpr uint32_t title_callback = HARD_BATTLE_TITLE_CALLBACK;
    constexpr unsigned title_countdown = HARD_BATTLE_TITLE_COUNTDOWN;

    const auto *callback = cpu->work_ram + scheduler_callback + cpu->work_ram[scheduler_slot];
    const unsigned scene = callback[0] | (callback[1] << 8) | (callback[2] << 16);
    auto *countdown = cpu->work_ram + title_countdown;
    if (scene == title_callback && countdown[1] == 0 && countdown[0] <= 32) {
        // Extend by 256 frames to preserve the 32-frame blink phase. The
        // original title accepts Start while the countdown is below 300.
        countdown[1] = 1;
    }
    return hard_battle_continue(cpu);
}
