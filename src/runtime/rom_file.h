#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

std::filesystem::path read_rom_selection(const std::filesystem::path &selection_path);
std::vector<std::uint8_t> read_verified_rom(const std::filesystem::path &path,
                                            std::uint32_t size, std::uint32_t expected_crc);
