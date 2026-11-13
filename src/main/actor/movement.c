#include "actor.h"
#include "actor_type.h"
#include "angle.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "vecmath.h"

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

/* Steers `actor` around a target point instead of straight at it: aims at the
   point `radius` away from `tgt` on the side the actor is already turning
   toward, and drives the walk with that heading. Returns the residual angle
   error (|delta|) when the actor is already lined up (inside `deadzone`) or
   nothing moved, 0x100 when the approach is refused because the target
   direction is within 5 of one of the two blocked headings `blockA`/`blockB`
   on the same side as the turn, and the negated move result otherwise.
   The frame delta (g_nActorFrameDelta 3 / 4) scales the turn speed by 1.5x /
   2x; blockA/blockB STEER_NO_BLOCK (0xFF) disables a blocked heading.
   (0x80038638, 740 bytes.) */

int SteerActorAroundPoint(Actor *actor, int *tgt, int radius, int face,
                          int deadzone, int speed, int turnStep,
                          int turnThreshold, int blockA, int blockB,
                          int contactRadius, int wallRadius, int flags) {
  int pt[3];
  int dir;
  int t;
  int e;
  int heading;
  int r;
  dir = ArcTan2(actor->posX - tgt[0], actor->posY - tgt[1], ATAN_ANGLE8);
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
  if (blockA != STEER_NO_BLOCK) {
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
  if (blockB != STEER_NO_BLOCK) {
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
  heading = ArcTan2(pt[0] - actor->posX, pt[1] - actor->posY, ATAN_ANGLE8);
  if (g_nActorFrameDelta == 3) {
    speed = speed + (speed >> 1);
  } else if (g_nActorFrameDelta == 4) {
    speed = speed * 2;
  }
  if (flags & MOVE_STRAFE) {
    TurnActorYawTowardAngle(
        actor, ArcTan2(tgt[0] - actor->posX, tgt[1] - actor->posY, ATAN_ANGLE8),
        turnStep, 0, YAW_HOLD_IN_BAND);
    r = MoveActorAlongAngle(actor, heading, speed, contactRadius, wallRadius,
                            flags);
  } else {
    r = TurnActorYawTowardAngle(actor, heading, turnStep, turnThreshold,
                                YAW_STEP_IN_BAND);
    if (r == 0) {
      goto stalled;
    }
    r = MoveActorAlongHeading(actor, speed, contactRadius, wallRadius, flags);
  }
  if (r != 0) {
    return -r;
  }
stalled:
  return abs(face);
}

/* Initial Z velocity for a lob from `pos` that lands on `target` (0x8003891c):
   the flight time is the XY distance over `speed` frames (written to *pOutTime
   when non-null), and vz solves dz + gravity * t^2 / 2 over that time; 0 when
   the flight takes under a frame. */
int ComputeLobVelocityZ(int *pos, int *target, int speed, int gravity,
                        int *pOutTime) {
  int v[2];
  int t;
  int r;

  v[0] = pos[0] - target[0];
  v[1] = pos[1] - target[1];
  t = VectorLength(v, VEC_LEN_XY) / speed;
  if (pOutTime != 0) {
    *pOutTime = t;
  }
  if (t != 0) {
    return -((pos[2] - target[2]) + ((gravity * (t * t)) >> 1)) / t;
  }
  return 0;
}

/* Rotate `actor`'s byte yaw (rotZ) toward the direction of Spyro, stepping
   by at most `step` (boosted 1.5x at frame delta 3, 2x at 4) but never
   past the actual delta. Returns 0 while still outside the `threshold`
   dead-band (and keeps turning); returns 1 once within it, applying one more
   step when `flag` is YAW_STEP_IN_BAND. */
int TrackEntityYawTowardSpyro(Actor *actor, int step, int threshold, int flag) {
  int target = ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                       g_anSpyroWorldPos[1] - actor->posY, ATAN_ANGLE8);
  int delta = AbsAngleDelta8(target, actor->rotZ);

  if (g_nActorFrameDelta == 3) {
    step += step >> 1;
  } else if (g_nActorFrameDelta == 4) {
    step <<= 1;
  }
  if (delta < step) {
    step = delta;
  }
  if (threshold < AbsAngleDelta8(target, actor->rotZ)) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
    return 0;
  }
  if (flag) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
  }
  return 1;
}

/* Rotate `actor`'s byte yaw (rotZ) toward an explicit `target` angle,
   stepping by at most `step` (boosted 1.5x at frame delta 3, 2x at 4) but
   never past the actual delta. Returns 0 while still outside the `threshold`
   dead-band (and keeps turning); returns 1 once within it, applying one more
   step when `flag` is YAW_STEP_IN_BAND. */
int TurnActorYawTowardAngle(Actor *actor, int target, int step, int threshold,
                            int flag) {
  int delta = AbsAngleDelta8(target, actor->rotZ);

  if (g_nActorFrameDelta == 3) {
    step += step >> 1;
  } else if (g_nActorFrameDelta == 4) {
    step <<= 1;
  }
  if (delta < step) {
    step = delta;
  }
  if (delta >= threshold) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
    return 0;
  }
  if (flag) {
    actor->rotZ = ApproachAngle8(target, actor->rotZ, step, (step >> 1) + 1);
  }
  return 1;
}

/* Steer an actor's yaw toward Spyro (0x80038fc8, 0x260). Computes the
   signed wrap-around delta between the ArcTan2 bearing to Spyro and the
   tracked yaw at *yawp, picks/holds a turn direction in *dirp (dir 0
   seeds from the delta sign; an established direction only flips once
   the opposite-side delta exceeds `thresh`), and advances *yawp by the
   gamestate-scaled `speed` clamped to the remaining delta. While more
   than 0x101 off-bearing it swaps the actor's anim pair (anim..frameRate)
   (animId when turning positive, animId+1 otherwise) through
   OnActorAnimQueued. Stores the coarse yaw in rotZ; returns the delta.

   In each anim arm the 0x10 stores are bare literals (a shared carrier costs
   the birthing boost and swaps the `li` with the call-arg copy), and `old` is
   a `$2` register local: as a pseudo it spans the cross-jumped tail, so it is
   a global allocno assigned after local-alloc has already given v0 to the
   constant. */
