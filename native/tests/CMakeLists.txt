# native/tests/CMakeLists.txt — unit tests for the native host code.
#
#   make native-test   configure + build into build/native-test/, run ctest
#
# A project of its own: it needs only a host C compiler and the tomlc17
# submodule, none of the wasm toolchain or SDL3 that native/ does.

cmake_minimum_required(VERSION 3.21)
project(open_spyro_native_tests C)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(HOST ${ROOT}/src/native/host)
set(TOMLC17 ${ROOT}/external/tomlc17/src)
if(NOT EXISTS ${TOMLC17}/tomlc17.c)
  message(FATAL_ERROR "tomlc17 missing: git submodule update --init external/tomlc17")
endif()

# A unit's tests live next to it as <unit>_test.c (src/native/host/config_test.c
# tests config.c); every one is picked up here.
file(GLOB unit_tests CONFIGURE_DEPENDS ${HOST}/*_test.c)

add_executable(native_tests
    test_main.c
    ${unit_tests}
    ${HOST}/config.c
    ${TOMLC17}/tomlc17.c)
target_include_directories(native_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${HOST} ${TOMLC17})
target_compile_definitions(native_tests PRIVATE
    "TEST_TMPDIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")
target_compile_options(native_tests PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(native_tests PRIVATE -fsanitize=address,undefined)
set_target_properties(native_tests PROPERTIES C_STANDARD 11)
set_source_files_properties(${TOMLC17}/tomlc17.c PROPERTIES COMPILE_OPTIONS "-w")

enable_testing()
add_test(NAME native_tests COMMAND native_tests)
# The settings keep their strings for the whole run, by design.
set_tests_properties(native_tests PROPERTIES ENVIRONMENT ASAN_OPTIONS=detect_leaks=0)
