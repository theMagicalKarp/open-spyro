#include "globals.h"

extern void FillWord(void *dst, unsigned int value, int byte_count);
extern void CopyVector(int *dst, int *src);

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
    dst = (int *)((int)g_pSpriteRecordWriteCursor + 0xc);
    *(short *)((int)g_pSpriteRecordWriteCursor + 0x36) = 0x105;
    CopyVector(dst, param_1);
    if (param_3 < 2) {
      *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x44) = 0x40;
      *(char *)((int)g_pSpriteRecordWriteCursor + 0x46) = param_3 << 7;
    }
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x47) = 0x7f;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x4f) = 0xb;
    *(unsigned char *)((int)g_pSpriteRecordWriteCursor + 0x50) = 0xff;
  }
}
