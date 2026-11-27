#include "hard_battle_rom.h"
#include "hard_battle_program.h"
#include "rom_file.h"

std::filesystem::path configured_hard_battle_rom(const std::filesystem::path &executable) {
    return read_rom_selection(executable.parent_path() / "hard_battle_rom_path.txt");
}

std::vector<uint8_t> load_hard_battle_rom(const std::filesystem::path &path) {
    return read_verified_rom(path, hard_battle_rom_size, hard_battle_rom_crc);
}
