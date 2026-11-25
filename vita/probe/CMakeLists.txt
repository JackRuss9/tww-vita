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

set(TWW_VITA_GAME_VERSION 01.45)
set(sw_game "${TWW_SWITCHWAKER_DIR}/game")
set(sw_native "${TWW_SWITCHWAKER_DIR}/native")
set(aurora_os "${TWW_AURORA_DIR}/lib/dolphin/os")
option(TWW_BUILD_GAME_OBJECTS "Enable ARM compilation of the SwitchWaker game modules" OFF)
option(TWW_BUILD_RUNTIME_DVD "Build Aurora's runtime DVD service on the Vita CISO reader" OFF)
set(TWW_GXM_AURORA_DIR "" CACHE PATH "Aurora GXM checkout used to build against its GX ABI")
set(TWW_GXM_BUILD_DIR "" CACHE PATH "Built static GXM Aurora and dependency archives")
set(TWW_VITASHARK_PREFIX "" CACHE PATH "VitaShaRK dependency prefix (optional if in VitaSDK)")

if(TWW_BUILD_RUNTIME_DVD)
  set(tww_nod "${TWW_SWITCHWAKER_DIR}/switch/native/nod")
  add_library(tww_vita_dvd STATIC
    disc.c vita_dvd_platform.cpp
    "${tww_nod}/nod_gcn.cpp"
    "${TWW_AURORA_DIR}/lib/dolphin/dvd/dvd.cpp"
    "${TWW_AURORA_DIR}/lib/dolphin/dvd/fst.cpp")
  target_compile_features(tww_vita_dvd PRIVATE c_std_11 cxx_std_20)
  target_compile_definitions(tww_vita_dvd PRIVATE TWW_VITA_DVD=1 TARGET_PC=1)
  target_include_directories(tww_vita_dvd PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${tww_nod}" "${TWW_AURORA_DIR}/include" "${sw_native}/include/sdk")
  target_compile_options(tww_vita_dvd PRIVATE "SHELL:-include '${sw_native}/include/sdk/cos_sdk_extras.h'")
  target_link_libraries(tww_vita_dvd PUBLIC pthread SceIofilemgr_stub)
endif()

add_library(tww_game_headers INTERFACE)
target_compile_features(tww_game_headers INTERFACE c_std_11 cxx_std_20)
target_compile_definitions(tww_game_headers INTERFACE
  TARGET_PC=1 TWW_VITA_BOOT=1 MTX_USE_PS=1 VERSION=2 NDEBUG=1)
target_compile_options(tww_game_headers INTERFACE
  -fsigned-char -fno-strict-aliasing -ffunction-sections -fdata-sections
  -ftrivial-auto-var-init=zero -fno-lifetime-dse -Wno-multichar
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fcheck-new;-Wno-write-strings;-Wno-narrowing;-Wno-changes-meaning>
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include '${sw_native}/include/pc/cos_pc_config.h'>"
  "SHELL:-include '${sw_native}/include/sdk/cos_sdk_extras.h'")
target_include_directories(tww_game_headers INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}" "${sw_native}/include" "${sw_native}/include/sdk"
  "${TWW_AURORA_DIR}/include" "${sw_game}/include" "${sw_game}/src"
  "${TWW_SWITCHWAKER_ASSETS}/include" "${TWW_SWITCHWAKER_ASSETS}"
  "${sw_native}/include/pc/msl" "${sw_native}/sdk/include")

option(TWW_VITA_PROFILE "Profiling build: -finstrument-functions and the sampling profiler (slower)" OFF)
if(TWW_VITA_PROFILE)
  # Standard library headers stay uninstrumented; with them every iterator
  # and accessor call would pay for the hooks.
  target_compile_options(tww_game_headers INTERFACE -finstrument-functions
    "-finstrument-functions-exclude-file-list=include/c++,/bits/")
  target_compile_definitions(tww_game_headers INTERFACE TWW_VITA_PROFILE=1)
endif()