int SteerActorYawTowardSpyro(Actor *actor, int *dirp, int *yawp, int speed,
                             int thresh, int animId) {
  int bearing;
  int yaw;
  int delta;
  int oppDelta;
  int absDelta;
  int dir;
  int next;
  int newYaw;

  bearing = ArcTan2(g_anSpyroWorldPos[0] - actor->posX,
                    g_anSpyroWorldPos[1] - actor->posY, ATAN_ANGLE12);
  yaw = *yawp;
  delta = (bearing - yaw) & ANGLE12_MASK;
  bearing -= 0x800;
  oppDelta = (bearing - yaw) & ANGLE12_MASK;
  if (delta >= 0x801) {
    delta -= 0x1000;
  }
  if (oppDelta >= 0x801) {
    oppDelta -= 0x1000;
  }
  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  absDelta = abs(delta);
  if (absDelta < speed) {
    speed = absDelta;
  }
  dir = *dirp;
  if (dir == 0) {
    if (delta < 0) {
      *dirp = -1;
    } else {
      *dirp = 1;
    }
  } else if (delta < 0) {
    if (dir > 0) {
      if (thresh < abs(oppDelta)) {
        *dirp = -1;
      }
    }
  } else if (dir < 0 && delta > 0) {
    if (thresh < abs(oppDelta)) {
      *dirp = 1;
    }
  }
  if (abs(delta) >= 0x101) {
    next = animId + 1;
    if (*dirp == 1) {
      if (actor->nextAnim != animId) {
        /* Hard-register pins (A374 / F14j): each holds the register the
           original uses for it, which settles the competing temps. */
        register int old asm("$2");
        int oldFrame;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        oldFrame = actor->nextFrame;
        actor->frameProgress = ANIM_RATE_TICKS(4);
        actor->frameRate = ANIM_RATE_TICKS(4);
        actor->nextAnim = animId;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = oldFrame;
        OnActorAnimQueued(actor, oldFrame, next);
      }
    } else {
      if (actor->nextAnim != next) {
        register int old asm("$2");
        int oldFrame;
        g_nActorAnimDone = 0;
        old = actor->nextAnim;
        oldFrame = actor->nextFrame;
        actor->frameProgress = ANIM_RATE_TICKS(4);
        actor->frameRate = ANIM_RATE_TICKS(4);
        actor->nextAnim = next;
        actor->nextFrame = 0;
        actor->anim = old;
        actor->frame = oldFrame;
        OnActorAnimQueued(actor, oldFrame, next);
      }
    }
  }
  newYaw = *yawp + speed * *dirp;
  *yawp = newYaw;
  if (newYaw < 0) {
    *yawp = newYaw + 0x1000;
  }
  actor->rotZ = *yawp >> 4;
  return delta;
}

/* Guided-move step along the actor's own heading (0x80039398, 0x2f0).
   Sibling of MoveActorAlongAngle, but the direction comes from the actor's rotZ
   byte instead of an angle param. Advances the cached position
   (+0xC/+0x10) by `speed` (scaled 1.5x/2x at frame delta 3/4), then runs
   the contact/collision checks: actor contact over a `contactRadius` sphere
   (MOVE_PASS_ACTORS pushes the step out of them instead of failing), an
   optional +MOVE_LIFT_HEIGHT + radius Z bias around the world collision (flags
   bit 0, undone after), world collision over `radius` (flags bit 5 slides to
   the resolved contact), a ground probe (flags bits 2/4/6), a step-height
   reject >= 0x191 (bit 4), and a probe-normal slope reject within 0x17 of
   vertical (bit 6). Returns 1 on actor contact, 2 on world/step/slope
   rejection, 0 when the move was committed. */

int MoveActorAlongHeading(Actor *actor, int speed, int contactRadius,
                          int radius, int flags) {
  int newpos[3];
  int ground;
  int t;
  int floorz;
  int step;
  int ang;
  int snapz;
  int d;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  newpos[0] = actor->posX + ((g_anCosineLut[actor->rotZ] * speed) >> 12);
  newpos[1] = actor->posY + ((g_anSineLut[actor->rotZ] * speed) >> 12);
  newpos[2] = actor->posZ;
  if (contactRadius != 0) {
    if (!(flags & MOVE_PASS_ACTORS)) {
      if (DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor,
                                       CONTACT_STOP_AT_SOLID) != 0) {
        return MOVED_HIT_ACTOR;
      }
    } else {
      DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor,
                                   CONTACT_PUSH_OUT | CONTACT_ALL);
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] + MOVE_LIFT_HEIGHT;
    newpos[2] = t + radius;
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      if (!(flags & MOVE_SLIDE)) {
        return MOVED_BLOCKED;
      }
      CopyVector(newpos, g_anCollisionResolvedPos);
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] - MOVE_LIFT_HEIGHT;
    newpos[2] = t - radius;
  }
  if (flags & MOVE_PROBES_FLOOR) {
    newpos[2] += GROUND_PROBE_LIFT;
    ground = FindGroundHeightBelow(newpos, 0x1388);
  }
  if (flags & MOVE_STEP_LIMIT) {
    floorz = ground + actor->floorOffset;
    step = floorz - actor->posZ;
    if (step > 0) {
      if (step >= 0x191) {
        goto blocked;
      }
    } else if (actor->posZ - floorz >= 0x191) {
      goto blocked;
    }
  }
  if (flags & MOVE_SLOPE_LIMIT) {
    t = VectorLength(g_anCollisionHitNormal, VEC_LEN_XY);
    ang = ArcTan2(t, g_anCollisionHitNormal[2], ATAN_ANGLE8);
    if (ang >= 0x81) {
      ang -= 0x100;
    }
    if (abs(ang) < 0x17) {
    blocked:
      return MOVED_BLOCKED;
    }
  }
  actor->posX = newpos[0];
  actor->posY = newpos[1];
  if (flags & MOVE_FOLLOW_FLOOR) {
    snapz = ground + actor->floorOffset;
    d = snapz - actor->posZ;
    if (abs(d) >= 0x259) {
      if (d > 0) {
        actor->posZ = actor->posZ + 0xFA;
      } else {
        actor->posZ = actor->posZ - 0xFA;
      }
    } else {
      actor->posZ = snapz;
    }
  }
  if (flags & MOVE_PROBES_FLOOR) {
    EncodeHitNormalToActorGroundCode(actor);
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return MOVED_OK;
}

