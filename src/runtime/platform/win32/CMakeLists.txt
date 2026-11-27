add_library(ramma_snes_preview_win32 STATIC snes_preview.cpp wave_output.cpp)
target_include_directories(ramma_snes_preview_win32 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ramma_snes_preview_win32 PUBLIC ramma_runtime PRIVATE gdi32 user32 winmm)
