#include "camera.h"
#include "globals.h"

/* The camera's target pose: six consecutive ints from g_nCameraTargetYaw. */
#define CAMERA_TARGET (*(CameraAngles *)&g_nCameraTargetYaw)

/* 0x80034198 (0x6c) — load the camera target pose from the current mode's
   parameter record: yaw relative to the mode's yaw bias (wrapped to 12 bits),
   then pitch, distance, roll and the two biases verbatim.

   The target pose is written as record members: an in-struct store depends
   on the next in-struct `params[k]` read (sched.c true_dependence), which
   keeps the original's load/store interleave instead of batching the six
   loads. */
void LoadCameraTargetAnglesFromMode(void) {
  int *params = g_pCameraTargetParams;

  CAMERA_TARGET.yaw = (params[0] - g_nCameraParamsReferenceYaw) & 0xfff;
  CAMERA_TARGET.pitch = params[1];
  CAMERA_TARGET.distance = params[2];
  CAMERA_TARGET.roll = params[3];
  CAMERA_TARGET.pitchBias = params[4];
  CAMERA_TARGET.yawBias = params[5];
}
