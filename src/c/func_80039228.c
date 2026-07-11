#include "globals.h"

/* Move an actor by a delta with contact/collision checks (0x80039228,
   0x170). In gamestate 3 the delta is scaled by 1.5x (half added), in
   state 4 doubled. flags bit 0 lifts the probe point by 0x12C+radius
   during the checks. Returns 1 on actor contact, 2 on world collision,
   0 when the move was committed. */
extern void CopyVector(int *dst, int *src);
extern void AddVector(int *dst, int *a, int *b);
extern void RShiftVector3(int *vec, int shift);
extern void LShiftVector3(int *vec, int shift);
extern int DispatchActorContactAtSphere(int *pos, int group, int a, int b,
                                        int *actor, int c);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern void func_800529E4(int *actor, int mode);

extern int D_800756C4;

typedef struct {
  int x;
  int y;
  int z;
} DeltaVec;

int func_80039228(int *actor, DeltaVec delta, int group, int radius,
                  int flags) {
  int newpos[4];
  int half[4];
  int *h;
  int *d;
  int t;

  if (D_800756C4 == 3) {
    h = half;
    d = (int *)&delta;
    CopyVector(h, d);
    RShiftVector3(h, 1);
    AddVector(d, d, h);
  } else if (D_800756C4 == 4) {
    LShiftVector3((int *)&delta, 1);
  }
  AddVector(newpos, actor + 3, (int *)&delta);
  if (flags & 1) {
    t = newpos[2] + 0x12C;
    newpos[2] = t + radius;
  }
  if (group != 0) {
    if (DispatchActorContactAtSphere(newpos, group, 0, 0, actor, 0) != 0) {
      return 1;
    }
  }
  if (radius != 0) {
    if (CollideSphereWithWorldAndActors(newpos, radius, radius) != 0) {
      return 2;
    }
  }
  if (flags & 1) {
    t = newpos[2] - 0x12C;
    newpos[2] = t - radius;
  }
  CopyVector(actor + 3, newpos);
  func_800529E4(actor, 2);
  return 0;
}
