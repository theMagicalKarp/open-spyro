#include "globals.h"
#include "sdata.h"

extern void CopyVector(int *dst, int *src);
extern int ComputeCameraToTargetAngle(int *pos);

SDATA(g_nCameraLosClear, 4);

/* Camera cut (0x80034c84, 100 bytes): snaps the smoothed camera position to
   the current eye (g_anCameraPos + 0xC), zeros the spring tracking state,
   raises g_nCameraSnapped + g_nCameraLosClear and rebases the camera-to-target
   angles. */
void SnapCameraToTarget(void) {
  CopyVector(g_anCameraPos, g_anCameraPos + 3);
  g_nCameraCollisionRetries = 0;
  g_nCameraStuckFrames = 0;
  g_nCameraSnapped = 1;
  g_nCameraLosClear = 1;
  g_nCameraTargetInverted = ComputeCameraToTargetAngle(g_anCameraPos);
}
