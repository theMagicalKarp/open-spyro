#include "actor.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* Push a blinking SPRT_16 marker onto the front of the sprite queue
   (0x80018534): g_pSpriteRecordWriteCursor bumps down 0x58 bytes. Gated by
   (frame & 0x1f) < 0x10 so it shows for half of each 32-frame cycle;
   tile < 2 sets rotX = 0x40 and rotZ = tile * 0x80. The marker is the '1'
   digit glyph model (text glyphs are `ch + 0xD4`, BuildTextSprites), turned
   by those angles. */
void EnqueueBlinkingMarkerSprite(int *pos, int frame, int tile) {
  int *dst;

  if ((frame & 0x1f) < 0x10) {
    g_pSpriteRecordWriteCursor--;
    FillWord(g_pSpriteRecordWriteCursor, 0, 0x58);
    dst = &g_pSpriteRecordWriteCursor->posX;
    g_pSpriteRecordWriteCursor->type = ACTOR_TYPE_GEM_DIGIT_0 + 1;
    CopyVector(dst, pos);
    if (tile < 2) {
      g_pSpriteRecordWriteCursor->rotX = 0x40;
      g_pSpriteRecordWriteCursor->rotZ = tile << 7;
    }
    g_pSpriteRecordWriteCursor->otBias = 0x7f;
    g_pSpriteRecordWriteCursor->tintMode = 0xb;
    g_pSpriteRecordWriteCursor->cullRadius = 0xff;
  }
}
