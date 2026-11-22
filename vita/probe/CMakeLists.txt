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

add_executable(tww-vita-probe main.c gpu.c gx_probe.cpp)
target_compile_features(tww-vita-probe PRIVATE c_std_11 cxx_std_20)
target_compile_options(tww-vita-probe PRIVATE -Wall -Wextra -Wformat=2)
target_link_libraries(tww-vita-probe PRIVATE SceCtrl_stub SceDisplay_stub SceGxm_stub SceIofilemgr_stub)

vita_create_self(tww-vita-probe.self tww-vita-probe)
vita_create_vpk(tww-vita-probe.vpk TWWP00001 tww-vita-probe.self
  VERSION 01.05
  NAME "TWW Vita Probe"
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/libvita2d-LICENSE.txt" libvita2d-LICENSE.txt
  FILE "${CMAKE_CURRENT_SOURCE_DIR}/aurora-LICENSE.txt" aurora-LICENSE.txt)
