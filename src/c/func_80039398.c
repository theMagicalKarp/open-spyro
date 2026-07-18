#include "globals.h"

/* Guided-move step along the actor's own heading (0x80039398, 0x2f0).
   Sibling of func_80039688, but the direction comes from the actor's dir
   byte (+0x46) instead of an angle param. Advances the cached position
   (+0xC/+0x10) by `speed` (scaled 1.5x/2x in gamestates 3/4), then runs
   the contact/collision checks: actor contact against `group` (flags
   bit 1 makes it notify-only), an optional +0x12C+radius Z bias around
   the world collision (flags bit 0, undone after), world collision over
   `radius` (flags bit 5 slides to the resolved contact), a ground probe
   (flags bits 2/4/6), a step-height reject >= 0x191 (bit 4), and a
   probe-normal slope reject within 0x17 of vertical (bit 6). Returns 1
   on actor contact, 2 on world/step/slope rejection, 0 when the move
   was committed. */
extern int DispatchActorContactAtSphere(int *pos, int group, int a, int b,
                                        int *actor, int c);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern void CopyVector(int *dst, int *src);
extern int FindGroundHeightBelow(int *pt, int z);
extern void EncodeCachedVecToActorDirCode(int *actor);
extern void func_800529E4(int *actor, int mode);
extern int VectorLength(int *v, int mode);
extern int ArcTan2(int x, int y, int mode);
extern int abs(int);

extern int D_800756C4;
extern short g_anSineLut[];
extern short g_anCosineLut[]; /* g_anSineLut + 0x80 */

int func_80039398(int *actor, int speed, int group, int radius, int flags) {
  int newpos[3];
  int ground;
  int t;
  int floorz;
  int step;
  int ang;
  int snapz;
  int d;

  if (D_800756C4 == 3) {
    speed += speed >> 1;
  } else if (D_800756C4 == 4) {
    speed <<= 1;
  }
  newpos[0] =
      actor[3] +
      ((g_anCosineLut[*(unsigned char *)((char *)actor + 0x46)] * speed) >> 12);
  newpos[1] =
      actor[4] +
      ((g_anSineLut[*(unsigned char *)((char *)actor + 0x46)] * speed) >> 12);
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
  if (flags & 1) {
    t = newpos[2] - 0x12C;
    newpos[2] = t - radius;
  }
  if (flags & 0x54) {
    newpos[2] += 0x400;
    ground = FindGroundHeightBelow(newpos, 0x1388);
  }
  if (flags & 0x10) {
    floorz = ground + *(short *)((char *)actor + 0x38);
    step = floorz - actor[5];
    if (step > 0) {
      if (step >= 0x191) {
        goto blocked;
      }
    } else if (actor[5] - floorz >= 0x191) {
      goto blocked;
    }
  }
  if (flags & 0x40) {
    t = VectorLength(g_anCollisionProbeVec, 0);
    ang = ArcTan2(t, g_anCollisionProbeVec[2], 0);
    if (ang >= 0x81) {
      ang -= 0x100;
    }
    if (abs(ang) < 0x17) {
    blocked:
      return 2;
    }
  }
  actor[3] = newpos[0];
  actor[4] = newpos[1];
  if (flags & 4) {
    snapz = ground + *(short *)((char *)actor + 0x38);
    d = snapz - actor[5];
    if (abs(d) >= 0x259) {
      if (d > 0) {
        actor[5] = actor[5] + 0xFA;
      } else {
        actor[5] = actor[5] - 0xFA;
      }
    } else {
      actor[5] = snapz;
    }
  }
  if (flags & 0x54) {
    EncodeCachedVecToActorDirCode(actor);
  }
  func_800529E4(actor, 2);
  return 0;
}