if(TWW_GXM_AURORA_DIR)
  # Both sides of GX must share the texture and palette object layouts.
  set(gxm_abi_headers "${CMAKE_CURRENT_BINARY_DIR}/gxm-abi")
  configure_file("${TWW_GXM_AURORA_DIR}/include/dolphin/gx/GXStruct.h"
    "${gxm_abi_headers}/dolphin/gx/GXStruct.h" COPYONLY)
  target_include_directories(tww_game_headers BEFORE INTERFACE "${gxm_abi_headers}")
  target_compile_options(tww_game_headers INTERFACE -fno-short-enums)
  add_library(tww_vita_gxm_frames STATIC vita_gxm_frames.cpp)
  target_compile_features(tww_vita_gxm_frames PRIVATE cxx_std_20)
  target_compile_options(tww_vita_gxm_frames PRIVATE -fno-short-enums)
  target_include_directories(tww_vita_gxm_frames PRIVATE
    "${TWW_GXM_AURORA_DIR}/include" "${sw_native}/include")
  if(TWW_BUILD_RUNTIME_DVD)
    add_library(tww_vita_game_entry STATIC vita_game_entry.cpp)
    target_compile_definitions(tww_vita_game_entry PRIVATE TWW_VITA_GAME_VERSION="${TWW_VITA_GAME_VERSION}")
    target_link_libraries(tww_vita_game_entry PRIVATE tww_game_headers switchwaker_assets tww_vita_dvd)
    target_include_directories(tww_vita_game_entry PRIVATE
      "${sw_native}/src/pc" "${tww_nod}")
  endif()
endif()

if(TWW_BUILD_GAME_OBJECTS)
  add_library(tww_vita_card STATIC vita_card.cpp)
  target_include_directories(tww_vita_card PRIVATE "${sw_native}/src/pc" "${sw_native}/include")
  target_link_libraries(tww_vita_card PRIVATE tww_game_headers)
  file(GLOB_RECURSE runtime_sdk_sources CONFIGURE_DEPENDS
    "${sw_native}/sdk/src/*.c" "${sw_native}/sdk/src/*.cpp")
  # This boot build uses the muted Vita audio adapter.
  list(FILTER runtime_sdk_sources EXCLUDE REGEX "/sdk/src/audio/")
  add_library(tww_vita_sdk STATIC ${runtime_sdk_sources})
  target_sources(tww_vita_sdk PRIVATE
    vita_pad.cpp vita_menu.cpp vita_runtime_hooks.cpp vita_muted_audio.cpp
    $<$<BOOL:${TWW_VITA_PROFILE}>:${CMAKE_CURRENT_SOURCE_DIR}/vita_prof.cpp>
    "${aurora_os}/OSCache.cpp"
    "${sw_game}/src/dolphin/pad/Padclamp.c"
    "${sw_game}/src/dolphin/gf/GFGeometry.cpp"
    "${sw_game}/src/dolphin/gf/GFLight.cpp"
    "${sw_game}/src/dolphin/gf/GFPixel.cpp"
    "${sw_game}/src/dolphin/gf/GFTev.cpp"
    "${sw_game}/src/dolphin/gf/GFTransform.cpp"
    "${sw_native}/src/helpers/offset_ptr.cpp"
    "${sw_native}/src/pc/game_hooks/pc_j3d_hooks.cpp"
    "${sw_native}/src/pc/game_hooks/pc_j3d_transform.cpp"
    "${sw_native}/src/pc/game_hooks/pc_jkr_heap.cpp"
    "${sw_native}/src/pc/game_hooks/pc_jkr_hooks.cpp"
    "${sw_native}/src/pc/game_hooks/pc_audio_hooks.cpp"
    "${sw_native}/src/pc/game_hooks/pc_c_bg_s.cpp"
    "${sw_native}/src/pc/game_hooks/pc_jfw_display.cpp"
    "${sw_native}/src/pc/game_hooks/pc_meter_hooks.cpp"
    "${sw_native}/src/pc/game_hooks/pc_m_do_lib.cpp"
    "${sw_native}/src/pc/features/pc_aspect.cpp"
    "${sw_native}/src/pc/features/pc_controls.cpp"
    "${sw_native}/src/pc/features/pc_qol.cpp")
  target_include_directories(tww_vita_sdk PRIVATE "${sw_native}/src/pc")
  set_source_files_properties(vita_prof.cpp PROPERTIES COMPILE_OPTIONS -fno-instrument-functions)
  target_link_libraries(tww_vita_sdk PRIVATE tww_game_headers PUBLIC pthread)
  target_compile_definitions(tww_vita_sdk PRIVATE
    TWW_VITA_RUNTIME_SDK=1
    COS_AURORA_COMMIT_STR="3227d76c60e1e782ca576610bce61c9e7744d8be")
  if(TWW_GXM_AURORA_DIR)
    target_compile_definitions(tww_vita_sdk PRIVATE TWW_VITA_GXM=1)
  endif()
  set(COS_ROOT "${sw_game}")
  add_library(cos_game_headers ALIAS tww_game_headers)
  include("${sw_native}/cmake/deferred.cmake")
  include("${sw_native}/cmake/modules.cmake")

  # Resolve calls between boot modules before reporting the platform boundaries.
  # A relocatable link does not require the host services or the graphics backend.
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-boot-arm.o"
    COMMAND "${CMAKE_LINKER}" -r -o "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-boot-arm.o"
      $<TARGET_OBJECTS:m_Do> $<TARGET_OBJECTS:framework> $<TARGET_OBJECTS:JSystem-core>
    DEPENDS m_Do framework JSystem-core
      $<TARGET_OBJECTS:m_Do> $<TARGET_OBJECTS:framework> $<TARGET_OBJECTS:JSystem-core>
    COMMAND_EXPAND_LISTS VERBATIM)
  add_custom_target(switchwaker_boot_link_check
    COMMAND "${CMAKE_NM}" -Cu "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-boot-arm.o"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-boot-arm.o"
    VERBATIM)

  set(game_objects)
  foreach(module IN LISTS COS_MODULES)
    if(TARGET "${module}")
      list(APPEND game_objects "$<TARGET_OBJECTS:${module}>")
    endif()
  endforeach()
  set(game_response "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-objects.rsp")
  set(game_bundle "${CMAKE_CURRENT_BINARY_DIR}/switchwaker-arm.o")
  file(GENERATE OUTPUT "${game_response}"
    CONTENT "\"$<JOIN:${game_objects},\"\n\">\"\n")
  add_custom_command(OUTPUT "${game_bundle}"
    COMMAND "${CMAKE_LINKER}" -r -o "${game_bundle}" "@${game_response}"
    DEPENDS cos_modules "${game_response}" ${game_objects}
    COMMAND_EXPAND_LISTS VERBATIM)
  add_custom_target(switchwaker_all_link_check
    COMMAND "${CMAKE_NM}" -Cu "${game_bundle}"
    DEPENDS "${game_bundle}" VERBATIM)
