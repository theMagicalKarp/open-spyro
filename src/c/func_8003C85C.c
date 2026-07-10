#include "globals.h"

extern void RotateVectorByMatrix(int *mtx, int *in, int *out);
extern void AddVector(int *dst, int *a, int *b);
extern void SubtractVector(int *dst, int *a, int *b);
extern void func_80016FD0(int *out, int *mtx);
extern unsigned int VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int high_precision);

extern int g_anCameraScriptStickRequestXBlock[];

/* 0x8003c85c (0x138) — derive scripted-camera stick requests that steer the
   view toward `target`: project a probe point ahead of Spyro through his body
   matrix, take the offset to the target back into body space, then convert
   the pitch/yaw angles into clamped stick-request values (dead-banded away
   from the straight-up/straight-down poles). */
void func_8003C85C(int *target) {
  int inv[8];
  int v[3];

  v[0] = 0x64;
  v[1] = 0;
  v[2] = 0x40;
  RotateVectorByMatrix(g_anSpyroBodyMtx, v, v);
  AddVector(v, v, g_anSpyroBodyMtx - 13);
  SubtractVector(v, target, v);
  func_80016FD0(inv, g_anSpyroBodyMtx);
  RotateVectorByMatrix(inv, v, v);

  {
    int ang = ArcTan2(VectorLength(v, 0), v[2], 1);
    g_nCameraScriptStickRequestY = ang;
    if ((unsigned int)(ang - 0x181) < 0x67F) {
      g_nCameraScriptStickRequestY = 0x180;
    } else if ((unsigned int)(ang - 0x800) < 0x680) {
      g_nCameraScriptStickRequestY = 0xE80;
    }
  }

  {
    int *req;
    int ang = ArcTan2(v[0], v[1], 1);
    req = g_anCameraScriptStickRequestXBlock;
    *req = ang;
    if ((unsigned int)(ang - 0x201) < 0x5FF) {
      *req = 0x200;
    } else if ((unsigned int)(ang - 0x800) < 0x600) {
      *req = 0xE00;
    }
  }
}
