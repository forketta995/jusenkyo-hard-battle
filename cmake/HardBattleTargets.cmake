find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
set(edition_finder "${PROJECT_SOURCE_DIR}/tools/hard_battle_editions.py")
file(GLOB edition_configs CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/config/hard_battle_*/rom.toml")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${RAMMA_HARD_BATTLE_ROM}" "${edition_finder}" ${edition_configs})
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${edition_finder}" "${RAMMA_HARD_BATTLE_ROM}"
    RESULT_VARIABLE edition_status
    OUTPUT_VARIABLE hard_battle_edition OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE edition_error ERROR_STRIP_TRAILING_WHITESPACE)
if(NOT edition_status EQUAL 0)
    message(FATAL_ERROR "${edition_error}")
endif()

set(hard_battle_sources "${PROJECT_SOURCE_DIR}/src/runtime/games/hard_battle")
add_library(hard_battle STATIC
    "${hard_battle_sources}/hard_battle_content.cpp"
    "${hard_battle_sources}/hard_battle_diagnostics.cpp"
    "${hard_battle_sources}/hard_battle_preview.cpp"
    "${hard_battle_sources}/hard_battle_rom.cpp"
)
target_include_directories(hard_battle PUBLIC "${hard_battle_sources}")
target_link_libraries(hard_battle PUBLIC ramma_runtime PRIVATE ramma_rom_file)
target_add_snes_recompilation(hard_battle "${hard_battle_edition}" "${RAMMA_HARD_BATTLE_ROM}"
    "${CMAKE_CURRENT_BINARY_DIR}/hard_battle_rom_path.txt")
include("${PROJECT_SOURCE_DIR}/config/${hard_battle_edition}/preview.cmake")
target_compile_definitions(hard_battle PRIVATE
    HARD_BATTLE_TITLE_CALLBACK=${hard_battle_title_callback}
    HARD_BATTLE_TITLE_COUNTDOWN=${hard_battle_title_countdown})

target_link_libraries(ramma_recomp PRIVATE hard_battle)
target_compile_definitions(ramma_recomp PRIVATE RAMMA_HARD_BATTLE)

if(WIN32)
    add_executable(hard_battle_preview WIN32 "${hard_battle_sources}/hard_battle_preview_win32.cpp")
    target_link_libraries(hard_battle_preview PRIVATE hard_battle ramma_snes_preview_win32)
    if(MINGW)
        target_link_options(hard_battle_preview PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    install(TARGETS hard_battle_preview ramma_spc
        RUNTIME DESTINATION . COMPONENT HardBattlePreview)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hard_battle_rom_path.txt"
        DESTINATION . COMPONENT HardBattlePreview)
    install(FILES "${PROJECT_SOURCE_DIR}/third_party/snes_spc/license.txt"
        DESTINATION . RENAME snes_spc-license.txt COMPONENT HardBattlePreview)
endif()

if(BUILD_TESTING AND hard_battle_edition STREQUAL "hard_battle_usa")
    add_executable(hard_battle_boot_test tests/hard_battle_boot.cpp)
    target_link_libraries(hard_battle_boot_test PRIVATE hard_battle)
    add_test(NAME hard_battle_opening_scene
        COMMAND hard_battle_boot_test "${RAMMA_HARD_BATTLE_ROM}")
    add_executable(hard_battle_audio_test tests/hard_battle_audio.cpp)
    target_link_libraries(hard_battle_audio_test PRIVATE hard_battle)
    add_test(NAME hard_battle_audio
        COMMAND hard_battle_audio_test "${RAMMA_HARD_BATTLE_ROM}")
    set(RAMMA_HARD_BATTLE_TITLE_GRAPHIC_ROM "" CACHE FILEPATH "Japan revision 0 ROM for the USA content test")
    if(RAMMA_HARD_BATTLE_TITLE_GRAPHIC_ROM)
        add_executable(hard_battle_content_test tests/hard_battle_content.cpp)
        target_link_libraries(hard_battle_content_test PRIVATE hard_battle)
        add_test(NAME hard_battle_title_graphic COMMAND hard_battle_content_test
            "${RAMMA_HARD_BATTLE_ROM}" "${RAMMA_HARD_BATTLE_TITLE_GRAPHIC_ROM}")
    endif()
elseif(BUILD_TESTING AND hard_battle_edition STREQUAL "hard_battle_jpn")
    add_executable(hard_battle_jpn_test tests/hard_battle_jpn.cpp)
    target_link_libraries(hard_battle_jpn_test PRIVATE hard_battle)
    add_test(NAME hard_battle_jpn_opening
        COMMAND hard_battle_jpn_test "${RAMMA_HARD_BATTLE_ROM}")
endif()
