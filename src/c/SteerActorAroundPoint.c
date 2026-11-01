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
#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Steers `actor` around a target point instead of straight at it: aims at the
   point `radius` away from `tgt` on the side the actor is already turning
   toward, and drives the walk with that heading. Returns the residual angle
   error (|delta|) when the actor is already lined up (inside `deadzone`) or
   nothing moved, 0x100 when the approach is refused because the target
   direction is within 5 of one of the two blocked headings `blockA`/`blockB`
   on the same side as the turn, and the negated move result otherwise.
   Camera mode g_nActorFrameDelta scales the turn speed by 1.5x / 2x.
   (0x80038638, 740 bytes.) */

int SteerActorAroundPoint(Actor *actor, int *tgt, int radius, int face,
                          int deadzone, int speed, int turnStep,
                          int turnThreshold, int blockA, int blockB,
                          int moveGroup, int moveRadius, int flags) {
  int pt[3];
  int dir;
  int t;
  int e;
  int heading;
  int r;
  dir = ArcTan2(actor->posX - tgt[0], actor->posY - tgt[1], 0);
  face = WrapAngleDelta8(face, dir);
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
      e = WrapAngleDelta8(blockA, dir);
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
      e = WrapAngleDelta8(blockB, dir);
      if ((face < 0) && (e < 0)) {
        return 0x100;
      }
      if ((face > 0) && (e > 0)) {
        return 0x100;
      }
    }
  }
  if (face < 0) {
    dir = WrapAddAngle8(dir, 3);
  } else {
    dir = WrapAddAngle8(dir, -3);
  }
  pt[0] = tgt[0] + ((g_anCosineLut[dir] * radius) >> 12);
  pt[1] = tgt[1] + ((g_anSineLut[dir] * radius) >> 12);
  heading = ArcTan2(pt[0] - actor->posX, pt[1] - actor->posY, 0);
  if (g_nActorFrameDelta == 3) {
    speed = speed + (speed >> 1);
  } else if (g_nActorFrameDelta == 4) {
    speed = speed * 2;
  }
  if (flags & MOVE_STRAFE) {
    TurnActorYawTowardAngle(
        actor, ArcTan2(tgt[0] - actor->posX, tgt[1] - actor->posY, 0), turnStep,
        0, 0);
    r = MoveActorAlongAngle(actor, heading, speed, moveGroup, moveRadius,
                            flags);
  } else {
    r = TurnActorYawTowardAngle(actor, heading, turnStep, turnThreshold, 1);
    if (r == 0) {
      goto stalled;
    }
    r = MoveActorAlongHeading(actor, speed, moveGroup, moveRadius, flags);
  }
  if (r != 0) {
    return -r;
  }
stalled:
  return abs(face);
}
