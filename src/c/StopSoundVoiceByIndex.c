#include "globals.h"

/* Queue an SPU key-off for voice `index` (0x80056200): set the pending key-off
   bit, and when `mode` is 2 or 4 also tear down the voice's common-attr shadow
   record (voice*28 stride): free the sample-callback slot, reset the ADSR/mode
   fields, and mark it stopped. */
extern unsigned int g_anSpuPendingKeyOffBlock[];

void StopSoundVoiceByIndex(int index, int mode) {
  unsigned int voice = index & 0x7F;
  int off;
  char *cb;

  if (voice < 24) {
    unsigned int *keyoff = g_anSpuPendingKeyOffBlock;
    *keyoff |= 1 << voice;
    if (mode == 2 || mode == 4) {
      off = voice * 28;
      cb = *(char **)((char *)g_abSpuCommonAttr + off + 0x40);
      if (cb != 0) {
        *cb = 0x7F;
      }
      *(short *)((char *)g_abSpuCommonAttr + off + 0x36) = 0x40;
      *(int *)((char *)g_abSpuCommonAttr + off + 0x40) = 0;
      *(int *)((char *)g_abSpuCommonAttr + off + 0x28) = 0;
      *(char *)((char *)g_abSpuCommonAttr + off + 0x35) = 0xFF;
    }
  }
}