endif()

add_library(switchwaker_boot_system STATIC
  boot_system.cpp
  vita_pad.cpp
  "${aurora_os}/OSInit.cpp"
  "${aurora_os}/OSArena.cpp"
  "${aurora_os}/OSAddress.cpp"
  "${aurora_os}/OSBootInfo.cpp"
  "${aurora_os}/OSAlloc.cpp"
  "${TWW_AURORA_DIR}/lib/dolphin/AR.cpp"
  "${sw_native}/sdk/src/os/OSThread.cpp"
  "${sw_native}/sdk/src/os/OSInterrupt.cpp"
  "${sw_native}/sdk/src/os/OSMutex.cpp"
  "${sw_native}/sdk/src/os/OSMessage.cpp"
  "${sw_native}/sdk/src/os/OSAlarm.cpp"
  "${sw_native}/sdk/src/os/OSContext.cpp"
  "${sw_game}/src/m_Do/m_Do_machine.cpp"
  "${sw_game}/src/m_Do/m_Do_main.cpp"
  "${sw_game}/src/m_Do/m_Do_ext.cpp"
  "${sw_game}/src/JSystem/JFramework/JFWSystem.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRHeap.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRExpHeap.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRDisposer.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRDvdFile.cpp"
  "${sw_game}/src/JSystem/JKernel/JKRDecomp.cpp"
  "${sw_game}/src/JSystem/JUtility/JUTGamePad.cpp"
  "${sw_game}/src/dolphin/pad/Padclamp.c"
  "${sw_game}/src/JSystem/JSupport/JSUList.cpp")
target_link_libraries(switchwaker_boot_system PRIVATE tww_game_headers)
target_compile_definitions(switchwaker_boot_system PRIVATE TWW_VITA_EARLY_BOOT=1)

add_executable(tww-vita-probe main.c disc.c bti.c switchwaker_boot.cpp gpu.c gx_probe.cpp)
target_compile_features(tww-vita-probe PRIVATE c_std_11 cxx_std_20)
target_compile_options(tww-vita-probe PRIVATE -Wall -Wextra -Wformat=2)
target_link_options(tww-vita-probe PRIVATE -Wl,--gc-sections)
target_link_libraries(tww-vita-probe PRIVATE switchwaker_assets switchwaker_boot_system pthread SceCtrl_stub SceDisplay_stub SceGxm_stub SceIofilemgr_stub)

