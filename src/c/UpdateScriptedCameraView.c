#include "camera.h"
#include "cd_stream.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "spyro_block.h"
#include "spyro_state.h"

/* The active scripted-camera record pointer (g_pScriptedCameraView) and the
   camera position 0xc4 below it are reached through one held base register. */
#define VIEW (g_apScriptedCameraViewBlock[0])
#define CAMPOS ((int *)&g_apScriptedCameraViewBlock[-49])

/* Camera updater for mode CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE (scripted
   vantage triplet). While the level-load state machine is finished
   (g_nCdStreamState < LSTREAM_PRESERVE) it just re-picks the per-state camera
   mode, unless Spyro is in one of the states that own the camera themselves.
   Otherwise it retires the active record when the player has walked past its
   range (range) or left its screen window (projected y > 0xf0), draws one of
   the three ROM records at g_aScriptedCamViews (avoiding an immediate repeat)
   and re-seats Spyro's body angles from it. Records with aimAtSpyro set also
   refresh the camera euler angles. (0x800377a8, 632 bytes.) */
void UpdateScriptedCameraView(void) {
  int delta[3];
  int dist;
  int n;

  n = 0;
  if (g_nCdStreamState < LSTREAM_PRESERVE) {
    if (g_nSpyroState != SPYRO_ST_FALL &&
        g_nSpyroState != SPYRO_ST_GLIDE_DROP && g_nSpyroState != SPYRO_ST_FLY &&
        g_nSpyroState != SPYRO_ST_FLY_CLIMB &&
        g_nSpyroState != SPYRO_ST_FLY_DIVE && g_nSpyroState != SPYRO_ST_GLIDE &&
        g_nSpyroState != SPYRO_ST_HAZARD) {
      g_nCameraNextMode =
          (&g_abSpyroStateCameraMode)[*(volatile int *)&g_nSpyroState];
    }
  } else {
    if (VIEW == 0) {
      n = 1;
    } else {
      SubtractVector(delta, CAMPOS, g_anSpyroWorldPos);
      dist = VectorLength(delta, VEC_LEN_XYZ);
      SubtractVector(delta, CAMPOS,
                     SPY_VIA(g_anSpyroWorldPos, SPY_POS, SPY_PREV_POS));
      if (VIEW->range < dist && VectorLength(delta, VEC_LEN_XYZ) < dist) {
        n = 1;
      } else {
        ProjectWorldPointGTEUnscaled(delta, g_anSpyroWorldPos);
        if (delta[1] >= 0xf1) {
          n = 1;
        }
      }
    }
    if (n != 0 ||
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      n = GetRandomU32() & 0xfff;
      n = (n * 3) >> 12;
      if (VIEW == &g_aScriptedCamViews[n]) {
        n = (n + 1) % 3;
      }
      ActivateScriptedCameraView(&g_aScriptedCamViews[n]);
      CopyVector(g_anSpyroWorldPos, VIEW->spyroPos);
      *(volatile int *)&g_nSpyroBodyPitch = *(volatile int *)&VIEW->spyroPitch;
      *(volatile int *)&g_nSpyroBodyRoll = *(volatile int *)&VIEW->spyroRoll;
      *(volatile int *)&g_nSpyroBodyYaw = *(volatile int *)&VIEW->spyroYaw;
    }
  }
  if (VIEW->aimAtSpyro == 1) {
    SubtractVector(delta, g_anSpyroWorldPos, CAMPOS);
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAMPOS);
    UpdateCameraEulerAngles();
  }
}
