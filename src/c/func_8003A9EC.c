#include "globals.h"

/* Pick an actor's LOD band and point its mesh-table entry at the matching
   VRAM page (0x8003a9ec). `level` 0 with no flags means "leave it alone";
   otherwise the band is nudged (out to 0x50 for flag-0x10000 actors, in by 2
   above 0x20, in by 1 below) and the quarter-band index selects the page
   (0x1000 across, 0x40 down, less one byte per index) stored at +0x4C.
   Returns the adjusted band. */
int func_8003A9EC(int *actor, int level) {
  int n;
  int addr;

  if (level != 0 || actor[6] != 0) {
    if ((actor[6] & 0x10000) && level < 0x50) {
      level += 0x10;
    } else if (level >= 0x21) {
      level -= 2;
    } else if (level > 0) {
      level -= 1;
    }
    n = level >> 2;
    addr = 0xA00000 + ((n + 0x18) << 12) + ((0x18 - n) << 6) + 0x18;
    actor[19] = addr - n;
  }
  return level;
}
