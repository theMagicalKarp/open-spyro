#include "globals.h"

extern void SubtractVector(int *dst, int *a, int *b);
extern uint VectorLength(int *vec, int include_z);
extern void ProjectWorldPointGTEUnscaled(int *out, int *pos);
extern uint GetRandomU32(void);
extern void ActivateScriptedCameraView(int *view);
extern void CopyVector(int *dst, int *src);
extern int ComputeCameraToTargetAngle(int *pos);
extern void UpdateCameraEulerAngles(void);

/* The active scripted-camera record pointer (D_80076EBC) and the camera
   position 0xc4 below it are reached through one held base register. */
extern int *g_apScriptedCameraViewBlock[];
#define VIEW (g_apScriptedCameraViewBlock[0])
#define CAMPOS ((int *)&g_apScriptedCameraViewBlock[-49])

extern int D_8006EB24[]; /* 3 scripted-camera records, stride 0x38 */

/* Camera updater for mode 0x8000000e (scripted vantage triplet). While the
   level-load state machine is finished (g_nCdStreamState < 0) it just re-picks
   the per-state camera mode, unless Spyro is in one of the states that own the
   camera themselves. Otherwise it retires the active record when the player has
   walked past its range (field 0x34) or left its screen window (projected
   y > 0xf0), draws one of the three ROM records at D_8006EB24 (avoiding an
   immediate repeat) and re-seats Spyro's body angles from it. Records whose
   type field (0x30) is 1 also refresh the camera euler angles.
   (0x800377a8, 632 bytes.) */
void UpdateScriptedCameraView(void) {
  int delta[3];
  int dist;
  int n;

  n = 0;
  if (g_nCdStreamState < 0) {
    if (g_nSpyroState != 6 && g_nSpyroState != 0x10 && g_nSpyroState != 0x20 &&
        g_nSpyroState != 0x21 && g_nSpyroState != 0x22 &&
        g_nSpyroState != 0xf && g_nSpyroState != 0x1d) {
      g_nCameraNextMode =
          (&g_abSpyroStateCameraMode)[*(volatile int *)&g_nSpyroState];
    }
  } else {
    if (VIEW == 0) {
      n = 1;
    } else {
      SubtractVector(delta, CAMPOS, g_anSpyroWorldPos);
      dist = VectorLength(delta, 1);
      SubtractVector(delta, CAMPOS, g_anSpyroWorldPos + 0x23);
      if (VIEW[0xd] < dist && (int)VectorLength(delta, 1) < dist) {
        n = 1;
      } else {
        ProjectWorldPointGTEUnscaled(delta, g_anSpyroWorldPos);
        if (delta[1] >= 0xf1) {
          n = 1;
        }
      }
    }
    if (n != 0 || g_nCameraCurrentMode != 0x8000000e) {
      n = GetRandomU32() & 0xfff;
      n = (n * 3) >> 12;
      if (VIEW == &D_8006EB24[n * 0xe]) {
        n = (n + 1) % 3;
      }
      ActivateScriptedCameraView(&D_8006EB24[n * 0xe]);
      CopyVector(g_anSpyroWorldPos, VIEW);
      *(volatile int *)&g_nSpyroBodyPitch = *(volatile int *)&VIEW[3];
      *(volatile int *)&g_nSpyroBodyRoll = *(volatile int *)&VIEW[4];
      *(volatile int *)&g_nSpyroBodyYaw = *(volatile int *)&VIEW[5];
    }
  }
  if (VIEW[0xc] == 1) {
    SubtractVector(delta, g_anSpyroWorldPos, CAMPOS);
    g_nCameraTargetInverted = ComputeCameraToTargetAngle(CAMPOS);
    UpdateCameraEulerAngles();
  }
}