/* Guided-move step for a tracked actor (0x80039688, 0x288). Advances the
   actor's cached position (+0xC/+0x10) along `angle` by `speed` (scaled
   1.5x/2x at frame delta 3/4), then runs the same contact/collision checks
   as MoveActorByDelta: actor contact over a `contactRadius` sphere
   (MOVE_PASS_ACTORS pushes the step out of them instead of failing), world
   collision over `radius` (flags bit 5 slides to the resolved contact instead
   of aborting), and a ground probe (flags bits 2/4) that either rejects steps
   >= 0xC9 above/below the floor (bit 4) or snaps/eases Z toward
   floor+0x38-offset (bit 2). Returns 1 on actor contact, 2 on world/step
   rejection, 0 when the move was committed. */

int MoveActorAlongAngle(Actor *actor, int angle, int speed, int contactRadius,
                        int radius, int flags) {
  int newpos[3];
  int ground;
  int t;
  int floorz;
  int step;
  int d;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  newpos[0] = actor->posX + ((g_anCosineLut[angle] * speed) >> 12);
  newpos[1] = actor->posY + ((g_anSineLut[angle] * speed) >> 12);
  newpos[2] = actor->posZ;
  if (contactRadius != 0) {
    if (!(flags & MOVE_PASS_ACTORS)) {
      if (DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor,
                                       CONTACT_STOP_AT_SOLID) != 0) {
        return MOVED_HIT_ACTOR;
      }
    } else {
      DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor,
                                   CONTACT_PUSH_OUT | CONTACT_ALL);
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] + MOVE_LIFT_HEIGHT;
    newpos[2] = t + radius;
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      if (!(flags & MOVE_SLIDE)) {
        return MOVED_BLOCKED;
      }
      CopyVector(newpos, g_anCollisionResolvedPos);
    }
  }
  if (flags & (MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
    newpos[2] += GROUND_PROBE_LIFT;
    ground = FindGroundHeightBelow(newpos, 0x1388);
  }
  if (flags & MOVE_STEP_LIMIT) {
    floorz = ground + actor->floorOffset;
    step = floorz - actor->posZ;
    if (step > 0) {
      if (step >= 0xC9) {
        goto blocked;
      }
      goto commit;
    }
    if (actor->posZ - floorz >= 0xC9) {
    blocked:
      return MOVED_BLOCKED;
    }
  }
commit:
  actor->posX = newpos[0];
  actor->posY = newpos[1];
  if (flags & MOVE_FOLLOW_FLOOR) {
    d = (ground + actor->floorOffset) - actor->posZ;
    newpos[2] += 0x400;
    if (abs(d) >= 0x259) {
      if (d > 0) {
        actor->posZ = actor->posZ + 0xFA;
      } else {
        actor->posZ = actor->posZ - 0xFA;
      }
    } else {
      actor->posZ = ground + actor->floorOffset;
    }
  }
  if (flags & MOVE_PROBES_FLOOR) {
    EncodeHitNormalToActorGroundCode(actor);
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return MOVED_OK;
}

/* One frame of ballistic actor motion (0x80039910, 0x198): a knock-back,
   hop or leap. Picks the probe mode/distance from the actor's probeDist
   byte and, while *speed is nonzero, steps the actor along `heading`
   (MoveActorAlongAngle), then bleeds *speed by `friction` (to 0). Unless vz
   is null or *vz is 0xFFFF (no vertical motion), adds *vz to the actor's Z
   and pulls it down by `gravity`, clamped at -0x104; once *vz is <= 0 the
   actor snaps up to the ground plus floorOffset. Returns the move-step
   result, or MOVED_LANDED when the floor snap landed. */
int DriveActorTrackedMove(Actor *actor, int *speed, int heading, int *vz,
                          int friction, int gravity) {
  int ret;
  int mode;
  int dist;
  int b;
  int t;
  int ground;

  ret = MOVED_OK;
  mode = MOVE_SLIDE | MOVE_LIFT;
  dist = 0x12C;
  if (vz == 0 || *vz == 0xFFFF) {
    mode = MOVE_SLIDE | MOVE_FOLLOW_FLOOR | MOVE_LIFT;
  }
  b = actor->probeDist;
  if (b == ACTOR_PROBE_LIFT_ONLY) {
    mode = MOVE_LIFT;
  } else if (b != 0) {
    mode &= ~MOVE_SLIDE;
    dist = b << 2;
    if (b == ACTOR_PROBE_NONE) {
      dist = 0;
    }
  }
  if (*speed != 0) {
    ret = MoveActorAlongAngle(actor, heading, *speed, dist, 0x1F4, mode);
    t = *speed - friction;
    *speed = t;
    if (t < 0) {
      *speed = 0;
    }
  }
  if (ret == MOVED_BLOCKED) {
    return MOVED_BLOCKED;
  }
  if (vz != 0 && *vz != 0xFFFF) {
    ground = ProbeActorGroundHeightRaised(actor, 0x258);
    actor->posZ += *vz;
    *vz -= gravity;
    if (*vz < -0x104) {
      *vz = -0x104;
    }
    if (*vz <= 0) {
      t = ground + actor->floorOffset;
      if (actor->posZ < t) {
        actor->posZ = t;
        ret = MOVED_LANDED;
      }
    }
    EncodeHitNormalToActorGroundCode(actor);
  }
  return ret;
}

/* Wander/patrol behaviour step for an actor (0x80039AA8, 0x3ec).

   `wb` is the actor's WanderBlock (actor_type.h): home point, speed/turn
   rate, re-pick/jitter ranges, leash and chase radii, weave and cooldowns.

   Each frame: age the re-pick countdown and, on expiry, jitter the heading by
   a random signed amount and re-arm the timer; add the weave offset (bouncing
   the weave direction at the amplitude limit); turn toward the resulting
   heading and take a guided-move step. On a blocked step, pick a fresh
   backwards-ish heading. Otherwise chase Spyro when he is inside 0x44C (or
   inside chaseRadius), else walk back home once past the leash, else just
   tick the two cooldowns down. */
void UpdateActorWander(Actor *actor, WanderBlock *wb) {
  int unused[2];
  int *spyro;
  int turn;
  int speed;
  int aligned;
  unsigned char timer;
  int jitter;
  int heading;
  int weave;
  int amp;
  int dist;
  int home;
  int hit;

  turn = wb->turnRate;
  speed = wb->speed;
  aligned = 1;
  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
    turn += turn >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
    turn <<= 1;
  }

  timer = wb->repickTimer;
  wb->repickTimer = timer + 0xFF; /* wrapping decrement */
  if (timer == 0) {
    jitter = RandomIntInRange(wb->jitterMin, wb->jitterMax);
    if (GetRandomU32() & 1) {
      jitter = -jitter;
    }
    wb->heading = (wb->heading + jitter + 0x100) % 0x100;
    wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
    wb->weave = 0;
    wb->chasing = 0;
  }

  heading = wb->heading;
  if (wb->weaveAmp != 0) {
    weave = wb->weave;
    heading += weave;
    weave += wb->weaveRate * wb->weaveDir;
    wb->weave = weave;
    amp = wb->weaveAmp;
    weave = (short)weave;
    if (weave < 0) {
      weave = -weave;
    }
    if (weave >= amp) {
      wb->weaveDir = -(int)wb->weaveDir;
    }
  }
  if (TurnActorYawTowardAngle(actor, heading, turn, 5, YAW_STEP_IN_BAND) == 0) {
    aligned = 0;
  }

  if (wb->contactCooldown != 0) {
    hit = MoveActorAlongHeading(actor, speed, 0, wb->contact,
                                MOVE_LIFT | MOVE_FOLLOW_FLOOR |
                                    MOVE_STEP_LIMIT | MOVE_SLOPE_LIMIT);
  } else {
    hit = MoveActorAlongHeading(actor, speed, wb->contact, wb->contact,
                                MOVE_LIFT | MOVE_FOLLOW_FLOOR |
                                    MOVE_STEP_LIMIT | MOVE_SLOPE_LIMIT);
  }

  if (hit != 0) {
    if (aligned != 0) {
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(0x40, 0xC0));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 0;
      if (hit & 2) {
        wb->chaseCooldown = 0x14;
      } else {
        wb->contactCooldown = 6;
      }
    }
  } else {
    spyro = g_anSpyroWorldPos;
    dist = ApproxDist2D(&actor->posX, spyro);
    if (dist < 0x44C && wb->chasing == 0) {
      wb->heading = ArcTan2(actor->posX - spyro[0],
                            actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(-0x3C, 0x3C));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 1;
      return;
    }
    if (wb->chaseRadius != 0 && wb->chasing == 0 && wb->chaseCooldown == 0 &&
        dist < (wb->chaseRadius << 10)) {
      wb->heading = ArcTan2(actor->posX - g_anSpyroWorldPos[0],
                            actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
      wb->heading = WrapAddAngle8(wb->heading, RandomIntInRange(-0x30, 0x30));
      wb->repickTimer = RandomIntInRange(wb->repickMin, wb->repickMax);
      wb->chasing = 1;
      return;
    }
    home = ApproxDist2D(&actor->posX, &wb->homeX);
    if ((wb->leash << 10) < home) {
      wb->heading = ArcTan2(wb->homeX - actor->posX, wb->homeY - actor->posY,
                            ATAN_ANGLE8);
      wb->chasing = 0;
      wb->chaseCooldown = 0x14;
      wb->repickTimer = home / (int)wb->speed;
    } else {
      if (wb->chaseCooldown != 0) {
        wb->chaseCooldown = wb->chaseCooldown - 1;
      }
      if (wb->contactCooldown != 0) {
        wb->contactCooldown = wb->contactCooldown - 1;
      }
    }
  }
}

