#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

std::filesystem::path configured_hard_battle_rom(const std::filesystem::path &executable);
std::vector<std::uint8_t> load_hard_battle_rom(const std::filesystem::path &path);
