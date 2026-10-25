#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Build the 3x3 camera view matrix from g_nCameraEulerPitch/Yaw/Roll (X*Y*Z
   order). Writes the full matrix to the view-matrix slot (0x80076de4) and a
   Y-row-scaled (*0x140>>9) copy to g_anWorldToCameraRotMtx (0x80033c50). */
void BuildCameraViewMatrix(void) {
  int v;
  MATRIX view;
  MATRIX axis;

  FillWord(&view, 0, 0x20);
  SetTransMatrix(&view);
  view.m[0][0] = 0x1000;
  v = LookupCosine(g_anCameraEulerPitchBlock[0]);
  view.m[1][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0]);
  view.m[2][1] = v;
  v = LookupSine(g_anCameraEulerPitchBlock[0]);
  view.m[1][2] = -v;
  v = LookupCosine(g_anCameraEulerPitchBlock[0]);
  view.m[2][2] = v;

  FillWord(&axis, 0, 0x20);
  v = LookupCosine(g_nCameraEulerYaw);
  axis.m[0][0] = v;
  v = LookupSine(g_nCameraEulerYaw);
  axis.m[2][0] = -v;
  axis.m[1][1] = 0x1000;
  v = LookupSine(g_nCameraEulerYaw);
  axis.m[0][2] = v;
  v = LookupCosine(g_nCameraEulerYaw);
  axis.m[2][2] = v;
  MulMatrix(&view, &axis);

  FillWord(&axis, 0, 0x20);
  v = LookupCosine(g_nCameraEulerRoll);
  axis.m[0][0] = v;
  v = LookupSine(g_nCameraEulerRoll);
  axis.m[1][0] = -v;
  v = LookupSine(g_nCameraEulerRoll);
  axis.m[0][1] = v;
  v = LookupCosine(g_nCameraEulerRoll);
  axis.m[1][1] = v;
  axis.m[2][2] = 0x1000;
  MulMatrix(&view, &axis);

  CopyWords((char *)g_anCameraEulerPitchBlock - 0x3a, &view, 0x14);
  v = view.m[1][0] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][0] = v >> 9;
  v = view.m[1][1] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][1] = v >> 9;
  v = view.m[1][2] * 0x140;
  if (v < 0) {
    v += 0x1ff;
  }
  view.m[1][2] = v >> 9;
  CopyWords((char *)g_anCameraEulerPitchBlock - 0x4e, &view, 0x14);
}
