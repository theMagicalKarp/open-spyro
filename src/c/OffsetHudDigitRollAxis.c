#include "globals.h"

/* Shift the Y origin of `count` consecutive HUD digit sprites by `delta`,
   starting at digit slot `base`. Each HUD rect record is 8 bytes (4 shorts);
   the Y field sits at byte offset 2. The rolled positions are read from the
   rest-pose table and written into the live sprite table. (0x8005445c, 76
   bytes.) */
void OffsetHudDigitRollAxis(int base, int count, int delta) {
  int i;
  for (i = 0; i < count; i++) {
    int off = (base + i) << 3;
    *(short *)((char *)g_anHudSpriteRects + off + 2) =
        *(unsigned short *)((char *)g_anHudDigitRestRects + off + 2) + delta;
  }
}
