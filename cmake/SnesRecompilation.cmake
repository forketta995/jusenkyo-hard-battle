function(target_add_snes_recompilation target game_id rom_path rom_selection)
    find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
    set(lifter "${PROJECT_SOURCE_DIR}/tools/recompile_65816.py")
    set(config "${PROJECT_SOURCE_DIR}/config/${game_id}/rom.toml")
    set(generated "${CMAKE_CURRENT_BINARY_DIR}/src/generated/${game_id}/reset.c")

    # Depend on the selected path as well as the ROM's timestamp.
    file(GENERATE OUTPUT "${rom_selection}" CONTENT "${rom_path}\n")
    add_custom_command(
        OUTPUT "${generated}"
        COMMAND "${Python3_EXECUTABLE}" "${lifter}"
            --config "${config}"
            --rom "${rom_path}"
            --build-dir "${CMAKE_CURRENT_BINARY_DIR}"
            --output "${generated}"
        DEPENDS "${lifter}" "${config}" "${rom_path}" "${rom_selection}"
        COMMENT "Recompiling the ${game_id} reset path"
        VERBATIM
    )
    target_sources(${target} PRIVATE "${generated}")
endfunction()

