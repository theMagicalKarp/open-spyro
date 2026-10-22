#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Trial-move actor `actor` `speed` units along its facing and commit the new
   position if nothing blocks it (0x8003bccc). Global pace g_nActorFrameDelta
   scales speed (3 -> 1.5x, 4 -> 2x). flags: bit 0 biases the probe Z up by
   0x12C+collideR for the contact tests (undone after), bit 1 selects the
   fire-and-forget contact dispatch (mode 3) over the blocking one, bits
   2/4 (0x14) probe the ground below, bit 4 also rejects a commit when the
   ground height differs from the actor's cached Z by >= 0xC9. Returns 1 on
   contact block, 2 on world collision / ground-step reject, 0 on commit. */
extern void ApplyEulerRotation(unsigned char *euler, int *mtx, int flag);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern int DispatchActorContactAtSphere(int *pos, int r, int a, int b,
                                        int actor, int flag);
extern int FindGroundHeightBelow(int *pt, int z);
extern void RefreshActorDerivedState(int actor, int n);
extern void EncodeCachedVecToActorDirCode(int actor);

int TryMoveActorForward(int actor, int speed, int contactR, int collideR,
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
  ApplyEulerRotation((unsigned char *)(actor + 0x44), mtx, 0);
  vec[0] = speed;
  vec[2] = 0;
  vec[1] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  AddVector(vec, vec, (int *)(actor + 0xC));

  if (flags & 1) {
    d = vec[2] + 0x12C;
    vec[2] = d + collideR;
  }
  if (contactR != 0) {
    if ((flags & 2) == 0) {
      if (DispatchActorContactAtSphere(vec, contactR, 0, 0, actor, 0) != 0) {
        return 1;
      }
    } else {
      DispatchActorContactAtSphere(vec, contactR, 0, 0, actor, 3);
    }
  }
  if (collideR != 0) {
    if (CollideSphereWithWorldAndActors(vec, collideR, collideR) != 0) {
      goto blocked;
    }
  }
  if (flags & 1) {
    d = vec[2] - 0x12C;
    vec[2] = d - collideR;
  }
  if (flags & 0x14) {
    vec[2] += 0x400;
    ground = FindGroundHeightBelow(vec, 0x800);
  }
  if (flags & 0x10) {
    d = ground - *(int *)(actor + 0x14);
    if (d > 0) {
      if (d >= 0xC9) {
        goto blocked;
      }
      goto gdone;
    }
    if (*(int *)(actor + 0x14) - ground < 0xC9) {
      goto gdone;
    }
  blocked:
    return 2;
  }
gdone:
  CopyVector((int *)(actor + 0xC), vec);
  EncodeCachedVecToActorDirCode(actor);
  RefreshActorDerivedState(actor, 2);
  return 0;
}
