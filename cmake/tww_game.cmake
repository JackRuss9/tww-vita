# The Wind Waker game code (tww/, the decomp, with patches/tww applied):
# its sources and how they are compiled, shared by the Vita build and the
# host compile check (check/).
#
# Needs ROOT (this project's directory) set.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TWW_DIR ${ROOT}/tww)
set(SMS_PORT_DIR ${ROOT}/sms-port)
execute_process(
  COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/gen_sources.py ${TWW_DIR}
          ${CMAKE_BINARY_DIR}/tww_sources.cmake ${CMAKE_BINARY_DIR}/tww_stubs.txt
  RESULT_VARIABLE _gen_rc)
if(NOT _gen_rc EQUAL 0)
  message(FATAL_ERROR "tools/gen_sources.py failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${TWW_DIR}/configure.py)
include(${CMAKE_BINARY_DIR}/tww_sources.cmake)
# The four .c files (JAudio's DSP code) were built as C++ too: their
# symbols are C++ names.
set_source_files_properties(${TWW_CXX_SOURCES} ${TWW_C_SOURCES} ${TWW_REL_SOURCES} PROPERTIES LANGUAGE CXX)

# GZLE01 (USA, Rev 0): VERSION_USA in tww/include/global.h
set(TWW_DEFS GEKKO TARGET_PC VERSION=2 NDEBUG=1)
set(TWW_FLAGS
  -w -fno-strict-aliasing -fwrapv -fsigned-char
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector
  -ffp-contract=off
  $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++11 -Wno-narrowing -fno-gnu-keywords -fpermissive -fno-threadsafe-statics -fno-rtti -fno-exceptions -fcheck-new -fno-lifetime-dse>
  $<$<COMPILE_LANGUAGE:C>:-std=gnu99 -fcommon>
  -include ${ROOT}/port/include/tww_compat.h)
# port/include first (its dolphin/types.h replaces the decomp's), then the
# Sunshine port's headers (port_compat.h and what it includes), then the
# decomp's own.
set(TWW_INCLUDE_DIRS
  ${ROOT}/port/include
  ${SMS_PORT_DIR}/src
  ${TWW_DIR}/include
  ${TWW_DIR}/src
  ${TWW_DIR}/build/GZLE01/include
  ${TWW_DIR}/assets/GZLE01)

function(tww_game_setup target)
  target_include_directories(${target} BEFORE PRIVATE ${TWW_INCLUDE_DIRS})
  target_compile_definitions(${target} PRIVATE ${TWW_DEFS})
  target_compile_options(${target} PRIVATE ${TWW_FLAGS})
endfunction()

# The actor modules, a library of their own: each one's static
# constructors are moved to a section named after it (tools/rel_cc.py) and
# run when the game links the module (rel_ctors.cpp).
function(tww_rels_setup target objcopy)
  tww_game_setup(${target})
  set_target_properties(${target} PROPERTIES CXX_COMPILER_LAUNCHER
    "${Python3_EXECUTABLE};${ROOT}/tools/rel_cc.py;${objcopy}")
endfunction()
function(tww_gen_rel_ctors out)
  add_custom_command(OUTPUT ${out}
    COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/gen_rel_ctors.py ${out} ${TWW_REL_NAMES}
    DEPENDS ${ROOT}/tools/gen_rel_ctors.py ${CMAKE_BINARY_DIR}/tww_sources.cmake
    COMMENT "Listing the actor modules' constructors")
endfunction()
