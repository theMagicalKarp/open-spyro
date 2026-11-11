#include "actor_type.h"
#include "frame_db.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Draw GS_CREDITS's post-credits "RETURNING HOME..." card with the animated
   per-glyph brightness wave (g_anCosineLut stepped by g_nReturningHomePhase),
   then the standard sprite-queue rasterize + frame-flush tail (0x8001eb80). */
void ReturningHome_Draw(void) {
  Actor *first;
  void **queue;
  int phase;
  Actor *glyph;
  int sizeVec[3];
  int posVec[3];

  first = g_pSpriteRecordWriteCursor;
  sizeVec[0] = 0x10;
  sizeVec[1] = 1;
  sizeVec[2] = TEXT_SMALL_DEPTH;
  posVec[0] = 0x76;
  posVec[1] = 0x6e;
  posVec[2] = TEXT_DEPTH;
  g_FrameDrawEnv0.r0 = 0;
  g_FrameDrawEnv0.g0 = 0;
  g_FrameDrawEnv0.b0 = 0;
  g_FrameDrawEnv1.r0 = 0;
  g_FrameDrawEnv1.g0 = 0;
  g_FrameDrawEnv1.b0 = 0;
  BuildTextSprites(g_szReturningHome, posVec, sizeVec, 0x12, TINT_HUD);
  glyph = first - 1;
  phase = 0;
  if ((int)g_pSpriteRecordWriteCursor <= (int)glyph) {
    do {
      glyph->rotZ =
          ((unsigned short *)
               g_anCosineLut)[(g_nReturningHomePhase * 2 + phase) & 0xff] >>
          7;
      glyph--;
      phase += 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= (int)glyph);
  }
  queue = g_apSpritePrimQueue;
  queue[0] = 0;
  EnqueuePendingSpritePrims();
  FillWord((char *)queue - FRAME_ARENA_SPRITE_QUEUE, 0, FRAME_ARENA_LIST_WORDS);
  RasterizeSpritePrimQueue();
  DrawSync(0);
  g_nVsyncFrameEndCount = VSync(-1);
  if (g_nVsyncFrameEndCount - g_nVsyncFramePaceAnchor < 2) {
    int *end = g_anVsyncFrameEndBlock;
    do {
      VSync(0);
      g_nVsyncFrameEndCount = VSync(-1);
    } while (g_nVsyncFrameEndCount - end[-1] < 2);
  }
  g_nVsyncFramePaceAnchor = VSync(-1);
  PutDispEnv(&FRAME_DB(g_pActiveFrameDrawEnv)->disp);
  PutDrawEnv(g_pActiveFrameDrawEnv);
  DrawOTag(LinkOTPrimitives(OT_BIN_COUNT));
}
