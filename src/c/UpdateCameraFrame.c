#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "sdata.h"
#include "spyro_state.h"

SDATA(g_nCameraSpringReengageCountdown, 4);

/* Per-frame camera driver (0x80037bd4, 708 bytes): latches look-around
   (Spyro state 0x1B), picks the next mode from the per-state table unless a
   scripted move or a sentinel holds it, arbitrates via SelectCameraMode and
   runs the mode's updater. Non-scripted modes then apply the decaying
   vertical camera shake and, while no CD stream runs, refresh the floor
   lookup under the camera. Pad snapshots bracket the frame. */
void UpdateCameraFrame(void) {
  if (g_nSpyroState == SPYRO_ST_HURT_LONG) {
    g_nCameraLookAroundActive = 1;
  } else {
    g_nCameraLookAroundActive = 0;
  }
  g_bSpyroPadSnapshotReverseFlag = 0;
  if (g_nSpyroPadSnapshotCountdown != 0) {
    SnapshotPadInputState(&g_dwPadPressed, &g_dwPad2Buttons);
  }
  if (g_nSpyroScriptedMoveMode != 0) {
    g_nCameraNextMode = CAM_MODE_STAND;
    g_nCameraSpringReengageCountdown = 0;
  } else {
    int *next;

    OPAQUE_COPY(next, &g_nCameraNextMode);
    if (*next >= 0) {
      *next = (&g_abSpyroStateCameraMode)[g_nSpyroState];
    }
  }
  SelectCameraMode();
  if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
    UpdateScriptedCameraView();
  } else if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_LEVEL_TRANSITION)) {
    UpdateLevelTransitionCamera();
  } else {
    if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_PORTAL_PATH)) {
      UpdatePortalPathCamera();
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
        g_nCameraVisZone = g_nWorldFloorVisZone;
      }
      if (g_nCameraVisZone >= g_nVisZoneCount) {
        g_nCameraVisZone = -1;
      }
      g_apScriptedCameraViewBlock[0] = 0;
    }
  }
  if (g_bSpyroPadSnapshotReverseFlag != 0) {
    SnapshotPadInputState(&g_dwPad2Buttons, &g_dwPadPressed);
  }
}
