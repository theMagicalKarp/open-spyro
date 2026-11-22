# native/tests/CMakeLists.txt — unit tests for the native host code.
#
#   make native-test   configure + build into build/native-test/, run ctest
#
# A project of its own: it needs only a host C compiler and the tomlc17
# submodule, none of the wasm toolchain or SDL3 that native/ does. It covers
# the native units with neither dependency (docs/native.md lists them), plus
# host/psx_memory.c when the wasm2c runtime is present.

cmake_minimum_required(VERSION 3.21)
project(open_spyro_native_tests C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(HOST ${ROOT}/src/native/host)
set(TOMLC17 ${ROOT}/external/tomlc17/src)
if(NOT EXISTS ${TOMLC17}/tomlc17.c)
  message(FATAL_ERROR "tomlc17 missing: git submodule update --init external/tomlc17")
endif()

set(WASM ${ROOT}/src/native/wasm)
set(HANDASM ${ROOT}/src/native/handasm)
set(GAME ${ROOT}/src/native/game)

# A unit's tests live next to it as <unit>_test.c (src/native/host/config_test.c
# tests config.c); every one is picked up here. The wasm-side units' tests
# include the unit itself (libc.c renames libc, memcard.c has static state),
# so only the host and hand-asm units are listed as sources. The native game
# replacements' tests (src/native/game/<subsystem>/) include their unit too,
# and see the game headers in include/.
file(GLOB unit_tests CONFIGURE_DEPENDS
    ${HOST}/*_test.c ${WASM}/*_test.c ${HANDASM}/*_test.c)
file(GLOB game_unit_tests CONFIGURE_DEPENDS ${GAME}/*/*_test.c)
list(APPEND unit_tests ${game_unit_tests})

# psx_memory_test.c needs the wasm2c runtime (tools/native_setup.sh); without
# it, it is left out.
list(FILTER unit_tests EXCLUDE REGEX "/psx_memory_test\\.c$")
set(WASM2C_RT ${ROOT}/build/native-tools/wasm2c-runtime CACHE PATH "wasm2c runtime sources")
set(psx_memory_sources)
if(EXISTS ${WASM2C_RT}/wasm-rt-mem-impl.c)
  set(psx_memory_sources
      ${HOST}/psx_memory_test.c ${HOST}/psx_memory.c
      ${WASM2C_RT}/wasm-rt-impl.c ${WASM2C_RT}/wasm-rt-mem-impl.c)
else()
  message(STATUS "wasm2c runtime not found: psx_memory tests skipped")
endif()

# The hand-asm leaves runtime_test.c runs: generated, register-only clusters.
# A cluster file is named for its head, so each leaf's file is found by the
# C-ABI wrapper it defines; a regeneration that regroups clusters still builds.
set(handasm_entries AbsAngleDelta8 AbsAngleDelta12 SignedAngleDelta8
    SignedAngleDelta12 ApproachAngle8 CountLeadingZeros)
list(JOIN handasm_entries "|" handasm_alt)
file(GLOB handasm_clusters CONFIGURE_DEPENDS ${HANDASM}/ha_*.c)
set(handasm_leaves)
set(handasm_found)
foreach(cluster IN LISTS handasm_clusters)
  file(STRINGS ${cluster} defs REGEX "^[A-Za-z_][A-Za-z0-9_ *]* (${handasm_alt})\\(")
  foreach(def IN LISTS defs)
    string(REGEX MATCH " (${handasm_alt})\\(" m "${def}")
    list(APPEND handasm_found ${CMAKE_MATCH_1})
    list(APPEND handasm_leaves ${cluster})
  endforeach()
endforeach()
list(REMOVE_DUPLICATES handasm_leaves)
foreach(entry IN LISTS handasm_entries)
  if(NOT entry IN_LIST handasm_found)
    message(FATAL_ERROR "hand-asm entry ${entry} not found in ${HANDASM}/ha_*.c")
  endif()
endforeach()

add_executable(native_tests
    test_main.c
    ${unit_tests}
    ${HOST}/config.c
    ${HOST}/diag_stats.c
    ${HOST}/host_io.c
    ${HOST}/script.c
    ${HOST}/stick.c
    ${HOST}/audio_format.c
    ${WASM}/shim_helpers.c
    ${HANDASM}/runtime.c
    ${psx_memory_sources}
    ${handasm_leaves}
    ${TOMLC17}/tomlc17.c)
target_include_directories(native_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${HOST} ${WASM} ${HANDASM} ${TOMLC17})
if(psx_memory_sources)
  target_include_directories(native_tests PRIVATE ${WASM2C_RT})
  set_source_files_properties(${WASM2C_RT}/wasm-rt-impl.c
      ${WASM2C_RT}/wasm-rt-mem-impl.c PROPERTIES COMPILE_OPTIONS "-w")
endif()
target_compile_definitions(native_tests PRIVATE
    "TEST_TMPDIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")
target_compile_options(native_tests PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(native_tests PRIVATE -fsanitize=address,undefined)
target_link_libraries(native_tests PRIVATE m)
set_target_properties(native_tests PROPERTIES C_STANDARD 11)
set_source_files_properties(${TOMLC17}/tomlc17.c ${handasm_leaves} PROPERTIES COMPILE_OPTIONS "-w")
# The game units' wasm export_name attributes mean nothing on the host.
set_source_files_properties(${game_unit_tests} PROPERTIES
    INCLUDE_DIRECTORIES ${ROOT}/include COMPILE_OPTIONS "-Wno-attributes")

enable_testing()
add_test(NAME native_tests COMMAND native_tests)
# The settings keep their strings for the whole run, by design.
set_tests_properties(native_tests PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
