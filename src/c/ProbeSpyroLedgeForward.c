#include "globals.h"

/* Probe for a grabbable ledge ahead of Spyro (0x8003e218, 0x100). Cast a ray
   from just above Spyro (body anchor -0x34, +0x164 up) to a point 0x1C4
   ahead / 0x1A4 up rotated into body space; on a hit whose surface normal is
   steep enough (arctan of z vs horizontal length >= 0x17) latch the
   ledge-grab flag (+0x80 past the body matrix) and stash the contact normal
   (+0x84). */
extern void RotateVectorByMatrix(int *mtx, int *src, int *dst);
extern void AddVector(int *dst, int *a, int *b);
extern void ApplyActiveGteRotation(int *src, int *dst);
extern int CastRayWorldAndActors(int *from, int *to);
extern unsigned int VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int flag);
extern void CopyVector(int *dst, int *src);

extern int g_anSpyroBodyMtx[];
extern int g_anCollisionProbeVec[];

void ProbeSpyroLedgeForward(void) {
  int from[4];
  int to[3];
  int *anchor;
  int *tp;
  int *cp;
  unsigned int len;
  from[2] = -0x164;
  to[0] = 0x1C4;
  from[1] = 0;
  from[0] = 0;
  to[1] = 0;
  to[2] = -0x1A4;
  RotateVectorByMatrix(g_anSpyroBodyMtx, from, from);
  anchor = g_anSpyroBodyMtx - 13;
  AddVector(from, from, anchor);
  do {
    tp = to;
    ApplyActiveGteRotation(tp, tp);
  } while (0);
  AddVector(tp, tp, anchor);
  if (CastRayWorldAndActors(from, tp) != 0) {
    cp = g_anCollisionProbeVec;
    len = VectorLength(cp, 0);
    if (((signed char)ArcTan2(g_anCollisionProbeVec[2], len, 0)) >= 0x17) {
      g_anSpyroBodyMtx[32] = 1;
      CopyVector(g_anSpyroBodyMtx + 33, cp);
    }
  }
}
