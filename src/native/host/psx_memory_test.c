/* src/native/host/psx_memory_test.c — unit tests for psx_memory.c (make
 * native-test). Built only when the wasm2c runtime is present
 * (tools/native_setup.sh; native/tests/CMakeLists.txt). */

#include <stdint.h>
#include <string.h>

#include "psx_memory.h"
#include "test.h"

static uint32_t read32(wasm_rt_memory_t *m, uint32_t addr) {
  uint32_t v;

  memcpy(&v, psx_ptr(m, addr), 4);
  return v;
}

static void write32(wasm_rt_memory_t *m, uint32_t addr, uint32_t v) {
  memcpy(psx_ptr(m, addr), &v, 4);
}

TEST(psx_memory_layout) {
  wasm_rt_memory_t m;

  CHECK_INT(psx_memory_init(&m), 0);
  /* the whole span up to the end of the dev-kit RAM is linear memory */
  CHECK_INT(m.size, (uint64_t)PSX_RAM_BASE + PSX_RAM_RESERVE);
  CHECK_INT(read32(&m, PSX_RAM_BASE), 0);
  /* every region the game uses is writable */
  write32(&m, PSX_SCRATCHPAD, 0x5C5C5C5C);
  write32(&m, NATIVE_MODULE_BASE, 0x11111111);
  write32(&m, NATIVE_STACK_TOP - 4, 0x22222222);
  write32(&m, PSX_RAM_BASE + PSX_RAM_RESERVE - 4, 0x33333333);
  CHECK_INT(read32(&m, PSX_SCRATCHPAD), 0x5C5C5C5C);
  CHECK_INT(read32(&m, NATIVE_STACK_TOP - 4), 0x22222222);
  CHECK_INT(read32(&m, PSX_RAM_BASE + PSX_RAM_RESERVE - 4), 0x33333333);
  psx_memory_free(&m);
}

TEST(psx_memory_kuseg_mirror) {
  wasm_rt_memory_t m;

  CHECK_INT(psx_memory_init(&m), 0);
  /* KSEG0 writes show through KUSEG, and the other way round */
  write32(&m, 0x80001234, 0xCAFEF00D);
  CHECK_INT(read32(&m, 0x00001234), 0xCAFEF00D);
  write32(&m, 0x00010000, 0x0BADBEEF);
  CHECK_INT(read32(&m, 0x80010000), 0x0BADBEEF);
  /* the first and last words of retail RAM */
  write32(&m, 0x00000000, 1);
  write32(&m, PSX_RAM_SIZE - 4, 2);
  CHECK_INT(read32(&m, PSX_RAM_BASE), 1);
  CHECK_INT(read32(&m, PSX_RAM_BASE + PSX_RAM_SIZE - 4), 2);
  /* the mirror is retail RAM only: dev-kit RAM above it is not aliased */
  write32(&m, PSX_RAM_BASE + PSX_RAM_SIZE, 0x77777777);
  CHECK_INT(read32(&m, PSX_RAM_SIZE), 0);
  psx_memory_free(&m);
}

TEST(psx_memory_reinit_is_zeroed) {
  wasm_rt_memory_t m;

  CHECK_INT(psx_memory_init(&m), 0);
  write32(&m, 0x80000100, 0xFFFFFFFF);
  psx_memory_free(&m);
  CHECK_INT(psx_memory_init(&m), 0);
  CHECK_INT(read32(&m, 0x80000100), 0);
  CHECK_INT(read32(&m, 0x00000100), 0);
  psx_memory_free(&m);
}
