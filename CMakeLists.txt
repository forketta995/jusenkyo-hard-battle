cmake_minimum_required(VERSION 3.20)
project(ramma-games-recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Export compile_commands.json (useful for IDEs and static analysis)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third_party/snes_spc EXCLUDE_FROM_ALL)

add_library(ramma_runtime STATIC
    src/runtime/snes/cpu_65816.cpp
    src/runtime/snes/cpu_65816_control.cpp
    src/runtime/snes/snes_bus.cpp
    src/runtime/snes/snes_hdma.cpp
    src/runtime/snes/snes_apu.cpp
    src/runtime/snes/snes_audio.cpp
    src/runtime/snes/snes_ppu.cpp
    src/runtime/snes/snes_frame.cpp
    src/runtime/snes/snes_objects.cpp
    src/runtime/snes/spc700_boot.cpp
)
target_link_libraries(ramma_runtime PRIVATE ramma_spc)

add_library(ramma_rom_file STATIC src/runtime/rom_file.cpp)
target_include_directories(ramma_rom_file PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime)

add_executable(ramma_recomp src/runtime/main.cpp)

target_include_directories(ramma_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/snes
)

if(MSVC)
    target_compile_options(ramma_runtime PRIVATE /W4)
    target_compile_options(ramma_recomp PRIVATE /W4)
else()
    target_compile_options(ramma_runtime PRIVATE -Wall -Wextra)
    target_compile_options(ramma_recomp PRIVATE -Wall -Wextra)
endif()

include(CTest)
include(cmake/SnesRecompilation.cmake)
if(WIN32)
    add_subdirectory(src/runtime/platform/win32 EXCLUDE_FROM_ALL)
else()
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        add_subdirectory(src/runtime/platform/sdl3 EXCLUDE_FROM_ALL)
    else()
        message(STATUS "SDL3 not found: the interactive preview will not be built")
    endif()
endif()

set(RAMMA_HARD_BATTLE_ROM "" CACHE FILEPATH "Local Hard Battle ROM")
if(RAMMA_HARD_BATTLE_ROM)
    include(cmake/HardBattleTargets.cmake)
endif()
