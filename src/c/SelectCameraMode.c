#include "globals.h"
#include "sdata.h"

extern int ComputeCameraToTargetAngle(int *pos);
extern void ProjectWorldPointGTEUnscaled(int *out, int *pos);
extern int abs(int x);

/* Held-base views of the camera record (from g_anCameraPos) and the Spyro
   record (from g_anSpyroWorldPos): field addresses taken off one register. */
extern int g_anCameraPosBlock[];
extern int g_anSpyroWorldPosBlock[];

#define CAM g_anCameraPosBlock
#define CAM_CURRENT_MODE 0x0C /* g_nCameraCurrentMode */
#define CAM_NEXT_MODE 0x26    /* g_nCameraNextMode */
#define CAM_ANCHOR 0x2A       /* g_pCameraAnchorPos */
#define SPY g_anSpyroWorldPosBlock
#define SPY_SPEED 0x46         /* g_nSpyroMotionSpeed */
#define SPY_SCRIPTED_MOVE 0x65 /* g_nSpyroScriptedMoveMode */
#define SPY_REQUEST 0x7D       /* g_dwSpyroRequestMask */

#define STATE_MODE() ((&g_abSpyroStateCameraMode)[g_nSpyroState])

typedef struct {
  int v[6];
} CameraAngles;

/* Re-sync the spring set to the angles recomputed from the eye position. */
#define REBASE()                                                               \
  {                                                                            \
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);                 \
    __asm__ volatile("");                                                      \
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

  if (mode == 0x80000009) {
    if (g_dwActiveCameraOptions & 0x10) {
      if (g_nSpyroState == 0 || g_nSpyroState == 0xD) {
        if (!(g_dwPadHeld & 0x10)) {
          g_nCameraNextMode = 0x80000010;
          g_nCameraPassiveModeHoldTimer = 0x2D;
          g_nSpyroSwingEulerPitchTarget = 0;
          g_nCameraScriptStickRequestY = 0;
          g_nCameraScriptStickRequestX = 0;
        }
        goto done;
      }
    } else if ((g_dwPadHeld & 0x10) && g_nSpyroState == 0) {
      goto done;
    }
    {
      int t = STATE_MODE();

      g_nSpyroSwingEulerPitchTarget = 0;
      g_nCameraScriptStickRequestY = 0;
      g_nCameraScriptStickRequestX = 0;
      *nextp = t;
    }
  } else if (mode == 0x8000000A) {
    if (!(SPY[SPY_REQUEST] & 0x200)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAM);
      __asm__ volatile("");
      opts = g_dwActiveCameraOptions;
      *(CameraAngles *)&g_nCameraSpringYaw = *(CameraAngles *)&g_nCameraYaw;
      if (((opts & 0x10) || g_nCameraLookAroundActive) &&
          CAM[CAM_NEXT_MODE] == 0 && g_nCameraSpringSuppressFlag == 0) {
        CAM[CAM_NEXT_MODE] = 0x80000010;
        goto hold;
      }
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == 0x80000007) {
    if (!(SPY[SPY_REQUEST] & 0x8000)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == 0x8000000C) {
    if (!(SPY[SPY_REQUEST] & 0x10000)) {
      CAM[CAM_NEXT_MODE] = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      REBASE();
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      REBASE();
    }
  } else if (mode == 0x8000000B) {
    if (!(SPY[SPY_REQUEST] & 0x400)) {
      table = STATE_MODE();
      *(int **)&CAM[CAM_ANCHOR] = SPY;
      *nextp = table;
    } else if (CAM[CAM_CURRENT_MODE] != mode) {
      if (g_nGemPickupCamPathReverseFlag != 0) {
        ((unsigned char *)g_pGemPickupCamPathData)[1] =
            ((unsigned char *)g_pGemPickupCamPathData)[0] - 1;
      } else {
        ((unsigned char *)g_pGemPickupCamPathData)[1] = 0;
      }
    }
  } else if (mode == 0x80000010) {
  passive:
    table = STATE_MODE();
    if (table == 0 || table == 4) {
      if (g_nCameraPassiveModeHoldTimer <= 0) {
        goto fallback;
      }
    } else {
      *nextp = table;
    }
  } else if (mode == 0x80000011) {
    goto passive;
  } else if (mode == 0x80000000) {
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
             ((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
             g_nCameraSpringSuppressFlag == 0) {
    ProjectWorldPointGTEUnscaled(pt, SPY);
    pt[0] -= 0x100;
    pt[1] -= 0x78;
    if (abs(pt[0]) > 0x40 || abs(pt[1]) > 0x28 || (unsigned)pt[2] > 0x1400) {
      g_nCameraNextMode = 0x80000010;
    hold:
      g_nCameraPassiveModeHoldTimer = 0x2D;
    }
  } else {
    if (*(int **)&CAM[CAM_ANCHOR] != g_anSpyroWorldPos &&
        CAM[CAM_CURRENT_MODE] != 6) {
      *(int **)&CAM[CAM_ANCHOR] = g_anSpyroWorldPos;
      REBASE();
    }
  }
done:
  if (CAM[CAM_NEXT_MODE] != g_nCameraCurrentMode) {
    if (((g_dwActiveCameraOptions & 0x10) || g_nCameraLookAroundActive) &&
        SPY[SPY_SCRIPTED_MOVE] == 0 && g_nCameraSpringSuppressFlag == 0 &&
        (CAM[CAM_NEXT_MODE] == 0 || CAM[CAM_NEXT_MODE] == 0x80000000) &&
        g_nCameraCurrentMode != 0x80000011 &&
        *(int **)&CAM[CAM_ANCHOR] == SPY) {
      g_nCameraPassiveModeHoldTimer = 8;
      CAM[CAM_NEXT_MODE] = 0x80000011;
      g_nCameraSpringReengageCountdown = 0x5A;
    } else if (CAM[CAM_CURRENT_MODE] == 2 ||
               CAM[CAM_CURRENT_MODE] == 0x8000000E) {
      REBASE();
    }
    g_nCameraCurrentMode = g_nCameraNextMode;
    if (g_nCameraCurrentMode == 0x80000009) {
      if (g_nCameraLosClear != 0) {
        g_nCameraCollisionRetries = 0;
      }
      g_anCameraScriptStickIntegrator[0] = 0;
      g_anCameraScriptStickIntegrator[1] = 0;
    }
  }
}
