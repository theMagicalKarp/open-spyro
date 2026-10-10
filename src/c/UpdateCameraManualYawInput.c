#include "globals.h"
#include "sdata.h"

SDATA(g_nCameraManualYawInput, 4);

/* R1/L1 camera rotate (0x80035f58, 92 bytes): writes +-0x400 to
   g_nCameraManualYawInput, the yaw-drive override AdvanceCameraSpringStep
   consumes. Suppressed by request bit 0x1000 or while the camera is stuck. */
void UpdateCameraManualYawInput(void) {
  g_nCameraManualYawInput = 0;
  if (g_dwSpyroRequestMask & 0x1000) {
    return;
  }
  if (g_nCameraStuckFrames != 0) {
    return;
  }
  if (g_dwPadHeld & 2) {
    g_nCameraManualYawInput = -0x400;
  } else if (g_dwPadHeld & 1) {
    g_nCameraManualYawInput = 0x400;
  }
}
