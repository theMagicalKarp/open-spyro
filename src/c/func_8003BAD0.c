#include "globals.h"

/* Steer actor `actor` toward world point `target` and advance it (0x8003bad0).
   Computes the target yaw/pitch from the delta vector, wraps each 8-bit euler
   delta to signed and clamps it to +/-yawLim / +/-pitchLim, applies the turn to
   the euler bytes (+0x45/+0x46), then moves the actor `speed` units along its
   new facing. With a nonzero `radius` it resolves sphere collision (snapping
   to g_anCollisionResolvedPos) and dispatches contact callbacks; returns 1 if
   the move was blocked/adjusted, else 0. */
extern void SubtractVector(int *dst, int *a, int *b);
extern int ArcTan2(int y, int x, int high_precision);
extern unsigned int VectorLength(int *vec, int include_z);
extern void ApplyEulerRotation(unsigned char *euler, int *mtx, int flag);
extern void RotateVectorByMatrix(int *mtx, int *src, int *dst);
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern int CollideSphereWithWorldAndActors(int *pos, int r1, int r2);
extern int DispatchActorContactAtSphere(int *pos, int r, int a, int b,
                                        int actor, int flag);
extern void func_800529E4(int actor, int n);
extern int func_80038340(int actor);
extern void EncodeCachedVecToActorDirCode(int actor);

int func_8003BAD0(int actor, int *target, int speed, int yawLim, int pitchLim,
                  int radius) {
  int dv[3];
  int mtx[8];
  int moved;
  int *pos;
  int yaw;
  int pitch;
  int heading;

  moved = 0;
  pos = (int *)(actor + 0xC);
  SubtractVector(dv, target, pos);
  yaw = ArcTan2(dv[0], dv[1], 0);
  pitch = ArcTan2(VectorLength(dv, 0), dv[2], 0);

  heading = *(unsigned char *)(actor + 0x46);
  yaw = (yaw - heading) & 0xFF;
  if (yaw >= 0x80) {
    yaw -= 0x100;
  }
  if (yaw < -yawLim) {
    yaw = -yawLim;
  }
  if (yawLim < yaw) {
    yaw = yawLim;
  }

  pitch = (pitch - *(unsigned char *)(actor + 0x45)) & 0xFF;
  if (pitch >= 0x80) {
    pitch -= 0x100;
  }
  if (pitch < -pitchLim) {
    pitch = -pitchLim;
  }
  if (pitchLim < pitch) {
    pitch = pitchLim;
  }

  *(unsigned char *)(actor + 0x46) = heading + yaw;
  *(unsigned char *)(actor + 0x45) = *(unsigned char *)(actor + 0x45) + pitch;
  ApplyEulerRotation((unsigned char *)(actor + 0x44), mtx, 0);
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
    if (DispatchActorContactAtSphere(pos, radius, 0, 0, actor, 1) != 0) {
      moved = 1;
    }
  }
  func_800529E4(actor, 2);
  func_80038340(actor);
  EncodeCachedVecToActorDirCode(actor);
  return moved;
}
