#include "angle.h"
#include "globals.h"

/* Publish the spring camera's orientation as the 12-bit euler angles the render
   path consumes (0x800342f8, 0x60 bytes). Pitch picks up the spring bias, roll
   passes straight through, and yaw is measured backwards from 0x800. The single
   `angle` scratch carries roll and then yaw, so both live in one register. */
void UpdateCameraEulerAngles(void) {
  int angle = g_nCameraSpringRoll;

  g_nCameraEulerPitch =
      (g_nCameraPitch + g_nCameraSpringPitchBias) & ANGLE12_MASK;
  g_nCameraEulerRoll = angle;
  angle = g_nCameraYaw;
  g_nCameraEulerYaw =
      (ANGLE12_HALF - angle - g_nCameraSpringYawBias) & ANGLE12_MASK;
}
