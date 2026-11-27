#include "rom_file.h"

#include <fstream>
#include <string>

namespace {

uint32_t crc32(const std::vector<uint8_t> &data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (const auto byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0u);
        }
    }
    return ~crc;
}

} // namespace

std::filesystem::path read_rom_selection(const std::filesystem::path &selection_path) {
    std::ifstream selection(selection_path);
    std::string line;
    if (!std::getline(selection, line)) {
        return {};
    }
    return std::u8string(line.begin(), line.end());
}

std::vector<uint8_t> read_verified_rom(const std::filesystem::path &path,
                                      uint32_t size, uint32_t expected_crc) {
    std::ifstream input(path, std::ios::binary);
    std::vector<uint8_t> rom(size);
    if (!input.read(reinterpret_cast<char *>(rom.data()), rom.size()) ||
        input.peek() != std::char_traits<char>::eof() || crc32(rom) != expected_crc) {
        return {};
    }
    return rom;
}
