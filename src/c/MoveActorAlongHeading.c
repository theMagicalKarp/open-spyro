#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Guided-move step along the actor's own heading (0x80039398, 0x2f0).
   Sibling of MoveActorAlongAngle, but the direction comes from the actor's dir
   byte (+0x46) instead of an angle param. Advances the cached position
   (+0xC/+0x10) by `speed` (scaled 1.5x/2x at frame delta 3/4), then runs
   the contact/collision checks: actor contact over a `contactRadius` sphere
   (flags bit 1 makes it notify-only), an optional +0x12C+radius Z bias around
   the world collision (flags bit 0, undone after), world collision over
   `radius` (flags bit 5 slides to the resolved contact), a ground probe
   (flags bits 2/4/6), a step-height reject >= 0x191 (bit 4), and a
   probe-normal slope reject within 0x17 of vertical (bit 6). Returns 1
   on actor contact, 2 on world/step/slope rejection, 0 when the move
   was committed. */

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
      if (DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor, 0) !=
          0) {
        return 1;
      }
    } else {
      DispatchActorContactAtSphere(newpos, contactRadius, 0, 0, actor, 3);
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] + 0x12C;
    newpos[2] = t + radius;
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      if (!(flags & MOVE_SLIDE)) {
        return 2;
      }
      CopyVector(newpos, g_anCollisionResolvedPos);
    }
  }
  if (flags & MOVE_LIFT) {
    t = newpos[2] - 0x12C;
    newpos[2] = t - radius;
  }
  if (flags & MOVE_PROBES_FLOOR) {
    newpos[2] += 0x400;
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
    t = VectorLength(g_anCollisionProbeVec, VEC_LEN_XY);
    ang = ArcTan2(t, g_anCollisionProbeVec[2], ATAN_ANGLE8);
    if (ang >= 0x81) {
      ang -= 0x100;
    }
    if (abs(ang) < 0x17) {
    blocked:
      return 2;
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
    EncodeCachedVecToActorDirCode(actor);
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return 0;
}