/* Waypoint path-follow driver (0x80039e94, 0x2d8). Steers the actor
   toward its current path node (path->cur = node index, 16-byte node
   records at +8/+0xC/+0x10). On arrival (2D distance and, with flags
   bit 8, Z distance within `thresh`) advances the node index — forward
   with wrap when the path's +6 halfword is -1, backward with wrap to
   path->count-1 otherwise — and reports it as 0x100+index in the return.
   When the heading delta to the node exceeds `lim`, turns in place via
   ApproachAngle8 (deferring the turn when another live actor whose group is
   `turnGroup` is walking; 0xFF = no gating); otherwise marks the actor
   walking and delegates the step to TurnActorYawTowardAngle +
   MoveActorAlongHeading. */

/* Turn the actor's yaw toward `ang`, at most `step` per frame. */
#define TurnActorTowardAngle(actor, ang, step, lim)                            \
  do {                                                                         \
    actor->rotZ = ApproachAngle8(ang, actor->rotZ, step, lim);                 \
  } while (0)

int PivotActorAlongPath(Actor *actor, PathData *path, int thresh, int speed,
                        int contactRadius, int step, int lim, int turnGroup,
                        int flags) {
  int ret;
  int zclose;
  int ang;
  int idx;
  int wrap;
  int *node;
  Actor **list;
  Actor *peer;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }

  ret = 0;
  if (flags & MOVE_CHECK_Z) {
    zclose = abs(actor->posZ - path->nodes[path->cur].z) < thresh;
  } else {
    zclose = 1;
  }

  if (ApproxDist2D(&actor->posX, &path->nodes[path->cur].x) < thresh &&
      zclose != 0) {
    actor->subMode = PIVOT_SUB_ARRIVED;
    if (path->dir == -1) {
      wrap = path->count;
      idx = path->cur + 1;
      path->cur = idx;
      if ((idx & 0xFF) == wrap) {
        path->cur = 0;
      }
    } else {
      wrap = 0xFF;
      idx = (path->cur = path->cur - 1);
      if ((idx & 0xFF) == wrap) {
        path->cur = path->count - 1;
      }
    }
    ret = PATH_ARRIVED(path->cur);
  }

  node = (int *)((char *)path + (path->cur << 4)); /* node x/y at [2]/[3] */
  ang = ArcTan2(node[2] - actor->posX, node[3] - actor->posY, ATAN_ANGLE8);

  if (AbsAngleDelta8(ang, actor->rotZ) <= lim) {
    actor->subMode = PIVOT_SUB_WALKING;
    TurnActorYawTowardAngle(actor, ang, step, lim, YAW_STEP_IN_BAND);
    MoveActorAlongHeading(actor, speed, contactRadius, 0, flags);
    goto done;
  }

  if (actor->subMode == PIVOT_SUB_ARRIVED && turnGroup != PIVOT_TURN_ANY) {
    /* A freshly arrived actor starts its turn only while no peer of its turn
       group is walking (PIVOT_SUB_WALKING). */
    list = g_apActiveActorListBlock;
    while ((peer = *(list++)) != 0) {
      if (peer->group == turnGroup &&
          (unsigned int)peer->mode < ACTOR_MODE_DORMANT &&
          peer->subMode == PIVOT_SUB_WALKING) {
        break;
      }
    }
    if (peer != 0) {
      goto done;
    }
    TurnActorTowardAngle(actor, ang, step, lim);
    actor->subMode = PIVOT_SUB_TURNING;
  } else {
    TurnActorTowardAngle(actor, ang, step, lim);
  }

