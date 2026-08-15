#include "globals.h"

extern void TickSpyroGameplayFrame(void);
extern void UpdateCameraFrame(void);

/* incomplete-array view of g_nCameraEulerPitch (0x80076e1e) — every access is
   at offset 0 off one held base register. */
extern short g_anCameraEulerPitchBlock[];

/* 0x8002e000 (132 bytes) — the camera-settle gamestate tick: run one gameplay
   frame and one camera frame, wrap the camera pitch back into signed 12-bit
   range, and drop out to gamestate 0 once the camera has either pitched below
   -0x200 or handed control back to the mode-12 parameter block. */
void CameraSettle_Update(void) {
  int pitch;

  TickSpyroGameplayFrame();
  UpdateCameraFrame();
  pitch = g_anCameraEulerPitchBlock[0] & 0xFFF;
  g_anCameraEulerPitchBlock[0] = pitch;
  if (pitch > 0x800) {
    g_anCameraEulerPitchBlock[0] = pitch - 0x1000;
  }
  if ((g_anCameraEulerPitchBlock[0] < -0x200) ||
      (g_pCameraTargetParams == g_anCameraMode12Params)) {
    g_nGamestate = 0;
  }
}
