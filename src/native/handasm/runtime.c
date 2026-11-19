/* src/native/handasm/runtime.c — support code for the generated hand-asm
 * translation (handasm.h): the emulated MIPS stack, cluster entry/exit, the
 * GTE register layer and the unaligned load/store helpers. */

#include "handasm.h"

/* PsyZ's register-level GTE endpoints (psyz/gte.h; declared here so this file
 * does not need PsyZ's SDK headers). */
unsigned int Psyz_GteDataRead(unsigned reg);
void Psyz_GteDataWrite(unsigned reg, unsigned int value);
unsigned int Psyz_GteCtrlRead(unsigned reg);
void Psyz_GteCtrlWrite(unsigned reg, unsigned int value);
void Psyz_GteCommand(unsigned int cmd);

int printf(const char *fmt, ...);
_Noreturn void abort(void);

/* The PS1 $gp (config/spyro.main.ld _gp). Hand-asm saves and restores it
 * like any other register. */
#define PS1_GP 0x80075264u

/* Hand-asm frames: a 256 KiB stack in module memory. The deepest hand-asm
 * frame on PS1 is well under 1 KiB. */
static uint8_t ha_stack[256 * 1024] __attribute__((aligned(16)));
uint32_t ha_mips_sp;

void ha_enter(HaRegs *R) {
  int i;

  if (ha_mips_sp == 0) {
    ha_mips_sp = (uint32_t)(uintptr_t)(ha_stack + sizeof ha_stack);
  }
  for (i = 0; i < 32; i++) {
    R->r[i] = 0;
  }
  R->hi = R->lo = 0;
  /* 0x20 bytes of argument home space, as a PS1 caller leaves. */
  R->r[29] = ha_mips_sp - 0x20;
  R->r[28] = PS1_GP;
  R->r[31] = HA_RET_MAGIC;
}

void ha_leave(HaRegs *R) { (void)R; }

_Noreturn void ha_bad_jump(const char *cluster, uint32_t target,
                           uint32_t entry_ra) {
  printf("handasm: %s: jump to %08x is not a known target (entry ra %08x)\n",
         cluster, target, entry_ra);
  abort();
}

/* ---- GTE ------------------------------------------------------------------
 */

static uint32_t lzcs;

static uint32_t orgb(void) {
  uint32_t v = 0;
  unsigned k;

  for (k = 0; k < 3; k++) {
    int32_t c = (int32_t)Psyz_GteDataRead(9 + k) >> 7;

    if (c < 0) {
      c = 0;
    } else if (c > 0x1F) {
      c = 0x1F;
    }
    v |= (uint32_t)c << (5 * k);
  }
  return v;
}

uint32_t ha_gte_read(unsigned reg) {
  switch (reg) {
  case 15: /* SXYP reads as SXY2 */
    return Psyz_GteDataRead(14);
  case 28: /* IRGB and ORGB both read as ORGB */
  case 29:
    return orgb();
  case 30:
    return lzcs;
  case 31: { /* LZCR: leading bits equal to LZCS's sign bit */
    uint32_t v = (lzcs & 0x80000000u) ? ~lzcs : lzcs;
    uint32_t n = 0;

    while (n < 32 && !(v & (0x80000000u >> n))) {
      n++;
    }
    return n;
  }
  default:
    return Psyz_GteDataRead(reg);
  }
}

void ha_gte_write(unsigned reg, uint32_t v) {
  switch (reg) {
  case 15: /* SXYP pushes the screen-XY FIFO */
    Psyz_GteDataWrite(12, Psyz_GteDataRead(13));
    Psyz_GteDataWrite(13, Psyz_GteDataRead(14));
    Psyz_GteDataWrite(14, v);
    Psyz_GteDataWrite(15, v);
    break;
  case 28: /* IRGB sets IR1..IR3 from 5:5:5 */
    Psyz_GteDataWrite(9, (v & 0x1F) << 7);
    Psyz_GteDataWrite(10, ((v >> 5) & 0x1F) << 7);
    Psyz_GteDataWrite(11, ((v >> 10) & 0x1F) << 7);
    break;
  case 29: /* ORGB, LZCR: read-only */
  case 31:
    break;
  case 30:
    lzcs = v;
    break;
  default:
    Psyz_GteDataWrite(reg, v);
    break;
  }
}

uint32_t ha_gte_ctrl_read(unsigned reg) { return Psyz_GteCtrlRead(reg); }

void ha_gte_ctrl_write(unsigned reg, uint32_t v) { Psyz_GteCtrlWrite(reg, v); }

void ha_gte_command(uint32_t cmd) { Psyz_GteCommand(cmd); }

/* ---- unaligned word access (little-endian lwl/lwr/swl/swr) ----------------
 */

uint32_t ha_lwl(uint32_t addr, uint32_t rt) {
  uint32_t w = HA_LW(addr & ~3u);
  unsigned sh = 8 * (3 - (addr & 3));

  return (rt & (0xFFFFFFFFu >> (32 - sh) & -(uint32_t)(sh != 0))) | (w << sh);
}

uint32_t ha_lwr(uint32_t addr, uint32_t rt) {
  uint32_t w = HA_LW(addr & ~3u);
  unsigned sh = 8 * (addr & 3);

  return (rt & ~(0xFFFFFFFFu >> sh)) | (w >> sh);
}

void ha_swl(uint32_t addr, uint32_t rt) {
  uint32_t *p = HA_PTR(uint32_t, addr & ~3u);
  unsigned sh = 8 * (3 - (addr & 3));

  *p = (*p & ~(0xFFFFFFFFu >> sh)) | (rt >> sh);
}

void ha_swr(uint32_t addr, uint32_t rt) {
  uint32_t *p = HA_PTR(uint32_t, addr & ~3u);
  unsigned sh = 8 * (addr & 3);

  *p = (*p & ~(0xFFFFFFFFu << sh)) | (rt << sh);
}