done:
  return ret;
}

#undef TurnActorTowardAngle

/* Path-follow steering step (0x8003a16c, 0x2b4 bytes). Moves `actor` toward
 * waypoint path->cur of `path` (u8 count, u8 index, 16-byte waypoints at +8):
 * builds a byte euler {0, pitch-to-waypoint, yaw} where the yaw chases the
 * waypoint bearing, stepped at most +/-`clamp` per tick (movement stalls when
 * the remaining turn exceeds `limit`), advances by `speed` (capped at the
 * remaining distance) through the rotated basis, optionally applies a contact
 * probe of radius `contact`, then snaps to the ground within 0x400 below.
 * Within `radius` of the waypoint: clears the actor's subMode latch, advances
 * (and wraps) the waypoint index, and returns index + 0x100; else returns 0.
 * `d` carries the turn delta and is reused for the ground height. */
int SteerActorAlongWaypoints(Actor *actor, PathData *path, int radius,
                             int speed, int contact, int clamp, int limit,
                             int *yawp) {
  char eul[8];
  int vec[4];
  int d;
  int mtx[8];
  int ret;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), vec[2], ATAN_ANGLE8);
  if (VectorLength(vec, VEC_LEN_XY) < radius) {
    eul[2] = *yawp;
  } else {
    int yaw;
    d = ArcTan2(vec[0], vec[1], ATAN_ANGLE8);
    yaw = *yawp;
    d = (d - yaw) & ANGLE8_MASK;
    if (d >= 0x81) {
      d -= 0x100;
    }
    if (limit < abs(d)) {
      speed = 0;
    }
    if (d < (-clamp)) {
      d = -clamp;
    }
    if (clamp < d) {
      d = clamp;
    }
    eul[2] = d + yaw;
    *yawp = (d + yaw) & 0xFF;
  }
  {
    int len = VectorLength(vec, VEC_LEN_XYZ);
    if (len < speed) {
      speed = len;
    }
  }
  ApplyEulerRotation(eul, mtx, 0);
  vec[0] = speed;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  AddVector(vec, &actor->posX, vec);
  if (contact != 0) {
    DispatchActorContactAtSphere(vec, contact, 0, 0, actor,
                                 CONTACT_STOP_AT_SOLID);
  }
  vec[2] += GROUND_PROBE_LIFT;
  d = FindGroundHeightBelow(vec, 0x400);
  if (d != 0) {
    vec[2] = d;
  } else {
    vec[2] -= 0x400;
  }
  CopyVector(&actor->posX, vec);
  EncodeHitNormalToActorGroundCode(actor);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ret = 0;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  if (VectorLength(vec, VEC_LEN_XYZ) < radius) {
    actor->subMode = 0;
    path->cur += 1;
    if (path->cur == path->count) {
      path->cur = 0;
    }
    ret = PATH_ARRIVED(path->cur);
  }
  return ret;
}

/* 0x8003A420 (size 0x300): actor waypoint-follow step. Steers an actor toward
 * its current waypoint (path->nodes[path->cur]), clamps the
 * per-frame turn to +/-maxTurn, moves by the (matrix-rotated) capped speed,
 * resolves ground contact, then advances to the next waypoint when close
 * enough.
 *
 * The ground-probe result is a `$3` register local: as a pseudo its call-result
 * copy gives it a hard v0 preference, find_reg takes v0, and jump.c then
 * cross-jumps the two `vec[2] =` stores (-3 insns).  `ret = 0` sits ABOVE the
 * last SubtractVector so ret's range overlaps arriveDist's and it takes a fresh
 * callee-saved (s4) instead of sharing arriveDist's s1.  `abs()` is the builtin
 * (A160). */