# VitaSDK's packaging macros mutate their flags in the caller's scope.
function(tww_package_vpk executable version title)
  set(options UNSAFE)
  set(multiValueArgs FILE)
  cmake_parse_arguments(pack "${options}" "" "${multiValueArgs}" ${ARGN})
  if(pack_UNSAFE)
    set(self_options UNSAFE)
  endif()
  vita_create_self("${executable}.self" "${executable}" ${self_options})
  vita_create_vpk("${executable}.vpk" TWWP00001 "${executable}.self"
    VERSION "${version}" NAME "${title}" FILE ${pack_FILE})
endfunction()

tww_package_vpk(tww-vita-probe 01.15 "TWW Vita Probe"
  FILE "${TWW_SWITCHWAKER_DIR}/LICENSE" switchwaker-LICENSE.txt
  FILE "${TWW_AURORA_DIR}/LICENSE" aurora-LICENSE.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/libvita2d-LICENSE.txt" libvita2d-LICENSE.txt)

if(TWW_GXM_BUILD_DIR)
  if(NOT TWW_BUILD_GAME_OBJECTS OR NOT TARGET tww_vita_game_entry)
    message(FATAL_ERROR "The game VPK needs game objects, GXM headers and runtime DVD")
  endif()

  set(renderer_archives
    libaurora_gx.a libaurora_gd.a libaurora_mtx.a libaurora_os.a
    libaurora_vi.a libaurora_core.a libaurora_ms.a
    extern/libimgui.a
    _deps/fmt-build/libfmt.a _deps/freetype-build/libfreetype.a
    _deps/png-build/liblibpng16_static.a _deps/sdl-build/libSDL3.a
    _deps/xxhash-build/libxxhash.a _deps/zlib-build/libz.a
    _deps/zstd-build/lib/libzstd.a)
  list(TRANSFORM renderer_archives PREPEND "${TWW_GXM_BUILD_DIR}/")
  foreach(archive IN LISTS renderer_archives)
    if(NOT EXISTS "${archive}")
      message(FATAL_ERROR "Build the GXM renderer archives first. Missing ${archive}")
    endif()
  endforeach()

  set_source_files_properties("${game_bundle}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(switchwaker-vita "${game_bundle}")
  set_target_properties(switchwaker-vita PROPERTIES LINKER_LANGUAGE CXX)
  # vita-elf-create adds SCE import metadata between the executable and data segments.
  target_link_options(switchwaker-vita PRIVATE "-Wl,--defsym=__sce_headroom=0x4000")
  # libstdc++ references these weakly. An archive does not load a member for
  # a weak reference, so force the Vita pthread implementation into the SELF.
  set(vita_pthread_symbols
    pthread_cancel pthread_once pthread_cond_wait pthread_cond_timedwait
    pthread_cond_signal pthread_cond_broadcast pthread_cond_destroy)
  list(TRANSFORM vita_pthread_symbols PREPEND "-Wl,--undefined=")
  target_link_options(switchwaker-vita PRIVATE ${vita_pthread_symbols})
  if(TWW_VITASHARK_PREFIX)
    target_link_directories(switchwaker-vita PRIVATE "${TWW_VITASHARK_PREFIX}/lib")
  endif()
  target_link_libraries(switchwaker-vita PRIVATE
    -Wl,--start-group
    tww_vita_game_entry switchwaker_assets tww_vita_gxm_frames tww_vita_dvd
    tww_vita_sdk tww_vita_card ${renderer_archives}
    vitashark SceShaccCgExt SceShaccCg_stub_weak taihen_stub pthread
    SceGxm_stub SceDisplay_stub SceCtrl_stub SceAppMgr_stub SceAppUtil_stub
    SceAudio_stub SceAudioIn_stub SceSysmodule_stub SceIofilemgr_stub
    SceCommonDialog_stub SceTouch_stub SceHid_stub SceMotion_stub ScePower_stub
    SceProcessmgr_stub SceCamera_stub SceKernelModulemgr_stub
    -Wl,--end-group)

  tww_package_vpk(switchwaker-vita "${TWW_VITA_GAME_VERSION}" "SwitchWaker Vita" UNSAFE
    FILE "${TWW_SWITCHWAKER_DIR}/LICENSE" switchwaker-LICENSE.txt
    FILE "${TWW_GXM_AURORA_DIR}/LICENSE" aurora-LICENSE.txt
    FILE "${TWW_GXM_AURORA_DIR}/../LICENSE" mh3-wii-LICENSE.txt)
endif()
