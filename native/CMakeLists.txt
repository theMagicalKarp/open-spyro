# native/CMakeLists.txt — the native build: macOS arm64,
# Linux arm64 and Linux x86_64.
#
#   make native        configure + build into build/native/
#   make native-run    run the executable
#
# Two halves:
#   1. native/wasm (ExternalProject, wasi-sdk toolchain): game C + PsyZ core
#      -> wasm32 modules. ILP32 with PS1 RAM at its real addresses
#      (docs/native-spike.md).
#   2. this project (host clang or gcc): wasm2c turns each module into C, which
#      is compiled for the host and linked with the host runtime (src/native/host)
#      and PsyZ's SDL3 platform layer, built natively.
#
# Needs tools/native_setup.sh (wasi-sdk, wasm2c, SDL3, cmake). The PS1 build
# never reads anything here.

cmake_minimum_required(VERSION 3.21)
project(open_spyro_native C CXX)
# compile_commands.json for editors (`make compdb` merges it with the wasm and
# test builds' into the repo root).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CMake 4.5+ only (ExternalProject install prefix); older CMake (Ubuntu 24.04
# ships 3.28) keeps the old behaviour, which this build does not depend on.
if(POLICY CMP0225)
  cmake_policy(SET CMP0225 NEW)
endif()

if(APPLE)
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(FATAL_ERROR "the native build targets macOS on arm64 only")
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|x86_64|AMD64")
    message(FATAL_ERROR "the native build targets Linux on arm64 or x86_64 only")
  endif()
else()
  message(FATAL_ERROR "the native build targets macOS arm64, Linux arm64 and Linux x86_64")
endif()

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(PSYZ ${ROOT}/external/psyz/psyz)
# tomlc17: the TOML parser for the host's config file (main.c).
set(TOMLC17 ${ROOT}/external/tomlc17/src)
# Dear ImGui + ImPlot: the diagnostics overlay (src/native/host/diag.cpp).
set(IMGUI ${ROOT}/external/imgui)
set(IMPLOT ${ROOT}/external/implot)
set(TOOLS ${ROOT}/build/native-tools)
set(WASI_SDK ${TOOLS}/wasi-sdk-34.0 CACHE PATH "wasi-sdk install")
set(WASM2C_RT ${TOOLS}/wasm2c-runtime CACHE PATH "wasm2c runtime sources")
# wasm2c from tools/native_setup.sh, built from the same source as its runtime.
find_program(WASM2C wasm2c PATHS ${TOOLS}/wabt-1.0.42/bin NO_DEFAULT_PATH REQUIRED)
find_program(UV uv REQUIRED)
# SDL3: the pinned static build tools/native_setup.sh makes in
# build/native-tools/sdl3, linked into the executable. Only that prefix is
# searched, so a system (shared) SDL3 is never picked up instead.
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-static
    PATHS ${TOOLS}/sdl3 NO_DEFAULT_PATH)

if(NOT EXISTS ${WASI_SDK}/bin/clang OR NOT EXISTS ${WASM2C_RT}/wasm-rt-impl.c)
  message(FATAL_ERROR "native toolchain missing: run tools/native_setup.sh")
endif()
if(NOT EXISTS ${PSYZ}/CMakeLists.txt)
  message(FATAL_ERROR "PsyZ missing: git submodule update --init external/psyz")
endif()
if(NOT EXISTS ${TOMLC17}/tomlc17.c)
  message(FATAL_ERROR "tomlc17 missing: git submodule update --init external/tomlc17")
endif()
if(NOT EXISTS ${IMGUI}/imgui.cpp OR NOT EXISTS ${IMPLOT}/implot.cpp)
  message(FATAL_ERROR "ImGui/ImPlot missing: git submodule update --init external/imgui external/implot")
endif()

# --- 1. wasm modules ----------------------------------------------------------
include(ExternalProject)
set(WASM_BIN ${CMAKE_BINARY_DIR}/wasm)
ExternalProject_Add(wasm_modules
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wasm
    BINARY_DIR ${WASM_BIN}
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${WASI_SDK}/share/cmake/wasi-sdk-p1.cmake
        -DWASI_SDK_PREFIX=${WASI_SDK}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    BUILD_ALWAYS ON
    # wasm_modules-configure: `make compdb` configures without building.
    STEP_TARGETS configure
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${WASM_BIN}/spyro.wasm ${WASM_BIN}/gate.wasm)

