#include "globals.h"

/* Average the frame-to-frame motion of the collision tri Spyro stands on
   (0x8003e628, 0x130). Unpacks the current ground tri, subtracts the
   previous frame's vertices, averages the three deltas into the pending
   platform impulse, and suppresses the camera spring when the platform
   moved far enough. */

extern void ZeroVector(int *vec);
extern void UnpackWorldCollisionTri(int chunkId, int *verts);
extern void SubtractVector(int *dst, int *a, int *b);
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern int VectorLength(int *vec, int include_z);

extern int g_anSpyroGroundChunkBlock[];

void AverageSpyroPlatformMotion(void) {
  int tri[10];
  int avg[3];
  int *chunk = g_anSpyroGroundChunkBlock;
  int *v;
  int *w;
  int *sum;
  int i;

  v = avg;
  if (chunk[0] >= 0) {
    ZeroVector(v);
    UnpackWorldCollisionTri(chunk[0], tri);
    i = 0;
    sum = v;
    chunk += 1;
    w = tri;
    do {
      SubtractVector(w, w, chunk);
      AddVector(sum, sum, w);
      chunk += 3;
      i += 1;
      w += 3;
    } while (i < 3);
    avg[0] = avg[0] / 3;
    avg[1] = avg[1] / 3;
    avg[2] = avg[2] / 3;
    CopyVector(g_anSpyroPendingImpulse, sum);
    if (VectorLength(sum, 1) >= 0x21) {
      g_nCameraSpringSuppressFlag = 1;
    }
  }
}
