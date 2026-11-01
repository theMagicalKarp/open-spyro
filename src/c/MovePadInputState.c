#include "funcs.h"
#include "globals.h"

/* Move the 0xA4-byte pad state block (g_dwPadPressed..) from `from` to `to`,
   then clear `from` (ResetPadFrameSubstepTable). Toggles g_bPadInputStashed.
   Spyro's frame ticks call it in pairs: live -> g_dwPadStash before the frame
   (input blanked while g_nSpyroInputBlankCountdown runs), then back after it.
   (0x80053708, 84 bytes.) */
void MovePadInputState(void *from, void *to) {
  g_bPadInputStashed = 1 - g_bPadInputStashed;
  CopyWords(to, from, 0xA4);
  ResetPadFrameSubstepTable(from);
}
