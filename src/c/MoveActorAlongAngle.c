#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Guided-move step for a tracked actor (0x80039688, 0x288). Advances the
   actor's cached position (+0xC/+0x10) along `angle` by `speed` (scaled
   1.5x/2x at frame delta 3/4), then runs the same contact/collision checks
   as MoveActorByDelta: actor contact over a `contactRadius` sphere (flags bit 1
   makes it notify-only), world collision over `radius` (flags bit 5 slides to
   the resolved contact instead of aborting), and a ground probe (flags bits
   2/4) that either rejects steps >= 0xC9 above/below the floor (bit 4) or
   snaps/eases Z toward floor+0x38-offset (bit 2). Returns 1 on actor
   contact, 2 on world/step rejection, 0 when the move was committed. */

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
  if (flags & (MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
    newpos[2] += 0x400;
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
      return 2;
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
    EncodeCachedVecToActorDirCode(actor);
  }
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  return 0;
}
