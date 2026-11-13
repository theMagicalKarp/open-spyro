#include "frame_arena.h"
#include "funcs.h"
#include "gamestate.h"
#include "globals.h"
#include "main_shared.h"

/* Mid-frame scene composer (0x80019698, 164 bytes): sequences the actor /
   sprite / effect passes of one frame — actor draw lists, a FillWord clear of
   the OT depth-bin heads, sprite prims, actor drop shadows; Spyro's own passes
   (RasterizeSpyroModel + drop shadow) unless g_nSpyroDrawSuppressed; flame
   trails only while g_bSpyroFlameActive; then the world
   particle emitters. */

void ComposeFrameScene(void) {
  BuildActorDrawList();
  FillWord(&g_apMainDrawEntityList, 0, FRAME_ARENA_LIST_WORDS);
  EmitActorDrawList();
  BuildSecondaryActorDrawList();
  EmitSecondaryActorPrimitives();
  RasterizeSpritePrimQueue();
  DrawActorDropShadows();
  if (g_nSpyroDrawSuppressed == 0) {
    RasterizeSpyroModel();
    DrawSpyroDropShadow();
  }
  if (g_bSpyroFlameActive != 0) {
    DrawSpyroFlameTrails();
  }
  DrawWorldParticleEmitters();
}

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

  FillBlock16(&g_apMainDrawEntityList, 0, 0x1c00);
  if (g_nCameraVisZone < g_nVisZoneCount) {
    g_nFarPlaneDrawDistance = 0x28000;
    RenderWorldChunks(g_nCameraVisZone);
  } else {
    if ((unsigned int)(g_nGamestate - GS_TITLE) <
        2) { /* GS_TITLE, GS_CUTSCENE */
      dist = 0x1c000;
      g_nFarPlaneDrawDistance = dist;
    } else {
      dist = 0x14000;
      g_nFarPlaneDrawDistance = dist;
    }
    RenderWorldChunks(-1);
  }
}
