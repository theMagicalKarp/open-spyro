#include "actor.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

#define SPR ((Actor *)g_pSpriteRecordWriteCursor)

/* Push a blinking SPRT_16 marker onto the front of the sprite queue
   (0x80018534): g_pSpriteRecordWriteCursor bumps down 0x58 bytes. Gated by
   (param_2 & 0x1f) < 0x10 so it shows for half of each 32-frame cycle;
   param_3 < 2 selects tile U=0 vs U=0x80. */
void EnqueueBlinkingMarkerSprite(int *param_1, int param_2, int param_3) {
  int *dst;

  if ((param_2 & 0x1f) < 0x10) {
    g_pSpriteRecordWriteCursor =
        (void *)((int)g_pSpriteRecordWriteCursor - 0x58);
    FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
    dst = &SPR->posX;
    SPR->type = 0x105;
    CopyVector(dst, param_1);
    if (param_3 < 2) {
      SPR->rotX = 0x40;
      SPR->rotZ = param_3 << 7;
    }
    SPR->otBias = 0x7f;
    SPR->tintMode = 0xb;
    SPR->cullRadius = 0xff;
  }
}
