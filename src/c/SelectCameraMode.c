#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad_buttons.h"
#include "portal.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"

#define CAM g_anCameraPosBlock
#define CAM_CURRENT_MODE 0x0C /* g_nCameraCurrentMode */
#define CAM_NEXT_MODE 0x26    /* g_nCameraNextMode */
#define CAM_ANCHOR 0x2A       /* g_pCameraAnchorPos */

#define STATE_MODE() ((&g_abSpyroStateCameraMode)[g_nSpyroState])

/* Re-sync the spring set to the angles recomputed from the eye position. */
#define REBASE()                                                               \
  {                                                                            \
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);                 \
    SCHED_FENCE();                                                             \
    *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;     \
  }

SDATA(g_nCameraLosClear, 4);
SDATA(g_nCameraPassiveModeHoldTimer, 4);
SDATA(g_nCameraSpringReengageCountdown, 4);

/* Per-frame camera-mode arbiter (0x800357a4, 1972 bytes). Holds or releases
   the sentinel modes against the request bits (0x200 mode 0xA, 0x400 gem
   path 0xB, 0x8000 scripted target 7, 0x10000 scripted follow 0xC) and
   otherwise falls back to the per-state mode table. Free-look (9) exits to the
   passive hold 0x80000010 when L1 releases; the passive modes persist while
   the hold timer runs; an idle Spyro near the screen anchor re-enters passive
   mode. On a mode change, re-seats the spring angles where needed and, on
   entering 9, clears the script-stick integrator. */
void SelectCameraMode(void) {
  int pt[3];
  int *nextp = &CAM[CAM_NEXT_MODE];
  int mode = *nextp;
  int table;
  unsigned int opts;

  if (mode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
    if (g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) {
      if (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) {
        if (!(g_dwPadHeld & PAD_TRIANGLE)) {
          g_nCameraNextMode = (CAM_HELD | CAM_MODE_PASSIVE);
          g_nCameraPassiveModeHoldTimer = 0x2D;
          g_nSpyroSwingEulerTargetX = 0;
          g_nSpyroSwingEulerTargetY = 0;
          g_nSpyroSwingEulerTargetZ = 0;
        }
        goto done;
      }
    } else if ((g_dwPadHeld & PAD_TRIANGLE) && g_nSpyroState == SPYRO_ST_IDLE) {
      goto done;
    }
    {
      int t = STATE_MODE();

      g_nSpyroSwingEulerTargetX = 0;
      g_nSpyroSwingEulerTargetY = 0;
      g_nSpyroSwingEulerTargetZ = 0;
      *nextp = t;
    }
  } else if (mode == (CAM_HELD | CAM_MODE_REQUEST)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
      SCHED_FENCE();
      opts = g_dwActiveCameraOptions;
      *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
      if (((opts & CAM_OPT_PASSIVE_FOLLOW) || g_nCameraForcePassiveFollow) &&
          CAM[CAM_NEXT_MODE] == 0 && g_nCameraSpringSuppressFlag == 0) {
        CAM[CAM_NEXT_MODE] = (CAM_HELD | CAM_MODE_PASSIVE);
        goto hold;
      }
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_SCRIPTED_TARGET)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_TARGET_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_SCRIPTED_FOLLOW)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_FOLLOW_CAMERA)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PORTAL_PATH)) {
    if (!(SPY[SPY_REQUEST] & SPYRO_RQ_PORTAL_PATH_CAMERA)) {
      table = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      *nextp = table;
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      if (g_nPortalCamPathReverseFlag != 0) {
        g_pPortalCamPathData->index = g_pPortalCamPathData->count - 1;
      } else {
        g_pPortalCamPathData->index = 0;
      }
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PASSIVE)) {
  passive:
    table = STATE_MODE();
    if (table == 0 || table == 4) {
      if (g_nCameraPassiveModeHoldTimer <= 0) {
        goto fallback;
      }
    } else {
      *nextp = table;
    }
  } else if (mode == (CAM_HELD | CAM_MODE_PASSIVE_SETTLE)) {
    goto passive;
  } else if (mode == CAM_HELD) {
    if (SPY[SPY_SPEED] > 0x400) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
    } else {
      ProjectWorldPointGTEUnscaled(pt, SPY);
      pt[0] -= 0x100;
      pt[1] -= 0x78;
      if (abs(pt[0]) > 0x40 || abs(pt[1]) > 0x28 || (unsigned)pt[2] > 0x1400) {
      fallback:
        g_nCameraNextMode = STATE_MODE();
      }
    }
  } else if (mode == 0 && SPY[SPY_SCRIPTED_MOVE] == 0 &&
             ((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
              g_nCameraForcePassiveFollow) &&
             g_nCameraSpringSuppressFlag == 0) {
    ProjectWorldPointGTEUnscaled(pt, SPY);
    pt[0] -= 0x100;
    pt[1] -= 0x78;
    if (abs(pt[0]) > 0x40 || abs(pt[1]) > 0x28 || (unsigned)pt[2] > 0x1400) {
      g_nCameraNextMode = (CAM_HELD | CAM_MODE_PASSIVE);
    hold:
      g_nCameraPassiveModeHoldTimer = 0x2D;
    }
  } else {
    if (*(int **)&CAM[CAM_ANCHOR] != g_anSpyroWorldPos &&
        CAM[CAM_CURRENT_MODE] != CAM_MODE_HAZARD) {
      *(int **)&CAM[CAM_ANCHOR] = g_anSpyroWorldPos;
      REBASE();
    }
  }
done:
  if (CAM[CAM_NEXT_MODE] != g_nCameraCurrentMode) {
    if (((g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) ||
         g_nCameraForcePassiveFollow) &&
        SPY[SPY_SCRIPTED_MOVE] == 0 && g_nCameraSpringSuppressFlag == 0 &&
        (CAM[CAM_NEXT_MODE] == 0 || CAM[CAM_NEXT_MODE] == CAM_HELD) &&
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_PASSIVE_SETTLE) &&
        *(int **)&CAM[CAM_ANCHOR] == SPY) {
      g_nCameraPassiveModeHoldTimer = 8;
      CAM[CAM_NEXT_MODE] = (CAM_HELD | CAM_MODE_PASSIVE_SETTLE);
      g_nCameraSpringReengageCountdown = 0x5A;
    } else if (CAM[CAM_CURRENT_MODE] == 2 ||
               CAM[CAM_CURRENT_MODE] ==
                   (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      REBASE();
    }
    g_nCameraCurrentMode = g_nCameraNextMode;
    if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      if (g_nCameraLosClear != 0) {
        g_nCameraCollisionRetries = 0;
      }
      g_anCameraScriptStickIntegrator[0] = 0;
      g_anCameraScriptStickIntegrator[1] = 0;
    }
  }
}
