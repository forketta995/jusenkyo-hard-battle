#include "hard_battle_content.h"

#include "hard_battle_program.h"
#include "rom_file.h"

#include <span>

namespace {

constexpr uint32_t usa_crc = 0x8E4DADCD;
constexpr uint32_t japan_crc = 0x053B2615;
constexpr uint32_t cartridge_size = 0x180000;
constexpr unsigned graphic_table = 0x030000;
constexpr unsigned usa_title_graphic = 0x00A9;
constexpr unsigned japan_title_graphic = 0x00A2;
constexpr unsigned japan_title_graphic_size = 1076;

unsigned asset_offset(std::span<const uint8_t> rom, unsigned selector) {
    const unsigned entry = graphic_table + selector * 3;
    return rom[entry] | (rom[entry + 1] << 8) | ((rom[entry + 2] & 0x3F) << 16);
}

} // namespace

bool apply_japanese_title_graphic(std::vector<uint8_t> &base, const std::filesystem::path &source) {
    if (hard_battle_rom_crc != usa_crc || base.size() != cartridge_size) {
        return false;
    }
    const auto donor = read_verified_rom(source, cartridge_size, japan_crc);
    if (donor.empty()) {
        return false;
    }
    const auto source_offset = asset_offset(donor, japan_title_graphic);
    const auto base_offset = asset_offset(base, usa_title_graphic);
    const auto end = source_offset + japan_title_graphic_size;
    if (end > donor.size() || base_offset + 3 > base.size() ||
        donor[source_offset] != base[base_offset] ||
        donor[source_offset + 1] != base[base_offset + 1] ||
        donor[source_offset + 2] != base[base_offset + 2]) {
        return false;
    }

    // Use bank D8 in the in-memory cartridge view, beyond the original image.
    // The stock loader decompresses the replacement through its usual pointer table.
    base.insert(base.end(), donor.begin() + source_offset, donor.begin() + end);
    const unsigned entry = graphic_table + usa_title_graphic * 3;
    base[entry] = 0;
    base[entry + 1] = 0;
    base[entry + 2] = 0xD8;
    return true;
}

bool apply_configured_hard_battle_content(std::vector<uint8_t> &base, const std::filesystem::path &executable) {
    const auto selection = executable.parent_path() / "hard_battle_title_graphic_rom_path.txt";
    std::error_code error;
    const bool configured = std::filesystem::exists(selection, error);
    return !error && (!configured || apply_japanese_title_graphic(base, read_rom_selection(selection)));
}
