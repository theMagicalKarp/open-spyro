#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* 0x8002b9cc (0x9c) — render stage 1: clear the ordering-table depth bins,
   pick the frame's draw distance and render the world chunks. When the
   camera stands in a visibility zone (g_nCameraVisZone below g_nVisZoneCount)
   the distance is 0x28000 and only that zone's chunks render; otherwise
   gamestates 0xd/0xe draw to 0x1c000 and the rest to 0x14000, rendering every
   chunk (zone -1).

   Both calls share one `jal` through jump.c cross-jumping. Each arm of the
   inner select stores its own distance: with one store after the if/else,
   jump.c rewrites the select as "default, then conditional override", while
   per-arm stores leave the two `lui 1` heads for reorg to merge into the
   branch delay slot (the original's shape). */
void SetupFrameOT(void) {
  int dist;

  FillBlock16(&g_pOtDepthBinHead0, 0, 0x1c00);
  if (g_nCameraVisZone < g_nVisZoneCount) {
    g_nFarPlaneDrawDistance = 0x28000;
    RenderWorldChunks(g_nCameraVisZone);
  } else {
    if ((unsigned int)(g_nGamestate - 0xd) < 2) {
      dist = 0x1c000;
      g_nFarPlaneDrawDistance = dist;
    } else {
      dist = 0x14000;
      g_nFarPlaneDrawDistance = dist;
    }
    RenderWorldChunks(-1);
  }
}
