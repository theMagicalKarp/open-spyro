#include "globals.h"
#include "sdata.h"

extern void SnapshotPadInputState(uint *dst, uint *src);
extern void SelectCameraMode(void);
extern void UpdateScriptedCameraView(void);
extern void UpdateLedgeGrabCamera(void);
extern void UpdateGemPickupPathCamera(void);
extern void UpdateCameraSpringForMode(void);
extern int FindWorldFloorBelowPoint(uint *pos);

extern int D_80075844;
extern int D_80076E24;
extern int D_80076EBC;
extern int D_800785B4;

SDATA(g_nCameraSpringReengageCountdown, 4);

/* Per-frame camera driver (0x80037bd4, 708 bytes): latches look-around
   (Spyro state 0x1B), picks the next mode from the per-state table unless a
   scripted move or a sentinel holds it, arbitrates via SelectCameraMode and
   runs the mode's updater. Non-scripted modes then apply the decaying
   vertical camera shake and, while no CD stream runs, refresh the floor
   lookup under the camera. Pad snapshots bracket the frame. */
void UpdateCameraFrame(void) {
  if (g_nSpyroState == 0x1B) {
    g_nCameraLookAroundActive = 1;
  } else {
    g_nCameraLookAroundActive = 0;
  }
  g_bSpyroPadSnapshotReverseFlag = 0;
  if (g_nSpyroPadSnapshotCountdown != 0) {
    SnapshotPadInputState(&g_dwPadPressed, &g_dwPad2Buttons);
  }
  if (g_nSpyroScriptedMoveMode != 0) {
    g_nCameraNextMode = 0;
    g_nCameraSpringReengageCountdown = 0;
  } else {
    int *next;

    __asm__("" : "=r"(next) : "0"(&g_nCameraNextMode));
    if (*next >= 0) {
      *next = (&g_abSpyroStateCameraMode)[g_nSpyroState];
    }
  }
  SelectCameraMode();
  if (g_nCameraCurrentMode == 0x8000000E) {
    UpdateScriptedCameraView();
  } else if (g_nCameraCurrentMode == 0x80000012) {
    UpdateLedgeGrabCamera();
  } else {
    if (g_nCameraCurrentMode == 0x8000000B) {
      UpdateGemPickupPathCamera();
    } else {
      UpdateCameraSpringForMode();
    }
    if (g_nCameraShakeDuration != 0) {
      if (g_nCameraShakePeakAmplitude < g_nCameraShakeDuration) {
        g_nCameraShakePeakAmplitude = g_nCameraShakeDuration;
      }
      g_nCameraShakeDuration = g_nCameraShakeDuration - g_nFrameStep;
      if (g_nCameraShakeDuration < 0) {
        g_nCameraShakeDuration = 0;
      }
      g_anCameraPos[2] += g_nCameraShakeMagnitude * g_nCameraShakeDuration *
                              ((int)(g_dwGamestateFrames & 2) - 1) /
                              g_nCameraShakePeakAmplitude >>
                          6;
    } else {
      g_nCameraShakePeakAmplitude = 0;
    }
    if (g_nCdStreamState < 0) {
      if (FindWorldFloorBelowPoint((uint *)g_anCameraPos) != 0) {
        D_80076E24 = D_80075844;
      }
      if (D_80076E24 >= D_800785B4) {
        D_80076E24 = -1;
      }
      D_80076EBC = 0;
    }
  }
  if (g_bSpyroPadSnapshotReverseFlag != 0) {
    SnapshotPadInputState(&g_dwPad2Buttons, &g_dwPadPressed);
  }
}
