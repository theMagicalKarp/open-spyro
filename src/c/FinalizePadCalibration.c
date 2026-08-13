#include "globals.h"

extern void ResetPadStickCalibrationDefaults(unsigned char *cal);

/* 0x80053790, 432 bytes — validate and bake the pad-stick calibration block.
   Each of the four axes needs its recorded neutral (+0x20) at least 0x30 away
   from both reported extremes (+0x24 low / +0x28 high), otherwise the whole
   block falls back to the uncalibrated defaults and the function returns 0.
   Otherwise each axis gets its two breakpoints (+0x2c neutral+0x30 /
   +0x30 neutral-0x30) and the two fixed-point scale factors (+0x34 and +0x3c
   shorts), and the per-frame stick state is cleared: the snapshot head words,
   its four neutral 0x7f bytes, and the four 0x18-byte autorepeat records. */

/* Three of the register assignments here are decided by allocation PRIORITY
   (global.c allocno_compare: floor_log2(refs)*refs/live_length, ties by lower
   allocno), not by the code shape, so two dials are load-bearing:

   - the nested `do { } while (0)` wrappers: flow.c weights each mention of a
     pseudo by its basic block's loop depth, so wrapping one statement in N
     dummy loops adds N refs per mention without emitting an insn. Loop 2 needs
     `axis` lifted over `q` (both 9 refs; lengths 25 vs 22 -> 1.08 vs 1.227),
     which takes +2 refs on `axis`; the phase-3 fill loop needs `fill` (7 refs /
     11 insns) lifted over the shared `j` (18/21), which takes +6.
   - `j` is ONE variable across both fill loops on purpose: that makes it live
     across the phase-4 `rec` giv init, whose a0 preference then keeps `j` out
     of a0 (find_reg pass 0 skips regs_someone_prefers), leaving a0 for
     `neutral` and v1 for `k`.

   `rec` is a plain pointer biv, initialized BEFORE `k`, because both preheader
   inits have to come out in that order and loop.c always emits giv inits last
   (its per-biv giv list is built by prepending, so emission is reverse
   discovery order and the loop-test giv, recorded at the bottom compare, would
   go first). The three record-head stores are volatile only to stop loop.c
   reducing rec+0x48/0x4C/0x50 into a second induction register (+2 insns). */
int FinalizePadCalibration(unsigned char *cal) {
  unsigned char *p;
  unsigned char *q;
  unsigned char *fill;
  unsigned char *rec;
  unsigned char *fill2;
  int i;
  int axis;
  int ok;
  int j;
  int k;
  int mid;
  int hi;
  int num1;
  int num2;
  int neutral;
  int neutral2;

  ok = 1;
  for (i = 0; i < 4;) {
    p = cal + i;
    mid = p[0x20];
    if (mid + 0x30 >= p[0x28] || (i += 1, p[0x24] >= mid - 0x30)) {
      ResetPadStickCalibrationDefaults(cal);
      ok = 0;
      break;
    }
  }

  axis = 0;
  num1 = 0x4000;
  num2 = 0x3F80;
  do {
    do {
      do {
        p = cal + axis;
      } while (0);
    } while (0);
    q = cal + axis * 2;
    p[0x2C] = p[0x20] + 0x30;
    hi = p[0x28];
    hi = num1 / (hi - *(volatile unsigned char *)&p[0x2C]);
    p[0x30] = p[0x20] - 0x30;
    *(short *)(q + 0x34) = hi;
    *(short *)(q + 0x3C) =
        num2 / (*(volatile unsigned char *)&p[0x30] - p[0x24]);
    axis += 1;
  } while (axis < 4);

  neutral = 0x7F;
  j = 3;
  fill = cal + 3;
  *(int *)(cal + 0x00) = 0;
  *(int *)(cal + 0x04) = 0;
  *(int *)(cal + 0x08) = 0;
  *(int *)(cal + 0x18) = 1;
  *(int *)(cal + 0x1C) = 1;
  do {
    do {
      do {
        fill[0x14] = neutral;
        fill -= 1;
      } while (0);
    } while (0);
  } while (--j >= 0);

  neutral2 = 0x7F;
  rec = cal;
  k = 0;
  do {
    j = 3;
    fill2 = rec + 3;
    *(volatile int *)(rec + 0x4C) = 0;
    *(volatile int *)(rec + 0x50) = 0;
    *(volatile int *)(rec + 0x48) = 0;
    do {
      fill2[0x58] = neutral2;
      fill2 -= 1;
    } while (--j >= 0);
    k += 0x18;
    rec += 0x18;
  } while (k < 0x60);

  return ok;
}
