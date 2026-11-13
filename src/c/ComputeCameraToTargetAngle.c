#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "vecmath.h"

/* Camera-to-target angle solver (0x80033f08, 0x290). Builds the anchor→
   target delta vector, refines the 2D/3D distances, and derives the
   camera pitch/yaw from it. When the wrapped yaw delta is closer to the
   inverted heading (and the mode is CAM_HELD | CAM_MODE_FREE_LOOK or the target
   is flagged inverted), flips the framing: yaw += half-turn (or snaps to the
   spring yaw when the target is nearly overhead), pitch mirrored around 0x800,
   and returns 1; otherwise commits the plain yaw (or spring yaw) and
   returns 0. Tail publishes the draft roll and the wrapped pitch/yaw
   biases against the euler camera state. */

int ComputeCameraToTargetAngle(int *target) {
  int vec[3];
  int yaw;
  int dist;
  int oldYaw;
  int flipYaw;
  int yawDelta;
  int flipDelta;
  int pitchBias;
  int yawBias;
  int *pitch;

  SubtractVector(vec, target, g_pCameraAnchorPos);
  dist = VectorLength(vec, VEC_LEN_XYZ);
  g_nCameraDistance = dist;
  g_nCameraDistance = RefineSqrtEstimate(vec, dist, 1);
  dist = VectorLength(vec, VEC_LEN_XY);
  dist = RefineSqrtEstimate(vec, dist, 0);
  g_nCameraPitch = ArcTan2(dist, vec[2], ATAN_ANGLE12);
  yaw = ArcTan2(vec[0], -vec[1], ATAN_ANGLE12);
  oldYaw = g_nCameraYaw;
  yawDelta = (yaw - oldYaw) & ANGLE12_MASK;
  if (yawDelta >= 0x801) {
    yawDelta -= 0x1000;
  }
  flipYaw = oldYaw - 0x800;
  flipDelta = (yaw - flipYaw) & ANGLE12_MASK;
  if (flipDelta >= 0x801) {
    flipDelta -= 0x1000;
  }
  if (abs(flipDelta) < abs(yawDelta) &&
      (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK) ||
       g_nCameraTargetInverted != 0)) {
    if (abs(vec[0]) < 0x81) {
      if (abs(vec[1]) < 0x81) {
        goto spring;
      }
    }
    g_nCameraYaw = (yaw + ANGLE12_HALF) & ANGLE12_MASK;
    goto flip;
  spring:
    g_nCameraYaw = g_nCameraSpringYaw;
  flip:
    yaw = 1;
    pitch = g_anCameraPitchBlock;
    pitch[0] = (0x800 - pitch[0]) & ANGLE12_MASK;
  } else {
    if (abs(vec[0]) >= 0x81 || abs(vec[1]) >= 0x81) {
      g_nCameraYaw = yaw;
    } else {
      g_nCameraYaw = g_nCameraSpringYaw;
    }
    yaw = 0;
  }
  pitchBias = (g_nCameraEulerPitch - g_nCameraPitch) & ANGLE12_MASK;
  g_nCameraDraftRoll = g_nCameraEulerRoll;
  g_nCameraDraftPitchBias = pitchBias;
  if (pitchBias >= 0x801) {
    g_nCameraDraftPitchBias = pitchBias - 0x1000;
  }
  yawBias = (0x800 - g_nCameraEulerYaw - g_nCameraYaw) & ANGLE12_MASK;
  g_nCameraDraftYawBias = yawBias;
  if (yawBias >= 0x801) {
    g_nCameraDraftYawBias = yawBias - 0x1000;
  }
  return yaw;
}
