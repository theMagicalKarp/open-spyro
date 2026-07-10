#include "globals.h"

extern int strlen(char *s);
extern void *BuildTextSpriteChain(char *text, int *pos, int height, int flags);

/* 0x80018728 (0x150) — draw the dragon-rescue banner: "RESCUED" plus the
   freed dragon's name (g_apDragonNames[actor->id at +0x38]) as two
   BuildTextSpriteChain labels right-shifted by the name length, then walk
   the sprite records the two calls appended (queue grows downward) and pulse
   each glyph's alpha byte (+0x46) from the sine LUT off the gem-pickup
   timer, phase-stepped 0xC per glyph. */
void DrawDragonRescuedBanner(void) {
  int pos[3];
  char *name;
  unsigned char *rec;
  int w;
  int phase;

  name = g_apDragonNames[*(int *)(*(char **)g_pGemPickupSourceActor + 0x38)];
  w = (strlen(name) - 1) * 13;
  rec = (unsigned char *)g_pSpriteRecordWriteCursor;

  pos[0] = 0xB0 - w;
  pos[1] = 0xC8;
  pos[2] = 0x1000;
  BuildTextSpriteChain(g_szUiRescued, pos, 0x14, 2);

  pos[0] = 0x150 - w;
  pos[1] = 0xC8;
  pos[2] = 0xC00;
  BuildTextSpriteChain(name, pos, 0x1A, 2);

  phase = 0;
  rec -= 0x58;
  if ((int)rec >= (int)g_pSpriteRecordWriteCursor) {
    int *timer = &g_nGemPickupSecondaryTimer;
    unsigned short *lut = (unsigned short *)g_anSineLut + 64;
    do {
      int idx = (*timer * 2 + phase) & 0xFF;
      rec[0x46] = *(unsigned short *)((idx << 1) + (int)lut) >> 7;
      rec -= 0x58;
      phase += 0xC;
    } while ((int)rec >= (int)g_pSpriteRecordWriteCursor);
  }
}
