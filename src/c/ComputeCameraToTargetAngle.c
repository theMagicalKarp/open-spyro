#include "globals.h"

/* Camera-to-target angle solver (0x80033f08, 0x290). Builds the anchor→
   target delta vector, refines the 2D/3D distances, and derives the
   camera pitch/yaw from it. When the wrapped yaw delta is closer to the
   inverted heading (and the mode is 0x80000009 or the target is flagged
   inverted), flips the framing: yaw += half-turn (or snaps to the spring
   yaw when the target is nearly overhead), pitch mirrored around 0x800,
   and returns 1; otherwise commits the plain yaw (or spring yaw) and
   returns 0. Tail publishes the draft roll and the wrapped pitch/yaw
   biases against the euler camera state. */
extern void SubtractVector(int *dst, int *a, int *b);
extern int VectorLength(int *v, int mode);
extern int RefineSqrtEstimate(int *v, int len, int mode);
extern int ArcTan2(int x, int y, int mode);
extern int abs(int);

extern int g_anCameraPitchBlock[]; /* g_nCameraPitch */

int ComputeCameraToTargetAngle(int *target) {
  int vec[3];
  int yaw;
  int dist;
  int old;
  int opp;
  int d1;
  int d2;
  int d;
  int e;
  int *pitch;

  SubtractVector(vec, target, g_pCameraAnchorPos);
  dist = VectorLength(vec, 1);
  g_nCameraDistance = dist;
  g_nCameraDistance = RefineSqrtEstimate(vec, dist, 1);
  dist = VectorLength(vec, 0);
  dist = RefineSqrtEstimate(vec, dist, 0);
  g_nCameraPitch = ArcTan2(dist, vec[2], 1);
  yaw = ArcTan2(vec[0], -vec[1], 1);
  old = g_nCameraYaw;
  d1 = (yaw - old) & 0xFFF;
  if (d1 >= 0x801) {
    d1 -= 0x1000;
  }
  opp = old - 0x800;
  d2 = (yaw - opp) & 0xFFF;
  if (d2 >= 0x801) {
    d2 -= 0x1000;
  }
  if (abs(d2) < abs(d1) &&
      (g_nCameraCurrentMode == 0x80000009 || g_nCameraTargetInverted != 0)) {
    if (abs(vec[0]) < 0x81) {
      if (abs(vec[1]) < 0x81) {
        goto spring;
      }
    }
    g_nCameraYaw = (yaw + 0x800) & 0xFFF;
    goto flip;
  spring:
    g_nCameraYaw = g_nCameraSpringYaw;
  flip:
    yaw = 1;
    pitch = g_anCameraPitchBlock;
    pitch[0] = (0x800 - pitch[0]) & 0xFFF;
  } else {
    if (abs(vec[0]) >= 0x81 || abs(vec[1]) >= 0x81) {
      g_nCameraYaw = yaw;
    } else {
      g_nCameraYaw = g_nCameraSpringYaw;
    }
    yaw = 0;
  }
  d = (g_nCameraEulerPitch - g_nCameraPitch) & 0xFFF;
  g_nCameraDraftRoll = g_nCameraEulerRoll;
  g_nCameraDraftPitchBias = d;
  if (d >= 0x801) {
    g_nCameraDraftPitchBias = d - 0x1000;
  }
  e = (0x800 - g_nCameraEulerYaw - g_nCameraYaw) & 0xFFF;
  g_nCameraDraftYawBias = e;
  if (e >= 0x801) {
    g_nCameraDraftYawBias = e - 0x1000;
  }
  return yaw;
}
