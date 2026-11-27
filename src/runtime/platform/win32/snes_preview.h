#pragma once

#include <windows.h>

#include "cpu_65816.h"
#include "snes_program.h"

#include <filesystem>
#include <span>

struct SnesPreviewProgram {
    const wchar_t *name;
    bool (*boot)(Cpu65816 &);
    BootResult (*advance)(Cpu65816 *);
};

std::filesystem::path preview_executable_path();
int show_preview_error(const wchar_t *title, const wchar_t *message);
int run_snes_preview(HINSTANCE instance, int show_mode, const SnesPreviewProgram &program,
                     std::span<const uint8_t> rom);
