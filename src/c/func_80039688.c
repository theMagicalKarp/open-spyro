#include "globals.h"

/* Guided-move step for a tracked actor (0x80039688, 0x288). Advances the
   actor's cached position (+0xC/+0x10) along `angle` by `speed` (scaled
   1.5x/2x in gamestates 3/4), then runs the same contact/collision checks
   as func_80039228: actor contact against `group` (flags bit 1 makes it
   notify-only), world collision over `radius` (flags bit 5 slides to the
   resolved contact instead of aborting), and a ground probe (flags bits
   2/4) that either rejects steps >= 0xC9 above/below the floor (bit 4) or
   snaps/eases Z toward floor+0x38-offset (bit 2). Returns 1 on actor
   contact, 2 on world/step rejection, 0 when the move was committed. */
extern int DispatchActorContactAtSphere(int *pos, int group, int a, int b,
                                        int *actor, int c);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern void CopyVector(int *dst, int *src);
extern int FindGroundHeightBelow(int *pt, int z);
extern void EncodeCachedVecToActorDirCode(int *actor);
extern void func_800529E4(int *actor, int mode);
extern int abs(int);

extern int D_800756C4;
extern short g_anSineLut[];
extern short g_anCosineLut[]; /* g_anSineLut + 0x80 */

int func_80039688(int *actor, int angle, int speed, int group, int radius,
                  int flags) {
  int newpos[3];
  int ground;
  int t;
  int floorz;
  int step;
  int d;

  if (D_800756C4 == 3) {
    speed += speed >> 1;
  } else if (D_800756C4 == 4) {
    speed <<= 1;
  }
  newpos[0] = actor[3] + ((g_anCosineLut[angle] * speed) >> 12);
  newpos[1] = actor[4] + ((g_anSineLut[angle] * speed) >> 12);
  newpos[2] = actor[5];
  if (group != 0) {
    if (!(flags & 2)) {
      if (DispatchActorContactAtSphere(newpos, group, 0, 0, actor, 0) != 0) {
        return 1;
      }
    } else {
      DispatchActorContactAtSphere(newpos, group, 0, 0, actor, 3);
    }
  }
  if (flags & 1) {
    t = newpos[2] + 0x12C;
    newpos[2] = t + radius;
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      if (!(flags & 0x20)) {
        return 2;
      }
      CopyVector(newpos, g_anCollisionResolvedPos);
    }
  }
  if (flags & 0x14) {
    newpos[2] += 0x400;
    ground = FindGroundHeightBelow(newpos, 0x1388);
  }
  if (flags & 0x10) {
    floorz = ground + *(short *)((char *)actor + 0x38);
    step = floorz - actor[5];
    if (step > 0) {
      if (step >= 0xC9) {
        goto blocked;
      }
      goto commit;
    }
    if (actor[5] - floorz >= 0xC9) {
    blocked:
      return 2;
    }
  }
commit:
  actor[3] = newpos[0];
  actor[4] = newpos[1];
  if (flags & 4) {
    d = (ground + *(short *)((char *)actor + 0x38)) - actor[5];
    newpos[2] += 0x400;
    if (abs(d) >= 0x259) {
      if (d > 0) {
        actor[5] = actor[5] + 0xFA;
      } else {
        actor[5] = actor[5] - 0xFA;
      }
    } else {
      actor[5] = ground + *(short *)((char *)actor + 0x38);
    }
  }
  if (flags & 0x54) {
    EncodeCachedVecToActorDirCode(actor);
  }
  func_800529E4(actor, 2);
  return 0;
}
