#pragma once

#include "cpu_65816.h"
#include "snes_program.h"

#include <cstdint>
#include <filesystem>
#include <span>

struct SnesPreviewProgram {
    const char *name;
    bool (*boot)(Cpu65816 &);
    BootResult (*advance)(Cpu65816 *);
};

std::filesystem::path preview_executable_directory();
int show_preview_error(const char *title, const char *message);
int run_snes_preview(const SnesPreviewProgram &program, std::span<const std::uint8_t> rom);
