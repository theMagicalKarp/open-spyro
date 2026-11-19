/* src/native/host/psx_memory.h — the native build's PS1 address space.
 *
 * Game C runs as wasm32 (ILP32), so a game pointer is a linear-memory offset.
 * The linear memory is laid out so every PS1 address the game uses is valid
 * as-is:
 *
 *   0x00000000-0x001FFFFF  KUSEG mirror of main RAM (same pages as KSEG0)
 *   0x01000000-0x017FFFFF  wasm-side module data (__memory_base)
 *   0x01800000-0x01FFFFFF  wasm-side C stack (grows down from 0x02000000)
 *   0x1F800000-0x1F8003FF  scratchpad
 *   0x80000000-0x807FFFFF  main RAM (KSEG0; 2 MiB retail + dev-kit headroom)
 *
 * Only reserved pages that are touched get committed, so the 2 GiB span costs
 * nothing. */

#ifndef OPEN_SPYRO_PSX_MEMORY_H
#define OPEN_SPYRO_PSX_MEMORY_H

#include <stdint.h>

#include "wasm-rt.h"

#define PSX_RAM_BASE 0x80000000u
/* retail main RAM, the KUSEG-mirrored part */
#define PSX_RAM_SIZE 0x00200000u
/* 8 MiB: DEMO_RECORD_BUFFER is dev-kit RAM */
#define PSX_RAM_RESERVE 0x00800000u
#define PSX_SCRATCHPAD 0x1F800000u
#define NATIVE_MODULE_BASE 0x01000000u
#define NATIVE_STACK_TOP 0x02000000u
/* table slot 0 stays null: a NULL fn pointer traps */
#define NATIVE_TABLE_BASE 1u
#define NATIVE_TABLE_SIZE 0x10000u

/* Allocate the linear memory and map the KUSEG mirror. Returns 0 on success. */
int psx_memory_init(wasm_rt_memory_t *mem);
void psx_memory_free(wasm_rt_memory_t *mem);

/* Host view of a PS1 address. */
static inline void *psx_ptr(wasm_rt_memory_t *mem, uint32_t addr) {
  return mem->data + addr;
}

#endif