int WalkActorAlongWaypoints(Actor *actor, PathData *path, int arriveDist,
                            int cap, int contactRadius, int maxTurn,
                            int stallAngle, int *smoothVel) {
  unsigned char eul[3];
  int vec[3];
  int mtx[8];
  int len;
  int base;
  int delta;
  int absv;
  int ret;

  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, VEC_LEN_XY), vec[2], ATAN_ANGLE8);
  if (VectorLength(vec, VEC_LEN_XY) < arriveDist) {
    eul[2] = actor->rotZ;
  } else {
    delta = ArcTan2(vec[0], vec[1], ATAN_ANGLE8);
    base = actor->rotZ;
    delta = (delta - base) & 0xFF;
    if (delta >= 0x81) {
      delta -= 0x100;
    }
    absv = abs(delta);
    if (stallAngle < absv) {
      cap = 0;
    }
    if (delta < -maxTurn) {
      delta = -maxTurn;
    }
    if (maxTurn < delta) {
      delta = maxTurn;
    }
    eul[2] = base + delta;
    actor->rotZ = base + delta;
  }
  len = VectorLength(vec, VEC_LEN_XYZ);
  if (len < cap) {
    cap = len;
  }
  ApplyEulerRotation(eul, mtx, 0);
  vec[0] = cap;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  if (smoothVel != 0) {
    AddVector(smoothVel, smoothVel, vec);
    CopyVector(vec, smoothVel);
    RShiftVector3(vec, 2);
    SubtractVector(smoothVel, smoothVel, vec);
    AddVector(vec, &actor->posX, smoothVel);
  } else {
    AddVector(vec, &actor->posX, vec);
  }
  if (contactRadius != 0) {
    DispatchActorContactAtSphere(vec, contactRadius, 0, 0, actor,
                                 CONTACT_STOP_AT_SOLID);
  }
  vec[2] += GROUND_PROBE_LIFT;
  {
    register int ground asm("$3");
    ground = FindGroundHeightBelow(vec, 0x400);
    if (ground != 0) {
      vec[2] = ground;
    } else {
      vec[2] -= 0x400;
    }
  }
  CopyVector(&actor->posX, vec);
  EncodeHitNormalToActorGroundCode(actor);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ret = 0;
  SubtractVector(vec, &path->nodes[path->cur].x, &actor->posX);
  if (VectorLength(vec, VEC_LEN_XYZ) < arriveDist) {
    actor->subMode = 0;
    path->cur = path->cur + 1;
    if ((path->cur & 0xFF) == path->count) {
      path->cur = 0;
    }
    ret = path->cur + 0x100;
  }
  return ret;
}

/* Reset an actor's animation, render and activation fields to their defaults
   (0x8003a720, 0x7c): anim/frame state cleared with frameRate 0x20 and
   nextFrame 1, euler bytes and mode/subMode zeroed, flags/groundCode/tint
   words cleared, otBias 4, renderList 0x20, cullRadius 0x10, group, gateChunk,
   updateDist and dropKind 0xFF (gateChunk 0xFF = always drawn, updateDist the
   widest activation radius, dropKind the random gem roll) and voiceRow 0x7F. */
void InitActorRenderDefaults(Actor *actor) {
  actor->cullRadius = CULL_RADIUS(0x1000);
  actor->updateDist = ACTOR_UPDATE_ALWAYS;
  actor->otBias = OT_BIAS_DEFAULT;
  actor->nextFrame = 1;
  actor->mode = ACTOR_SPAWN_MODE;
  actor->subMode = 0;
  actor->anim = ACTOR_SPAWN_ANIM;
  actor->nextAnim = ACTOR_SPAWN_ANIM;
  actor->frame = 0;
  actor->frameProgress = 0;
  actor->frameRate = ANIM_RATE_TICKS(2);
  actor->group = 0xff;
  actor->gateChunk = GATE_CHUNK_ALWAYS;
  actor->dropKind = DROP_GEM_ROLL;
  actor->flags = 0;
  actor->rotX = 0;
  actor->rotY = 0;
  actor->rotZ = 0;
  actor->groundCode = 0;
  actor->scale = 0;
  actor->voiceRow = VOICE_ROW_NONE;
  actor->floorOffset = 0;
  actor->renderList = 0x20;
  *(int *)&actor->tint0 = 0;
}