# wasm2c each module into <name>_w.c / <name>_w.h, plus its GOT.mem table.
# The overlays' symbol files: overlay C reaches overlay data at its PS1 address
# (inside the image loaded at 0x8007aa38) through GOT.mem imports too.
file(GLOB overlay_syms CONFIGURE_DEPENDS
    ${ROOT}/config/overlays/*.symbols.txt
    ${ROOT}/config/overlays/*.undefined_syms.auto.txt
    ${ROOT}/config/overlays/undefined_syms_manual.txt)
set(overlay_sym_args)
foreach(f ${overlay_syms})
  list(APPEND overlay_sym_args --syms ${f})
endforeach()

function(wasm2c_module name)
  set(out ${CMAKE_BINARY_DIR}/${name}_w)
  add_custom_command(
      OUTPUT ${out}.c ${out}.h ${out}_got.c
      COMMAND ${WASM2C} ${WASM_BIN}/${name}.wasm -n ${name} -o ${out}.c
      COMMAND ${UV} run --project ${ROOT}/tools/open-spyro --quiet
              open-spyro native got-table ${out}.h --module ${name} -o ${out}_got.c
              ${overlay_sym_args}
      WORKING_DIRECTORY ${ROOT}
      DEPENDS wasm_modules ${WASM_BIN}/${name}.wasm
      COMMENT "wasm2c ${name}.wasm")
endfunction()
wasm2c_module(spyro)
wasm2c_module(gate)

set(WASM2C_SOURCES ${WASM2C_RT}/wasm-rt-impl.c ${WASM2C_RT}/wasm-rt-mem-impl.c)

# --- 2. PsyZ platform layer, native ------------------------------------------
# The SDL3 GPU backend (Metal on macOS, Vulkan on Linux) and its software fallback, plus logging. The
# rest of PsyZ's platform layer (threads, audio, card files) is replaced by
# single-threaded wasm-side stubs (src/native/wasm/psyz_bridge.c); audio
# output is src/native/host/audio.c.
add_library(psyz_platform STATIC
    ${PSYZ}/src/platform/sdl3_gpu.c
    ${PSYZ}/src/platform/sdl3_software.c
    ${PSYZ}/src/platform/sdl3_log.c)
target_compile_definitions(psyz_platform PUBLIC __psyz
    PRIVATE _FILE_OFFSET_BITS=64 "PSYZ_TITLE=\"open-spyro\"")
# PRIVATE: PsyZ's include/ shadows system headers (setjmp.h) that the wasm2c
# runtime needs; only imports.c sees it (below).
target_include_directories(psyz_platform PRIVATE ${PSYZ}/include ${PSYZ}/src)
target_compile_options(psyz_platform PRIVATE -O2 -w)
target_link_libraries(psyz_platform PUBLIC SDL3::SDL3-static)
set_target_properties(psyz_platform PROPERTIES C_STANDARD 11)

# --- 3. Dear ImGui + ImPlot, for the diagnostics overlay -----------------------
# The SDL3 platform and SDL_GPU renderer backends (SPIR-V on Vulkan, MSL or
# metallib on Metal).
add_library(imgui STATIC
    ${IMGUI}/imgui.cpp
    ${IMGUI}/imgui_draw.cpp
    ${IMGUI}/imgui_tables.cpp
    ${IMGUI}/imgui_widgets.cpp
    ${IMGUI}/backends/imgui_impl_sdl3.cpp
    ${IMGUI}/backends/imgui_impl_sdlgpu3.cpp
    ${IMPLOT}/implot.cpp
    ${IMPLOT}/implot_items.cpp)
target_include_directories(imgui PUBLIC ${IMGUI} ${IMGUI}/backends ${IMPLOT})
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_compile_options(imgui PRIVATE -O2 -w)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)
set_target_properties(imgui PROPERTIES CXX_STANDARD 17)

# --- 4. executables -----------------------------------------------------------
add_executable(spyro
    ${ROOT}/src/native/host/main.c
    ${ROOT}/src/native/host/config.c
    ${ROOT}/src/native/host/diag.cpp
    ${ROOT}/src/native/host/diag_stats.c
    ${ROOT}/src/native/host/imports.c
    ${ROOT}/src/native/host/keymap.c
    ${ROOT}/src/native/host/audio.c
    ${ROOT}/src/native/host/audio_format.c
    ${ROOT}/src/native/host/host_io.c
    ${ROOT}/src/native/host/script.c
    ${ROOT}/src/native/host/stick.c
    ${ROOT}/src/native/host/psx_memory.c
    ${TOMLC17}/tomlc17.c
    ${CMAKE_BINARY_DIR}/spyro_w.c
    ${CMAKE_BINARY_DIR}/spyro_w_got.c
    ${WASM2C_SOURCES})
target_include_directories(spyro PRIVATE
    ${CMAKE_BINARY_DIR} ${WASM2C_RT} ${ROOT}/src/native/host ${TOMLC17})
target_compile_options(spyro PRIVATE -O2)
set_source_files_properties(${CMAKE_BINARY_DIR}/spyro_w.c PROPERTIES
    COMPILE_OPTIONS "-w")
target_link_libraries(spyro PRIVATE psyz_platform imgui m)
# Linux: fold in the C++ runtime (ImGui) too, leaving only glibc and the
# libraries SDL3 dlopens (X11/Wayland, Vulkan, audio) to the system. macOS's
# libc++ is part of the OS, like libSystem.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(spyro PRIVATE -static-libstdc++ -static-libgcc)
endif()
set_source_files_properties(${ROOT}/src/native/host/imports.c PROPERTIES
    INCLUDE_DIRECTORIES ${PSYZ}/include)

add_executable(gate
    ${ROOT}/native/gate/gate_host.c
    ${ROOT}/src/native/host/psx_memory.c
    ${CMAKE_BINARY_DIR}/gate_w.c
    ${CMAKE_BINARY_DIR}/gate_w_got.c
    ${WASM2C_SOURCES})
target_include_directories(gate PRIVATE
    ${CMAKE_BINARY_DIR} ${WASM2C_RT} ${ROOT}/src/native/host)
target_compile_options(gate PRIVATE -O1)
set_source_files_properties(${CMAKE_BINARY_DIR}/gate_w.c PROPERTIES
    COMPILE_OPTIONS "-w")
