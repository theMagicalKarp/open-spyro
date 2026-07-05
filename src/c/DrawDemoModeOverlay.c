#include "globals.h"

extern unsigned char D_80010AC0[];
extern void *BuildTextSprites(unsigned char *str, int *pos, int *step, int a,
                              int b);
extern void EnqueuePendingSpritePrims(void);

/* Build the "DEMO MODE" text overlay (0x80018908) via BuildTextSprites, then
   phase-pulse each glyph's alpha (+0x46) from the sine LUT indexed by the
   vblank tick, and hand the records to the sprite queue via
   EnqueuePendingSpritePrims. */
void DrawDemoModeOverlay(void) {
  void *pvVar1;
  int iVar3;
  int iVar2;
  int local_28[4];
  int local_18[4];

  pvVar1 = g_pSpriteRecordWriteCursor;
  local_18[0] = 0x10;
  local_18[1] = 1;
  local_18[2] = 0x1400;
  local_28[0] = 199;
  local_28[1] = 200;
  local_28[2] = 0x1100;
  BuildTextSprites(D_80010AC0, local_28, local_18, 0x12, 2);
  iVar3 = (int)pvVar1 - 0x58;
  iVar2 = 0;
  if ((int)g_pSpriteRecordWriteCursor <= iVar3) {
    do {
      *(char *)(iVar3 + 0x46) =
          ((unsigned short *)
               g_anSineLut)[((g_nVblankTickCount * 4 + iVar2) & 0xff) + 0x40] >>
          7;
      iVar3 = iVar3 - 0x58;
      iVar2 = iVar2 + 0xc;
    } while ((int)g_pSpriteRecordWriteCursor <= iVar3);
  }
  EnqueuePendingSpritePrims();
}
