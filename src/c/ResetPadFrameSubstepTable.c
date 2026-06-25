#include "globals.h"

/* Reset the per-frame pad sub-step table. The header (base[0..7]) is zeroed
   except for the 0x7F7F7F7F neutral-stick word at +0x14 and the two enable
   flags at +0x18/+0x1C. Then four 0x18-byte sub-step entries (starting at
   +0x44) are cleared: each gets its three counters zeroed, the neutral-stick
   word at +0x58, and copies base[3] into its +0x44 field. */
void ResetPadFrameSubstepTable(int *base) {
  int i;

  base[5] = 0x7F7F7F7F;
  base[2] = 0;
  base[0] = 0;
  base[1] = 0;
  base[6] = 1;
  base[7] = 1;
  for (i = 0; i < 4; i++) {
    int t = base[3];
    base[i * 6 + 0x12] = 0;
    base[i * 6 + 0x13] = 0;
    base[i * 6 + 0x14] = 0;
    base[i * 6 + 0x16] = 0x7F7F7F7F;
    base[i * 6 + 0x11] = t;
  }
}
