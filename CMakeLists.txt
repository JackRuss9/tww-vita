cmake_minimum_required(VERSION 3.20)

# The Legend of Zelda: The Wind Waker (GZLE01) for the PS Vita.
#
# The game is the decompilation (tww/, downloaded by build.sh at a pinned
# commit, with patches/tww applied). Everything below the game, the Nintendo
# SDK's replacement (OS, DVD, GX, audio, pads, memory card), is the Super
# Mario Sunshine PC port's platform layer (sms-port/platform, with the
# sms-vita patches in patches/sms-port), built here against its own
# decompilation's SDK headers.
#
# Two builds:
#   Vita:   cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
#           (or ./build.sh)
#   PC:     cmake -B build-pc -G Ninja   (Linux / WSL, 32-bit, SDL2 window;
#           ./build-pc.sh) for quick tests on the computer
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VITASDK} AND NOT TWW_PC)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "VitaSDK toolchain")
endif()
project(tww_vita VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(SMS_PORT_SRC ${ROOT}/sms-port/src)
set(SMS_DECOMP ${ROOT}/sms-port/decomp)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g" CACHE STRING "" FORCE)

if(VITA)
  if(EXISTS "$ENV{VITASDK}/share/vita.cmake")
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  endif()
  # The GameCube compiler's enums are 4 bytes (vita/src keeps VitaSDK's).
  add_compile_options(-fno-short-enums)
  add_compile_options("SHELL:-idirafter ${ROOT}/vita/include")
  set(CMAKE_DL_LIBS "")
else()
  # The game keeps pointers in 4-byte fields: 32-bit, as the Vita.
  add_compile_options(-m32)
  add_link_options(-m32)
  # (SSE maths, as the Sunshine port's 32-bit build, not x87)
  add_compile_options(-msse2 -mfpmath=sse)
endif()
# The GameCube compiler's char is signed.
add_compile_options(-fsigned-char)

# --- The game ---------------------------------------------------------------
include(${ROOT}/cmake/tww_game.cmake)
set(SMS_OBJCOPY ${CMAKE_OBJCOPY})
if(NOT SMS_OBJCOPY)
  set(SMS_OBJCOPY objcopy)
endif()
add_library(tww_game STATIC ${TWW_CXX_SOURCES} ${TWW_C_SOURCES} ${ROOT}/port/src/tww_game_extra.cpp)
tww_game_setup(tww_game)
add_library(tww_rels STATIC ${TWW_REL_SOURCES})
tww_rels_setup(tww_rels ${SMS_OBJCOPY})
tww_gen_rel_ctors(${CMAKE_BINARY_DIR}/rel_ctors.cpp)
# The data the game's code embeds (textures, display lists), extracted from
# your disc image by build.sh (tww/build/GZLE01/include/assets/).
#   -DTWW_PLACEHOLDER_ASSETS=ON: all-zero stand-ins instead (link tests
#   without a disc image; such a build draws nothing in their place).
option(TWW_PLACEHOLDER_ASSETS "Zero-filled stand-ins for the embedded data (tests only)" OFF)
if(TWW_PLACEHOLDER_ASSETS)
  execute_process(COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/placeholder_assets.py ${TWW_DIR}
                  ${CMAKE_BINARY_DIR}/placeholder-include)
  target_include_directories(tww_game PRIVATE ${CMAKE_BINARY_DIR}/placeholder-include)
  target_include_directories(tww_rels PRIVATE ${CMAKE_BINARY_DIR}/placeholder-include)
elseif(NOT EXISTS ${TWW_DIR}/build/GZLE01/include/assets)
  message(WARNING "tww/build/GZLE01/include/assets is missing: build.sh extracts it from your disc image")
endif()
if(NOT VITA)
  target_compile_options(tww_game PRIVATE -fno-pie)
  target_compile_options(tww_rels PRIVATE -fno-pie)
endif()

# The game replaces the global operator new/delete with JKRHeap allocation;
# only the game's own calls should go there (as in the Sunshine port).
file(WRITE ${CMAKE_BINARY_DIR}/game_new_syms.txt
  "_Znwj __sms_game_new\n_Znaj __sms_game_new_array\n_ZdlPv __sms_game_delete\n_ZdaPv __sms_game_delete_array\n")
foreach(t tww_game tww_rels)
  add_custom_command(TARGET ${t} POST_BUILD
    COMMAND ${SMS_OBJCOPY} --redefine-syms=${CMAKE_BINARY_DIR}/game_new_syms.txt $<TARGET_FILE:${t}>
    COMMENT "Renaming the game's global operator new/delete")
endforeach()

# --- The graphics layer (sms-port/platform/gx) ------------------------------
set(SMS_GX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SMS_DECOMP_DIR ${SMS_DECOMP} CACHE PATH "" FORCE)
add_subdirectory(${ROOT}/sms-port/platform/gx ${CMAKE_BINARY_DIR}/sms_gx)
if(VITA)
  set_target_properties(sms_gx PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()
# GXVert.h's vertex writers become functions implemented by sms_gx.
target_include_directories(tww_game BEFORE PRIVATE ${SMS_GX_OVERRIDE_INCLUDE_DIR})
target_include_directories(tww_rels BEFORE PRIVATE ${SMS_GX_OVERRIDE_INCLUDE_DIR})

# --- The platform layer (sms-port/platform) and the program -----------------
set(SMS_DECOMP_INCLUDE_DIRS
  ${SMS_DECOMP}/include ${SMS_DECOMP}/libs/dolphin/include ${SMS_DECOMP}/libs/JSystem/include
  ${SMS_DECOMP}/libs/THPPlayer/include ${SMS_DECOMP}/libs/PowerPC_EABI_Support/include)
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
  ${ROOT}/sms-port/platform/*.c ${ROOT}/sms-port/platform/*.cpp)
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/platform/gx/")
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/platform/mods/eclipse/")
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/tests/")
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "_windows\\.cpp$|/windows_[a-z]+\\.cpp$")
# Sunshine-only parts: its HD movies, its own data's byte order, the warp
# menu, the presence (Discord) status, the mod hooks.
list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/platform/thp/|/endian/endian_game\\.cpp$|/misc/port_warp\\.cpp$|/presence/")
include(${ROOT}/port/port_sources.cmake)

add_executable(tww ${SMS_PORT_SRC}/port_main.cpp ${PLATFORM_SOURCES} ${TWW_PORT_SOURCES}
  ${CMAKE_BINARY_DIR}/rel_ctors.cpp)
set(SMS_COMMON_DEFS GEKKO TARGET_PC VERSION_GMSE01 BUILD_VERSION=2 NDEBUG=1)
target_include_directories(tww PRIVATE
  ${ROOT}/port/platform_include
  ${ROOT}/sms-port/platform ${ROOT}/sms-port/platform/mods/include ${SMS_PORT_SRC}
  ${SMS_PORT_SRC}/port_include ${SMS_DECOMP_INCLUDE_DIRS})
target_compile_definitions(tww PRIVATE ${SMS_COMMON_DEFS} SMS_AUDIO_DSP_HLE=1 TWW_PORT=1)
target_compile_options(tww PRIVATE -fno-strict-aliasing -Wno-unused-parameter -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11>)
foreach(gekko_src misc/msl_math.c mtx/mtx.cpp)
  set_source_files_properties(${ROOT}/sms-port/platform/${gekko_src} PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
endforeach()
find_package(Threads REQUIRED)
target_link_libraries(tww PRIVATE -Wl,--whole-archive tww_game tww_rels sms_gx -Wl,--no-whole-archive
  Threads::Threads m ${CMAKE_DL_LIBS})
target_link_options(tww PRIVATE "LINKER:-Map=${CMAKE_BINARY_DIR}/tww.map")
if(NOT VITA)
  target_compile_options(tww PRIVATE -fno-pie)
  target_link_options(tww PRIVATE -no-pie)
  # dsptask.c points a DSP task at the fixed low-memory address 0x80004600.
  target_link_options(tww PRIVATE -Wl,--defsym=lbl_80004600=0x80004600)
endif()

if(VITA)
  message(FATAL_ERROR "The Vita build is the next step: build the PC test version for now (./build-pc.sh)")
endif()
