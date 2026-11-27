add_library(ramma_spc SHARED
    SNES_SPC.cpp
    SNES_SPC_misc.cpp
    SNES_SPC_state.cpp
    SPC_DSP.cpp
)
target_include_directories(ramma_spc SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(ramma_spc PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
if(MINGW)
    target_link_options(ramma_spc PRIVATE -static-libgcc -static-libstdc++)
endif()
