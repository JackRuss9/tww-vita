cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "Set VITASDK to the VitaSDK installation directory")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE FILEPATH "VitaSDK toolchain")
endif()

project(tww_vita_probe LANGUAGES C CXX)
if(NOT VITA)
  message(FATAL_ERROR "This target requires the VitaSDK toolchain")
endif()
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(TWW_SWITCHWAKER_DIR "" CACHE PATH "SwitchWaker checkout")
set(TWW_AURORA_DIR "" CACHE PATH "Pinned Aurora checkout")
set(TWW_SWITCHWAKER_ASSETS "" CACHE PATH "Generated SwitchWaker USA runtime asset stubs")
foreach(input IN ITEMS
  "${TWW_SWITCHWAKER_DIR}/native/src/pc/runtime/pc_assets.cpp"
  "${TWW_AURORA_DIR}/include/dolphin/dvd.h"
  "${TWW_SWITCHWAKER_ASSETS}/include/assets/cos_assets.h")
  if(NOT EXISTS "${input}")
    message(FATAL_ERROR "Missing ${input}. Set the source and stub paths described in vita/README.md.")
  endif()
endforeach()
file(STRINGS "${TWW_SWITCHWAKER_ASSETS}/include/assets/cos_assets.h" tww_usa_assets
  REGEX "^#define COS_ASSETS_GAME_ID \"GZLE01\"$")
if(NOT tww_usa_assets)
  message(FATAL_ERROR "Build the probe with SwitchWaker runtime asset stubs for GZLE01")
endif()

add_library(switchwaker_assets STATIC "${TWW_SWITCHWAKER_DIR}/native/src/pc/runtime/pc_assets.cpp")
target_compile_features(switchwaker_assets PRIVATE cxx_std_20)
target_compile_definitions(switchwaker_assets PUBLIC TARGET_PC=1 PRIVATE COS_RUNTIME_ASSETS=1 TWW_VITA_BOOT=1)
target_compile_options(switchwaker_assets PRIVATE -include dolphin/types.h)
target_include_directories(switchwaker_assets PUBLIC
  "${TWW_SWITCHWAKER_DIR}/native/src/pc"
  "${TWW_SWITCHWAKER_DIR}/native/include"
  "${TWW_SWITCHWAKER_DIR}/native/sdk/include"
  "${TWW_SWITCHWAKER_ASSETS}/include"
  "${TWW_AURORA_DIR}/include")

set(sw_game "${TWW_SWITCHWAKER_DIR}/game")
set(sw_native "${TWW_SWITCHWAKER_DIR}/native")
set(aurora_os "${TWW_AURORA_DIR}/lib/dolphin/os")
add_library(switchwaker_boot_system STATIC
  boot_system.cpp
  "${aurora_os}/OSInit.cpp"
  "${aurora_os}/OSArena.cpp"
  "${aurora_os}/OSAddress.cpp"
  "${aurora_os}/OSBootInfo.cpp"
  "${aurora_os}/OSAlloc.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRHeap.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRExpHeap.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRDisposer.cpp"
  "${sw_game}/src/JSystem/JSupport/JSUList.cpp")
target_link_libraries(switchwaker_boot_system PRIVATE switchwaker_assets)
target_compile_features(switchwaker_boot_system PRIVATE cxx_std_20)
target_compile_definitions(switchwaker_boot_system PRIVATE TWW_VITA_BOOT=1 MTX_USE_PS=1 VERSION=2 NDEBUG=1)
target_compile_options(switchwaker_boot_system PRIVATE
  -fno-exceptions -fno-rtti -fsigned-char -fno-strict-aliasing -fcheck-new
  -Wno-multichar -Wno-write-strings
  "SHELL:-include '${sw_native}/include/pc/cos_pc_config.h'"
  "SHELL:-include '${sw_native}/include/sdk/cos_sdk_extras.h'")
target_include_directories(switchwaker_boot_system PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${sw_native}/include" "${sw_native}/include/sdk"
  "${TWW_AURORA_DIR}/include" "${sw_game}/include" "${TWW_SWITCHWAKER_ASSETS}"
  "${sw_native}/include/pc/msl")

add_executable(tww-vita-probe main.c disc.c bti.c switchwaker_boot.cpp gpu.c gx_probe.cpp)
target_compile_features(tww-vita-probe PRIVATE c_std_11 cxx_std_20)
target_compile_options(tww-vita-probe PRIVATE -Wall -Wextra -Wformat=2)
target_link_libraries(tww-vita-probe PRIVATE switchwaker_assets switchwaker_boot_system pthread SceCtrl_stub SceDisplay_stub SceGxm_stub SceIofilemgr_stub)

vita_create_self(tww-vita-probe.self tww-vita-probe)
vita_create_vpk(tww-vita-probe.vpk TWWP00001 tww-vita-probe.self
  VERSION 01.10
  NAME "TWW Vita Probe"
  FILE "${TWW_SWITCHWAKER_DIR}/LICENSE" switchwaker-LICENSE.txt
  FILE "${TWW_AURORA_DIR}/LICENSE" aurora-LICENSE.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/libvita2d-LICENSE.txt" libvita2d-LICENSE.txt)
