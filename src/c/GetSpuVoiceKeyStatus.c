#include "globals.h"

/* SpuGetKeyStatus-style voice key-state query (0x8005c9d0): find the lowest
   voice selected by `mask` (24 voices), then classify it against the live
   key-on mask and its envelope halfword (+0xC, stride 0x10 off the voice
   record base): keyed-on -> 3 (env dead) or 1 (on); keyed-off -> 2 (env
   still live) or 0 (off). No voice selected returns -1. */
extern char *D_80073554;

int GetSpuVoiceKeyStatus(unsigned int mask) {
  int found = -1;
  int bit = 0;
  int one = 1;
  do {
    int m = one << bit;
    if (mask & m) {
      found = bit;
      break;
    }
    bit += 1;
  } while (bit < 24);
  if (found == -1) {
    return -1;
  }
  {
    int envx = *(unsigned short *)((found << 4) + (int)D_80073554 + 0xC);
    int kmask = 1 << found;
    if (g_dwSpuLiveKeyOnMask & kmask) {
      if (envx != 0) {
        return 1;
      }
      return 3;
    }
    return (envx != 0) << 1;
  }
}
