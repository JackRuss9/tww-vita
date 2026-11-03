cmake_minimum_required(VERSION 3.20)
# Fast compile check of the Wind Waker game code on the host (gcc -m32):
#   cmake -S check -B build-check -G Ninja && ninja -C build-check -k 0
# Only compiles; it links nothing (the platform layer is the Vita build's).
project(tww_check C CXX)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
include(${ROOT}/cmake/tww_game.cmake)
add_compile_options(-m32)
add_library(tww_check OBJECT ${TWW_CXX_SOURCES} ${TWW_C_SOURCES} ${TWW_REL_SOURCES})
tww_game_setup(tww_check)
# Zero-filled stand-ins for the data the real build extracts from the disc
# (tools/placeholder_assets.py; check only).
execute_process(COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/placeholder_assets.py ${TWW_DIR}
                ${CMAKE_BINARY_DIR}/placeholder-include)
target_include_directories(tww_check PRIVATE ${CMAKE_BINARY_DIR}/placeholder-include)
