# native/wasm/CMakeLists.txt — the wasm32 half of the native build.
#
# Configured by native/CMakeLists.txt as an ExternalProject with the wasi-sdk
# toolchain file; never configure it directly. Produces:
#
#   spyro.wasm   the game: game C (src/main, src/overlays, src/native/game),
#                the translated hand-written asm (src/native/handasm), PsyZ
#                core and the wasm-side runtime (src/native/wasm). Also holds
#                the stub main (native_stub_main), a disc-free smoke
#                test
#   gate.wasm    the ILP32 gate: level.c + native/gate/gate_wasm.c
#
# Every module is position-independent (-fPIC, -pie) so undefined game globals
# become GOT.mem imports that the host resolves to PS1 addresses (open-spyro
# native got-table). No libc is linked: wasi-libc's libc.a is not PIC; the
# module carries its own (src/native/wasm/libc.c). Headers still come from the
# wasi sysroot.

cmake_minimum_required(VERSION 3.21)
project(open_spyro_wasm C)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(PSYZ ${ROOT}/external/psyz/psyz)

# PsyZ's own platform-independent source list, read from its CMakeLists so a
# submodule bump picks up added/removed files.
file(READ ${PSYZ}/CMakeLists.txt psyz_cmake)
string(REGEX MATCH "set\\(PSYZ_SOURCES([^)]*)\\)" _ "${psyz_cmake}")
string(REGEX REPLACE "[ \t\r\n]+" ";" psyz_core "${CMAKE_MATCH_1}")
list(FILTER psyz_core EXCLUDE REGEX "^$")
list(TRANSFORM psyz_core PREPEND ${PSYZ}/)
# The software sqrt (PsyZ's default) and the SPU emulator are also core.
list(APPEND psyz_core ${PSYZ}/src/psyz/libgte_sw_sqrt.c ${PSYZ}/src/psyz/psyz_spu.c)

# --no-wasm-opt: clang otherwise runs any wasm-opt it finds on PATH after
# linking, which makes the output depend on the machine and strips the name
# section (wasm2c then names functions f<N>). clang -O2 on the wasm2c output
# optimizes anyway.
set(PIC_LINK
    --no-wasm-opt -nostdlib -Wl,--no-entry -Wl,--experimental-pic -Wl,-pie
    -Wl,--unresolved-symbols=import-dynamic -Wl,--import-memory
    -Wl,--export=__wasm_call_ctors -Wl,--export=__wasm_apply_data_relocs)

# PsyZ core: built as PsyZ builds it natively, plus -fPIC.
add_library(psyz_core OBJECT ${psyz_core})
target_compile_options(psyz_core PRIVATE -O2 -fPIC -Wno-everything)
target_compile_definitions(psyz_core PUBLIC __psyz PRIVATE _FILE_OFFSET_BITS=64)
target_include_directories(psyz_core PUBLIC ${PSYZ}/include PRIVATE ${PSYZ}/src)
set_target_properties(psyz_core PROPERTIES C_STANDARD 11)
# Calls whose declaration in the calling file differs from PsyZ's definition
# go through an adapter with the caller's signature (src/native/wasm/
# psyz_fixups.c): wasm traps on a mismatched call.
set_source_files_properties(${PSYZ}/../decomp/src/libspu/s_crwa.c PROPERTIES
    COMPILE_DEFINITIONS "WaitEvent=PsyzFix_WaitEvent")
set_source_files_properties(${PSYZ}/../decomp/src/libcard/init.c PROPERTIES
    COMPILE_DEFINITIONS "StartCARD2=PsyzFix_StartCARD2;StopCARD2=PsyzFix_StopCARD2")
set_source_files_properties(${PSYZ}/../decomp/src/libcd/sys.c PROPERTIES
    COMPILE_DEFINITIONS "CD_datasync=PsyzFix_CD_datasync;CD_getsector2=PsyzFix_CD_getsector2")

