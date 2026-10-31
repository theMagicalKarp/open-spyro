#include "actor_type.h"
#include "funcs.h"
#include "globals.h"

#define ACT ((Actor *)actor)

/* Steer actor `actor` toward world point `target` and advance it (0x8003bad0).
   Computes the target yaw/pitch from the delta vector, wraps each 8-bit euler
   delta to signed and clamps it to +/-yawLim / +/-pitchLim, applies the turn to
   rotY/rotZ, then moves the actor `speed` units along its
   new facing. With a nonzero `radius` it resolves sphere collision (snapping
   to g_anCollisionResolvedPos) and dispatches contact callbacks; returns 1 if
   the move was blocked/adjusted, else 0. */

int SteerActorTowardPoint(int actor, int *target, int speed, int yawLim,
                          int pitchLim, int radius) {
  int dv[3];
  int mtx[8];
  int moved;
  int *pos;
  int yaw;
  int pitch;
  int heading;

  moved = 0;
  pos = &ACT->posX;
  SubtractVector(dv, target, pos);
  yaw = ArcTan2(dv[0], dv[1], 0);
  pitch = ArcTan2(VectorLength(dv, VEC_LEN_XY), dv[2], 0);

  heading = ACT->rotZ;
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

  pitch = (pitch - ACT->rotY) & 0xFF;
  if (pitch >= 0x80) {
    pitch -= 0x100;
  }
  if (pitch < -pitchLim) {
    pitch = -pitchLim;
  }
  if (pitchLim < pitch) {
    pitch = pitchLim;
  }

  ACT->rotZ = heading + yaw;
  ACT->rotY = ACT->rotY + pitch;
  ApplyEulerRotation(&ACT->rotX, mtx, 0);
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
  RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
  ProbeActorGroundHeight(actor);
  EncodeCachedVecToActorDirCode(actor);
  return moved;
}