/* 0x8003a79c: the distance from the actor to its path's current node,
   divided by divisor.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
int MeasureOffsetToPathPoint(Actor *actor, PathData *path, int divisor) {
  int v[3];

  v[0] = path->nodes[path->cur].x - actor->posX;
  v[1] = path->nodes[path->cur].y - actor->posY;
  v[2] = path->nodes[path->cur].z - actor->posZ;

  return VectorLength(v, VEC_LEN_XYZ) / divisor;
}

/* 0x8003a850 — clamp the record's step counter (byte +2) from byte +3,
   then rebuild its offset vector: copy, scale by half the counter, add the
   actor anchor (+0xC), subtract the indexed sub-record vector, and blend
   by the remaining step fraction.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
void StepTweenOffsetVector(Actor *actor, unsigned char *tween, int *out,
                           int *base) {
  unsigned int t;

  t = tween[3] & 0xFE;
  tween[2] = t;
  if (t >= 9) {
    tween[2] = 8;
  }

  CopyVector(base, out);
  ScaleVector3Sat(out, base, tween[2] >> 1);
  AddVector(out, out, &actor->posX);
  SubtractVector(out, (int *)(tween + ((tween[1] << 4) + 8)), out);
  DivideVector3(out, out, tween[3] - (tween[2] >> 1));
}

/* 0x8003a920 — blend two vectors by a per-mode weight pair: pick the
   weight table from the record's mode byte (+2), scale vecA/vecB by the
   idx'th pair, sum into out, then >>10 to renormalize.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
void BlendVectorPairByMode(int *out, unsigned char *rec, int idx, int *vecA,
                           int *vecB) {
  int va[4];
  int vb[4];
  short *tbl;
  short *entry;
  unsigned char sel;

  sel = rec[2];
  if (sel == 2) {
    tbl = g_anTweenBlendWeights2;
  } else if (sel == 4) {
    tbl = g_anTweenBlendWeights4;
  } else {
    tbl = g_anTweenBlendWeights8;
    if (sel == 6) {
      tbl = g_anTweenBlendWeights6;
    }
  }

  entry = (short *)((idx << 2) + (int)tbl);
  ScaleVector3Sat(va, vecA, entry[0]);
  ScaleVector3Sat(vb, vecB, entry[1]);
  AddVector(out, va, vb);
  RShiftVector3(out, 0xA);
}

/* Steer actor `actor` toward world point `target` and advance it (0x8003bad0).
   Computes the target yaw/pitch from the delta vector, wraps each 8-bit euler
   delta to signed and clamps it to +/-yawLim / +/-pitchLim, applies the turn to
   rotY/rotZ, then moves the actor `speed` units along its
   new facing. With a nonzero `radius` it resolves sphere collision (snapping
   to g_anCollisionResolvedPos) and dispatches contact callbacks; returns 1 if
   the move was blocked/adjusted, else 0. */

int SteerActorTowardPoint(Actor *actor, int *target, int speed, int yawLim,
                          int pitchLim, int radius) {
  int dv[3];
  int mtx[8];
  int moved;
  int *pos;
  int yaw;
  int pitch;
  int heading;

  moved = 0;
  pos = &actor->posX;
  SubtractVector(dv, target, pos);
  yaw = ArcTan2(dv[0], dv[1], ATAN_ANGLE8);
  pitch = ArcTan2(VectorLength(dv, VEC_LEN_XY), dv[2], ATAN_ANGLE8);

  heading = actor->rotZ;
  yaw = (yaw - heading) & ANGLE8_MASK;
  if (yaw >= 0x80) {
    yaw -= 0x100;
  }
  if (yaw < -yawLim) {
    yaw = -yawLim;
  }
  if (yawLim < yaw) {
    yaw = yawLim;
  }

  pitch = (pitch - actor->rotY) & ANGLE8_MASK;
  if (pitch >= 0x80) {
    pitch -= 0x100;
  }
  if (pitch < -pitchLim) {
    pitch = -pitchLim;
  }
  if (pitchLim < pitch) {
    pitch = pitchLim;
  }

  actor->rotZ = heading + yaw;
  actor->rotY = actor->rotY + pitch;
  ApplyEulerRotation(&actor->rotX, mtx, 0);
  dv[0] = speed;
  dv[1] = 0;
  dv[2] = 0;
  RotateVectorByMatrix(mtx, dv, dv);
  AddVector(pos, pos, dv);

  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(pos, radius, radius) != 0) {
      CopyVector(pos, g_anCollisionResolvedPos);
      moved = 1;
    }
    if (DispatchActorContactAtSphere(pos, radius, 0, 0, actor,
                                     CONTACT_PUSH_OUT) != 0) {
      moved = 1;
    }
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ProbeActorGroundHeight(actor);
  EncodeHitNormalToActorGroundCode(actor);
  return moved;
}

/* Trial-move actor `actor` `speed` units along its facing and commit the new
   position if nothing blocks it (0x8003bccc). Global pace g_nActorFrameDelta
   scales speed (3 -> 1.5x, 4 -> 2x). flags: bit 0 biases the probe Z up by
   MOVE_LIFT_HEIGHT + collideR for the contact tests (undone after), bit 1
   selects the fire-and-forget contact dispatch (mode 3) over the blocking one,
   bits 2/4 (0x14) probe the ground below, bit 4 also rejects a commit when the
   ground height differs from the actor's cached Z by >= 0xC9. Returns 1 on
   contact block, 2 on world collision / ground-step reject, 0 on commit. */

