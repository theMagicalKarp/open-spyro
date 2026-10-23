#include "globals.h"
#include "spu.h"

/* Queue an SPU key-off for voice `index` (0x80056200): set the pending key-off
   bit, and when `mode` is 2 or 4 also tear down the voice's common-attr shadow
   record (voice*28 stride): free the sample-callback slot, reset the ADSR/mode
   fields, and mark it stopped. */

void StopSoundVoiceByIndex(int index, int mode) {
  unsigned int voice = index & 0x7F;
  int off;
  char *cb;

  if (voice < 24) {
    unsigned int *keyoff = g_anSpuPendingKeyOffBlock;
    *keyoff |= 1 << voice;
    if (mode == 2 || mode == 4) {
      off = voice * 28;
      cb = VOICE_ROW_RAW(off, char *, pPlaying);
      if (cb != 0) {
        *cb = 0x7F;
      }
      VOICE_ROW_RAW(off, short, nFlags) = 0x40;
      VOICE_ROW_RAW(off, int, pPlaying) = 0;
      VOICE_ROW_RAW(off, int, pOwner) = 0;
      VOICE_ROW_RAW(off, char, nSampleId) = 0xFF;
    }
  }
}
