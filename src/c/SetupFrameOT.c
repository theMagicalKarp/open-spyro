#include "globals.h"

extern void FillBlock16(void *dst, int value, int count);
extern void RenderWorldChunks(int mode);
extern int D_80076E24;
extern int D_800785B4;
extern int D_800785D0; /* far-plane draw distance */

/* 0x8002b9cc (0x9c) — render stage 1: clear the ordering-table depth bins,
   pick the frame's draw distance and render the world chunks. Past the
   D_800785B4 threshold the distance is 0x28000 and the chunks render in
   mode D_80076E24; otherwise gamestates 0xd/0xe draw to 0x1c000 and the rest
   to 0x14000, rendering in mode -1.

   Both calls share one `jal` through jump.c cross-jumping. Each arm of the
   inner select stores its own distance: with one store after the if/else,
   jump.c rewrites the select as "default, then conditional override", while
   per-arm stores leave the two `lui 1` heads for reorg to merge into the
   branch delay slot (the original's shape). */
void SetupFrameOT(void) {
  int dist;

  FillBlock16(&g_pOtDepthBinHead0, 0, 0x1c00);
  if (D_80076E24 < D_800785B4) {
    D_800785D0 = 0x28000;
    RenderWorldChunks(D_80076E24);
  } else {
    if ((unsigned int)(g_nGamestate - 0xd) < 2) {
      dist = 0x1c000;
      D_800785D0 = dist;
    } else {
      dist = 0x14000;
      D_800785D0 = dist;
    }
    RenderWorldChunks(-1);
  }
}
