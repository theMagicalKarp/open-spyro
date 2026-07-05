#include "globals.h"

/* Refill the null-terminated sprite/HUD primitive request queue (0x80018880):
   append each remaining 0x58-byte primitive record (from
   g_pSpriteRecordWriteCursor up to g_pSpriteRecordBufferTop) as a queue entry
   after the last live one, then re-terminate with NULL. Called near the end of
   GamestateDraw. */
void EnqueuePendingSpritePrims(void) {
  void **p;

  p = g_apSpritePrimQueue;
  if (g_apSpritePrimQueue[0] != 0) {
    p = g_apSpritePrimQueue;
    do {
      p = p + 1;
    } while (*p != 0);
  }
  if (g_pSpriteRecordWriteCursor != g_pSpriteRecordBufferTop) {
    do {
      *p = g_pSpriteRecordWriteCursor;
      g_pSpriteRecordWriteCursor =
          (void *)((int)g_pSpriteRecordWriteCursor + 0x58);
      p = p + 1;
    } while (g_pSpriteRecordWriteCursor != g_pSpriteRecordBufferTop);
  }
  *p = 0;
}