# The wasm-side runtime: libc, the PsyZ platform bridge, the SDK shim and
# libmcrd (memcard.c), the entry points (game_main.c; stub_main.c, the disc-free smoke test).
add_library(native_runtime OBJECT
    ${ROOT}/src/native/wasm/libc.c
    ${ROOT}/src/native/wasm/psyz_bridge.c
    ${ROOT}/src/native/wasm/sdk_shim.c
    ${ROOT}/src/native/wasm/shim_helpers.c
    ${ROOT}/src/native/wasm/memcard.c
    ${ROOT}/src/native/wasm/psyz_fixups.c
    ${ROOT}/src/native/wasm/game_main.c
    ${ROOT}/src/native/wasm/stub_main.c)
target_compile_options(native_runtime PRIVATE -O2 -fPIC -Wall -Wno-macro-redefined)
target_link_libraries(native_runtime PRIVATE psyz_core)
target_include_directories(native_runtime PRIVATE ${ROOT}/src/native/wasm)
set_target_properties(native_runtime PROPERTIES C_STANDARD 11)

# Game C: the flags every game translation unit gets natively. ILP32 matches
# the PS1 data model; -DM2CTX blanks INCLUDE_ASM / SDATA; __psyz selects the
# native side of include/port.h and match_asm.h. -fwrapv / -fno-strict-aliasing:
# gcc 2.7.2 did neither optimization, and the matched C relies on that.
function(game_c target)
  target_compile_options(${target} PRIVATE
      -O2 -fPIC -std=gnu89 -nostdinc -funsigned-char -fno-strict-aliasing
      -fwrapv -Wno-everything -Werror=implicit-function-declaration)
  target_compile_definitions(${target} PRIVATE M2CTX __psyz)
  target_include_directories(${target} PRIVATE ${ROOT}/include ${ROOT}/asm)
endfunction()

# All game C. Left out: files whose PS1 C is register tricks with no native
# meaning (src/native/game holds their replacements, same functions in plain
# C), and the overlays' OverlayEnd markers: empty functions whose address is
# the end of the overlay image. Natively that address is a PS1 address like
# any other overlay symbol (a GOT.mem import).
file(GLOB_RECURSE game_sources CONFIGURE_DEPENDS ${ROOT}/src/main/*.c ${ROOT}/src/overlays/*.c)
list(FILTER game_sources EXCLUDE REGEX "/src/main/render/(ot|static_regs)\\.c$")
list(FILTER game_sources EXCLUDE REGEX "/src/overlays/[^/]+/overlay_end\\.c$")
file(GLOB_RECURSE native_game_sources CONFIGURE_DEPENDS ${ROOT}/src/native/game/*.c)
add_library(game_c OBJECT ${game_sources} ${native_game_sources})
game_c(game_c)
target_compile_options(game_c PRIVATE -include ${ROOT}/src/native/wasm/game_prelude.h)

# The hand-written asm, translated by `open-spyro native handasm`.
file(GLOB handasm_sources CONFIGURE_DEPENDS ${ROOT}/src/native/handasm/*.c)
list(FILTER handasm_sources EXCLUDE REGEX "_test\\.c$") # native/tests
add_library(handasm OBJECT ${handasm_sources})
target_compile_options(handasm PRIVATE -O2 -fPIC -fwrapv -fno-strict-aliasing -Wno-everything)
target_compile_definitions(handasm PRIVATE __psyz)
target_include_directories(handasm PRIVATE ${ROOT}/src/native/handasm)

add_executable(spyro
    $<TARGET_OBJECTS:psyz_core> $<TARGET_OBJECTS:native_runtime>
    $<TARGET_OBJECTS:game_c> $<TARGET_OBJECTS:handasm>)
# Every call must resolve: an unresolved function would become an import
# the host does not provide (wasm2c then fails to link), and a call through a
# mismatched signature traps. Both are link errors here.
target_link_options(spyro PRIVATE ${PIC_LINK} -Wl,--fatal-warnings)
set_target_properties(spyro PROPERTIES LINKER_LANGUAGE C SUFFIX .wasm)

# ILP32 gate.
add_library(gate_level OBJECT ${ROOT}/src/main/load/level.c)
game_c(gate_level)
add_library(gate_driver OBJECT ${ROOT}/native/gate/gate_wasm.c)
game_c(gate_driver)
add_executable(gate $<TARGET_OBJECTS:gate_level> $<TARGET_OBJECTS:gate_driver>)
target_link_options(gate PRIVATE ${PIC_LINK})
set_target_properties(gate PROPERTIES LINKER_LANGUAGE C SUFFIX .wasm)
