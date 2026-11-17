/* src/native/handasm/handasm.h — runtime for the literal hand-asm translation
 * (generator: `open-spyro native handasm`).
 *
 * The generated ha_*.c files hold the 100 hand-written functions as one C
 * function per cluster, with every MIPS register a uint32_t local. Memory is
 * identity-mapped (wasm32 linear address == PS1 address), so a load or store
 * is a plain pointer access at the address the original computes. */

#ifndef OPEN_SPYRO_HANDASM_H
#define OPEN_SPYRO_HANDASM_H

#include <stdint.h>

/* The register file a cluster is entered with and returns. r[0] stays 0. */
typedef struct HaRegs {
  uint32_t r[32];
  uint32_t hi, lo;
} HaRegs;

/* The emulated MIPS stack pointer for hand-asm frames (src/native/handasm/
 * runtime.c). The native C stack is separate; hand-asm code addresses its
 * frames through $sp like the original. */
extern uint32_t ha_mips_sp;

/* $ra a C-ABI wrapper enters a cluster with: no code lives there, so the
 * cluster's final `jr $ra` returns to the wrapper. */
#define HA_RET_MAGIC 0xFFFFFFF0u

void ha_enter(HaRegs *R);
void ha_leave(HaRegs *R);
_Noreturn void ha_bad_jump(const char *cluster, uint32_t target, uint32_t entry_ra);

/* GTE register access with the hardware's side effects (SXYP push, IRGB/ORGB,
 * LZCS/LZCR); everything else goes straight to PsyZ. */
uint32_t ha_gte_read(unsigned reg);
void ha_gte_write(unsigned reg, uint32_t v);
uint32_t ha_gte_ctrl_read(unsigned reg);
void ha_gte_ctrl_write(unsigned reg, uint32_t v);
void ha_gte_command(uint32_t cmd);

uint32_t ha_lwl(uint32_t addr, uint32_t rt);
uint32_t ha_lwr(uint32_t addr, uint32_t rt);
void ha_swl(uint32_t addr, uint32_t rt);
void ha_swr(uint32_t addr, uint32_t rt);

/* ---- used by the generated code ---------------------------------------- */

#define HA_PTR(t, a) ((t *)(uintptr_t)(uint32_t)(a))
#define HA_LW(a) (*HA_PTR(uint32_t, a))
#define HA_LH(a) ((uint32_t)(int32_t)*HA_PTR(int16_t, a))
#define HA_LHU(a) ((uint32_t)*HA_PTR(uint16_t, a))
#define HA_LB(a) ((uint32_t)(int32_t)*HA_PTR(int8_t, a))
#define HA_LBU(a) ((uint32_t)*HA_PTR(uint8_t, a))
#define HA_SW(a, v) (*HA_PTR(uint32_t, a) = (uint32_t)(v))
#define HA_SH(a, v) (*HA_PTR(uint16_t, a) = (uint16_t)(v))
#define HA_SB(a, v) (*HA_PTR(uint8_t, a) = (uint8_t)(v))
#define HA_LWL(a, rt) ha_lwl(a, rt)
#define HA_LWR(a, rt) ha_lwr(a, rt)
#define HA_SWL(a, rt) ha_swl(a, rt)
#define HA_SWR(a, rt) ha_swr(a, rt)

#define HA_MFC2(r) ha_gte_read(r)
#define HA_MTC2(r, v) ha_gte_write(r, v)
#define HA_CFC2(r) ha_gte_ctrl_read(r)
#define HA_CTC2(r, v) ha_gte_ctrl_write(r, v)
#define HA_GTE_CMD(c) ha_gte_command(c)

#define HA_MULT(a, b)                                                          \
  do {                                                                         \
    int64_t p_ = (int64_t)(int32_t)(a) * (int64_t)(int32_t)(b);                \
    lo = (uint32_t)p_;                                                         \
    hi = (uint32_t)((uint64_t)p_ >> 32);                                       \
  } while (0)
#define HA_MULTU(a, b)                                                         \
  do {                                                                         \
    uint64_t p_ = (uint64_t)(a) * (uint64_t)(b);                               \
    lo = (uint32_t)p_;                                                         \
    hi = (uint32_t)(p_ >> 32);                                                 \
  } while (0)
/* R3000 results for x/0 and INT_MIN/-1 (no exception). */
#define HA_DIV(a, b)                                                           \
  do {                                                                         \
    int32_t n_ = (int32_t)(a), d_ = (int32_t)(b);                              \
    if (d_ == 0) {                                                             \
      lo = n_ >= 0 ? 0xFFFFFFFFu : 1u;                                         \
      hi = (uint32_t)n_;                                                       \
    } else if (n_ == INT32_MIN && d_ == -1) {                                  \
      lo = 0x80000000u;                                                        \
      hi = 0;                                                                  \
    } else {                                                                   \
      lo = (uint32_t)(n_ / d_);                                                \
      hi = (uint32_t)(n_ % d_);                                                \
    }                                                                          \
  } while (0)
#define HA_DIVU(a, b)                                                          \
  do {                                                                         \
    uint32_t n_ = (a), d_ = (b);                                               \
    if (d_ == 0) {                                                             \
      lo = 0xFFFFFFFFu;                                                        \
      hi = n_;                                                                 \
    } else {                                                                   \
      lo = n_ / d_;                                                            \
      hi = n_ % d_;                                                            \
    }                                                                          \
  } while (0)

#define HA_LOCALS()                                                            \
  uint32_t at, v0, v1, a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, t7, s0, s1, \
      s2, s3, s4, s5, s6, s7, t8, t9, k0, k1, gp, sp, fp, ra, hi, lo
#define HA_REGS_(X)                                                            \
  X(1, at) X(2, v0) X(3, v1) X(4, a0) X(5, a1) X(6, a2) X(7, a3) X(8, t0)      \
  X(9, t1) X(10, t2) X(11, t3) X(12, t4) X(13, t5) X(14, t6) X(15, t7)         \
  X(16, s0) X(17, s1) X(18, s2) X(19, s3) X(20, s4) X(21, s5) X(22, s6)        \
  X(23, s7) X(24, t8) X(25, t9) X(26, k0) X(27, k1) X(28, gp) X(29, sp)        \
  X(30, fp) X(31, ra)
#define HA_LOAD1_(n, reg) reg = R->r[n];
#define HA_STORE1_(n, reg) R->r[n] = reg;
#define HA_RELOAD()                                                            \
  do {                                                                         \
    HA_REGS_(HA_LOAD1_) hi = R->hi;                                            \
    lo = R->lo;                                                                \
  } while (0)
#define HA_SPILL()                                                             \
  do {                                                                         \
    HA_REGS_(HA_STORE1_) R->hi = hi;                                           \
    R->lo = lo;                                                                \
  } while (0)

#endif
