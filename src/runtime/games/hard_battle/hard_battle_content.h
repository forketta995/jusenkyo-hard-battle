#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

// Rebinds the USA title graphic to the corresponding Japanese cartridge data.
// The caller supplies the already verified base ROM in memory.
bool apply_japanese_title_graphic(std::vector<uint8_t> &base, const std::filesystem::path &source);
bool apply_configured_hard_battle_content(std::vector<uint8_t> &base, const std::filesystem::path &executable);
