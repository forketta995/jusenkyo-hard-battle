# AGENTS.md — jusenkyo-hard-battle

## Project Overview

This repository hosts the Hard Battle static recompilation and the SNES tooling
and runtime it currently uses. The lifter reads the selected profile in
`config/` and outputs code into `<build>/src/generated/<edition>/`.

## Directory Structure

- `../roms/`: Local ROMs in the parent workspace. A standalone checkout may use any local path. ROMs are strictly read-only.
- `tools/`: Lifters, disassemblers, and Python static analysis scripts.
- `config/<game_id>/`: TOML configuration files for symbols, jump tables, patches, and lifter settings.
- `<build>/src/generated/<game_id>/`: Auto-generated C/C++ code produced by the lifter (Git-ignored).
- `src/runtime/`: Shared Host Layer (memory management, OS/syscall HLE stubs, audio, input, video).
- `docs/`: Technical documentation (ABI, memory maps, runtime macros).

## Hard Rules

1. **NEVER manually edit files in `src/generated/`.** All fixes, function overrides, and jump table resolutions must be applied via config files in `config/<game_id>/` or overrides/stubs in `src/runtime/`.
2. **NEVER modify, move, or delete original ROM files.**
3. **No raw assembly dumps in chat.** To search for patterns, functions, or jump tables in binaries, write and run small Python scripts in `tools/` (e.g., using `capstone`).
4. **Mandatory build verification loop.** After any change to `CMakeLists.txt`, `src/runtime/`, or `config/`, always run the build command and verify there are no compilation errors before concluding your turn.

## Core Commands

- **Configure CMake:** `cmake -B build -G Ninja` (fallback to `cmake -B build` if Ninja is unavailable)
- **Build:** `cmake --build build`
- **Run smoke test:** `./build/ramma_recomp` (or `.\build\ramma_recomp.exe` on Windows)

## Coding Conventions

- Use C++20 for `src/runtime/`.
- Keep OS/system call HLE stubs modular and separate from the entry point (`main.cpp`).
- Always reply to the user in the same language they use in chat.
