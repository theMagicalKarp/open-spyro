#include "actor_type.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Build the "DEMO MODE" text overlay (0x80018908) via BuildTextSprites, then
   phase-pulse each glyph record's rotZ byte (+0x46) from the sine LUT indexed
   by the vblank tick, and hand the records to the sprite queue via
   EnqueuePendingSpritePrims. */
void DrawDemoModeOverlay(void) {
  void *first;
  int glyph;
  int phase;
  int pos[4];
  int size[4];

  first = g_pSpriteRecordWriteCursor;
  size[0] = 0x10;
  size[1] = 1;
  size[2] = 0x1400;
  pos[0] = 199;
  pos[1] = 200;
  pos[2] = 0x1100;
  BuildTextSprites(g_szDemoMode, pos, size, 0x12, 2);
  glyph = (int)first - (int)sizeof(Actor);
  phase = 0;
  if ((int)g_pSpriteRecordWriteCursor <= glyph) {
    do {
      ((Actor *)glyph)->rotZ =
          ((unsigned short *)
               g_anSineLut)[((g_nVblankTickCount * 4 + phase) & 0xff) + 0x40] >>
          7;
      glyph -= sizeof(Actor);
      phase += 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= glyph);
  }
  EnqueuePendingSpritePrims();
}
