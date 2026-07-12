#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void CopyWords(void *dst, void *src, int byte_count);
extern void SetTransMatrix(MATRIX *m);
extern int LookupSine(unsigned int angle);
extern int LookupCosine(unsigned int angle);
extern void MulMatrix(MATRIX *a, MATRIX *b);

/* Held-base alias for &g_nCameraEulerPitch: the pitch reads and both output
   matrix addresses (view @ base-0x3A, world-to-camera @ base-0x4E) share it. */
extern short g_anCameraEulerPitchBlock[];

/* Build the 3x3 camera view matrix from g_nCameraEulerPitch/Yaw/Roll (X*Y*Z
   order). Writes the full matrix to the view-matrix slot (0x80076de4) and a
   Y-row-scaled (*0x140>>9) copy to g_anWorldToCameraRotMtx (0x80033c50). */
void BuildCameraViewMatrix(void) {
  int iVar1;
  MATRIX local_50;
  MATRIX local_30;

  FillWord(&local_50, 0, 0x20);
  SetTransMatrix(&local_50);
  local_50.m[0][0] = 0x1000;
  iVar1 = LookupCosine(g_anCameraEulerPitchBlock[0]);
  local_50.m[1][1] = iVar1;
  iVar1 = LookupSine(g_anCameraEulerPitchBlock[0]);
  local_50.m[2][1] = iVar1;
  iVar1 = LookupSine(g_anCameraEulerPitchBlock[0]);
  local_50.m[1][2] = -iVar1;
  iVar1 = LookupCosine(g_anCameraEulerPitchBlock[0]);
  local_50.m[2][2] = iVar1;

  FillWord(&local_30, 0, 0x20);
  iVar1 = LookupCosine(g_nCameraEulerYaw);
  local_30.m[0][0] = iVar1;
  iVar1 = LookupSine(g_nCameraEulerYaw);
  local_30.m[2][0] = -iVar1;
  local_30.m[1][1] = 0x1000;
  iVar1 = LookupSine(g_nCameraEulerYaw);
  local_30.m[0][2] = iVar1;
  iVar1 = LookupCosine(g_nCameraEulerYaw);
  local_30.m[2][2] = iVar1;
  MulMatrix(&local_50, &local_30);

  FillWord(&local_30, 0, 0x20);
  iVar1 = LookupCosine(g_nCameraEulerRoll);
  local_30.m[0][0] = iVar1;
  iVar1 = LookupSine(g_nCameraEulerRoll);
  local_30.m[1][0] = -iVar1;
  iVar1 = LookupSine(g_nCameraEulerRoll);
  local_30.m[0][1] = iVar1;
  iVar1 = LookupCosine(g_nCameraEulerRoll);
  local_30.m[1][1] = iVar1;
  local_30.m[2][2] = 0x1000;
  MulMatrix(&local_50, &local_30);

  CopyWords((char *)g_anCameraEulerPitchBlock - 0x3a, &local_50, 0x14);
  iVar1 = local_50.m[1][0] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  local_50.m[1][0] = iVar1 >> 9;
  iVar1 = local_50.m[1][1] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  local_50.m[1][1] = iVar1 >> 9;
  iVar1 = local_50.m[1][2] * 0x140;
  if (iVar1 < 0) {
    iVar1 += 0x1ff;
  }
  local_50.m[1][2] = iVar1 >> 9;
  CopyWords((char *)g_anCameraEulerPitchBlock - 0x4e, &local_50, 0x14);
}
