/* MATCHED 2026-08-14.
 *
 * Two levers, both source-level:
 *  - The two early-exit |face| sites are `abs()` (A160). gcc's expand_abs
 *    emits copy / compare / negate-in-place, which is the original's
 *    `move v0,s1` + `negu v0,v0`; a written-out `t = face; if (face < 0)
 *    t = -t;` gives `negu v0,s1` instead, because cse keeps the copy SOURCE
 *    canonical (A187) and a straight-line block cannot satisfy A187's
 *    condition 1.
 *  - `face` must then win s1 from `dir`, and `deadzone` must be the first
 *    call-crossing allocno (s0). Both are allocno_compare arithmetic
 *    (A195/A194): global.c ranks by floor_log2(refs)*refs/live_length and
 *    flow.c weights every mention by its basic block's loop depth, so the
 *    nested dummy `do { } while (0)` wrapper below buys `face` references
 *    without emitting an insn. With `abs(face)` there is only ONE mention
 *    inside the wrapper (+1 ref per level, not +2), so the depth is 16 where
 *    the written-out form needed 9; 15 is the measured minimum and every
 *    depth above it gives the same bytes. Once `face` wins s1 the deadzone
 *    wrapper the parked version carried is no longer needed.
 */
#include "globals.h"

extern int abs(int);
extern int ArcTan2(int y, int x, int high_precision);
extern int AbsAngleDelta8(int a, int b);
extern int func_800381BC(int from, int to);
extern int func_80038074(int angle, int step);
extern int func_80038EE0(int *actor, int angle, int p2, int p3, int p4);
extern int func_80039398(int *actor, int speed, int p2, int p3, int p4);
extern int func_80039688(int *actor, int angle, int speed, int p3, int p4,
                         int p5);

extern short g_anCosineLut[];
extern int D_800756C4; /* active camera mode (3 = 1.5x, 4 = 2x turn rate) */

/* Steers `actor` around a target point instead of straight at it: aims at the
   point `radius` away from `tgt` on the side the actor is already turning
   toward, and drives the walk with that heading. Returns the residual angle
   error (|delta|) when the actor is already lined up (inside `deadzone`) or
   nothing moved, 0x100 when the approach is refused because the target
   direction is within 5 of one of the two blocked headings `blockA`/`blockB`
   on the same side as the turn, and the negated move result otherwise.
   Camera mode D_800756C4 scales the turn speed by 1.5x / 2x.
   (0x80038638, 740 bytes.) */

int func_80038638(int *actor, int *tgt, int radius, int face, int deadzone,
                  int speed, int p6, int p7, int blockA, int blockB, int p10,
                  int p11, int flags) {
  int pt[3];
  int dir;
  int t;
  int e;
  int heading;
  int r;
  dir = ArcTan2(actor[3] - tgt[0], actor[4] - tgt[1], 0);
  face = func_800381BC(face, dir);
  do {
    do {
      do {
        do {
          do {
            do {
              do {
                do {
                  do {
                    do {
                      do {
                        do {
                          do {
                            do {
                              do {
                                do {
                                  t = abs(face);
                                } while (0);
                              } while (0);
                            } while (0);
                          } while (0);
                        } while (0);
                      } while (0);
                    } while (0);
                  } while (0);
                } while (0);
              } while (0);
            } while (0);
          } while (0);
        } while (0);
      } while (0);
    } while (0);
  } while (0);
  if (t < deadzone) {
    return t;
  }
  if (blockA != 0xff) {
    if (AbsAngleDelta8(dir, blockA) < 5) {
      e = func_800381BC(blockA, dir);
      if ((face < 0) && (e < 0)) {
        return 0x100;
      }
      if ((face > 0) && (e > 0)) {
        return 0x100;
      }
    }
  }
  if (blockB != 0xff) {
    if (AbsAngleDelta8(dir, blockB) < 5) {
      e = func_800381BC(blockB, dir);
      if ((face < 0) && (e < 0)) {
        return 0x100;
      }
      if ((face > 0) && (e > 0)) {
        return 0x100;
      }
    }
  }
  if (face < 0) {
    dir = func_80038074(dir, 3);
  } else {
    dir = func_80038074(dir, -3);
  }
  pt[0] = tgt[0] + ((g_anCosineLut[dir] * radius) >> 12);
  pt[1] = tgt[1] + ((g_anSineLut[dir] * radius) >> 12);
  heading = ArcTan2(pt[0] - actor[3], pt[1] - actor[4], 0);
  if (D_800756C4 == 3) {
    speed = speed + (speed >> 1);
  } else if (D_800756C4 == 4) {
    speed = speed * 2;
  }
  if (flags & 8) {
    func_80038EE0(actor, ArcTan2(tgt[0] - actor[3], tgt[1] - actor[4], 0), p6,
                  0, 0);
    r = func_80039688(actor, heading, speed, p10, p11, flags);
  } else {
    r = func_80038EE0(actor, heading, p6, p7, 1);
    if (r == 0) {
      goto stalled;
    }
    r = func_80039398(actor, speed, p10, p11, flags);
  }
  if (r != 0) {
    return -r;
  }
stalled:
  return abs(face);
}
