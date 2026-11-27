#ifdef RAMMA_HARD_BATTLE
#include "hard_battle_diagnostics.h"
#else
#include <iostream>
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
#ifdef RAMMA_HARD_BATTLE
    return run_hard_battle_diagnostics(argc, argv);
#else
    std::cout << "Set RAMMA_HARD_BATTLE_ROM in CMake to build the Hard Battle reset prototype.\n";
#endif
}