int TryMoveActorForward(Actor *actor, int speed, int contactR, int collideR,
                        int flags) {
  int mtx[8];
  int vec[3];
  int ground;
  int d;

  if (g_nActorFrameDelta == 3) {
    speed += speed >> 1;
  } else if (g_nActorFrameDelta == 4) {
    speed <<= 1;
  }
  ApplyEulerRotation(&actor->rotX, mtx, 0);
  vec[0] = speed;
  vec[2] = 0;
  vec[1] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  AddVector(vec, vec, &actor->posX);

  if (flags & MOVE_LIFT) {
    d = vec[2] + MOVE_LIFT_HEIGHT;
    vec[2] = d + collideR;
  }
  if (contactR != 0) {
    if ((flags & MOVE_PASS_ACTORS) == 0) {
      if (DispatchActorContactAtSphere(vec, contactR, 0, 0, actor,
                                       CONTACT_STOP_AT_SOLID) != 0) {
        return 1;
      }
    } else {
      DispatchActorContactAtSphere(vec, contactR, 0, 0, actor,
                                   CONTACT_PUSH_OUT | CONTACT_ALL);
    }
  }
  if (collideR != 0) {
    if (CollideSphereWithWorldAndActors(vec, collideR, collideR) != 0) {
      goto blocked;
    }
  }
  if (flags & MOVE_LIFT) {
    d = vec[2] - MOVE_LIFT_HEIGHT;
    vec[2] = d - collideR;
  }
  if (flags & (MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
    vec[2] += GROUND_PROBE_LIFT;
    ground = FindGroundHeightBelow(vec, 0x800);
  }
  if (flags & MOVE_STEP_LIMIT) {
    d = ground - actor->posZ;
    if (d > 0) {
      if (d >= 0xC9) {
        goto blocked;
      }
      goto gdone;
    }
    if (actor->posZ - ground < 0xC9) {
      goto gdone;
    }
  blocked:
    return 2;
  }
gdone:
  CopyVector(&actor->posX, vec);
  EncodeHitNormalToActorGroundCode(actor);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return 0;
}

/* 0x8003bed8 — classify the actor's position relative to Spyro into a
   quadrant mask: compute actor(+0xC) - Spyro (optionally un-rotated by the
   actor's yaw byte rotZ), then AND away the halves of 0xF that the
   X/Y signs rule out.
   Unreferenced: no call, branch, pointer or table entry in main or any overlay
   (xref). */
int ClassifyActorQuadrantVsSpyro(Actor *actor) {
  int mtx[8];
  signed char euler[8];
  int delta[4];
  int mask;

  mask = 0xF;
  if (actor->rotZ != 0) {
    euler[0] = 0;
    euler[1] = 0;
    euler[2] = -actor->rotZ;
    ApplyEulerRotation(euler, mtx, 0);
    SubtractVector(delta, &actor->posX, g_anSpyroWorldPos);
    RotateVectorByMatrix(mtx, delta, delta);
  } else {
    SubtractVector(delta, &actor->posX, g_anSpyroWorldPos);
  }

  if (delta[0] > 0) {
    mask &= 6;
  } else {
    mask &= 9;
  }
  if (delta[1] > 0) {
    mask &= 3;
  } else {
    mask &= 0xC;
  }
  return mask;
}

/* 0x8003bfc0 — advance an actor one sub-step along its waypoint ring.
   `path` is the waypoint block (count byte [0], cursor byte [1], vec3s at
   +8 + i*0x10) and `step` the sub-step counter within the current leg, which
   takes `legFrames` frames. Once the counter reaches the leg length the cursor
   advances (wrapping to 0, returning 2 instead of 1) and the raw leg vector
   is published in `prev`. Otherwise the remaining leg is divided into the
   frames still to run; for the first steps of a leg that share of the new
   leg is cross-faded with `prev` through the per-length weight table
   (BlendVectorPairByMode's tables). Flags bit 2 keeps the vertical component.
   The step vector is folded into the actor position and its yaw/pitch bytes
   (+0x46/+0x45) are re-aimed along it.
   Match notes: the empty do/while(0) keeps the idx load ahead of the *step
   load (the original's load-delay nop); `ret++` precedes the cursor test so
   reorg puts it in the branch delay slot; the divisor is a `$6` register
   local (the original's a2). */
int TweenActorAlongPath(Actor *actor, PathData *path, int *prev, int *step,
                        int legFrames, int flags) {
  int dir[4];
  int blend[4];
  short *tbl;
  int *pos;
  int idx;
  int t;
  int lim;
  int x;
  int n;
  register int span asm("$6");
  int ret;

  ret = 0;
  if (legFrames >= 9) {
    tbl = g_anTweenBlendWeights8;
    lim = 7;
  } else if (legFrames >= 7) {
    tbl = g_anTweenBlendWeights6;
    lim = 5;
  } else {
    lim = 1;
    if (legFrames >= 5) {
      tbl = g_anTweenBlendWeights4;
      lim = 3;
    } else {
      tbl = g_anTweenBlendWeights2;
    }
  }

  idx = path->cur;
  EBB_BREAK();
  t = *step;
  if (t >= legFrames) {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    CopyVector(prev, dir);
    ret++;
    if (++path->cur >= path->count) {
      path->cur = 0;
      ret++;
    }
    *step = 0;
  } else if (lim < t) {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    n = *step - 1;
    span = legFrames - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    *step = *step + 1;
  } else {
    SubtractVector(dir, &path->nodes[idx].x, &actor->posX);
    n = *step - 1;
    span = legFrames - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    ScaleVector3Sat(dir, dir, ((short *)((*step << 2) + (int)tbl))[0]);
    ScaleVector3Sat(blend, prev, ((short *)((*step << 2) + (int)tbl))[1]);
    AddVector(dir, dir, blend);
    RShiftVector3(dir, 0xa);
    *step = *step + 1;
  }

  if (!(flags & TWEEN_FLY)) {
    dir[2] = 0;
  }
  actor->rotZ = ArcTan2_8bit(dir[0], dir[1]);
  x = ArcTan2_8bit(dir[2], VectorLength(dir, VEC_LEN_XY));
  pos = &actor->posX;
  actor->rotY = -x + 0x40;
  AddVector(pos, pos, dir);
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return ret;
}

/* 0x8003c994 (0x130) — advance an actor along its waypoint path record:
   measure the distance from the actor to the current waypoint (path index
   byte +0x1, 0x10-byte waypoint records from +0x8); when closer than
   `threshold`, step the index (+0x6 halfword == -1 selects forward wrap
   [0..count), else backward wrap to count-1); then steer toward the (possibly
   new) waypoint with speed = dist>>4 clamped to `cap`. */
void FollowActorWaypointPath(Actor *actor, PathData *path, int threshold,
                             int cap, int radius, int yawLim, int pitchLim) {
  int v[3];
  int dist;

  SubtractVector(v, &actor->posX, &path->nodes[path->cur].x);
  dist = VectorLength(v, VEC_LEN_XYZ);
  if (dist < threshold) {
    if (path->dir == -1) {
      path->cur += 1;
      if (path->cur == path->count) {
        path->cur = 0;
      }
    } else {
      path->cur -= 1;
      if (path->cur == 0xFF) {
        path->cur = path->count - 1;
      }
    }
  }

  dist = dist >> 4;
  if (cap < dist) {
    dist = cap;
  }
  SteerActorTowardPoint(actor, &path->nodes[path->cur].x, dist, yawLim,
                        pitchLim, radius);
}
