#include "actor_type.h"
#include "globals.h"
#include "main_shared.h"
#include "spu.h"

extern SPUVOICEROW g_aSpuVoiceRows[]; /* the 24 mixer rows */

/* Stop every software voice row owned by `owner` (0x800562a4). mode 1
   (VOICE_STOP_ALL) stops unconditionally; mode 2 (VOICE_STOP_LOOPING) only
   stops rows whose nFlags has VOICE_ROW_LOOPING. A stopped row silences its
   live sample byte (0x7F), clears pPlaying and pOwner, parks nFlags at
   VOICE_ROW_STOPPED, zeros nPitchStep, resets nSampleId to 0xFF, and
   accumulates the voice bit into the pending hardware key-off mask. 24 rows of
   g_aSpuVoiceRows, walked as three field pointers. */

void StopSoundVoicesByOwner(int owner, int mode) {
  unsigned int kmask = 0;
  int i = 0;
  int one = 1;
  int two = 2;
  int silence = VOICE_ROW_NONE;
  int idle = VOICE_ROW_STOPPED;
  int noSample = 0xFF;
  int *ownerp = (int *)&g_aSpuVoiceRows[0].pOwner;
  unsigned short *flagsp = &g_aSpuVoiceRows[0].nFlags;
  unsigned char **playingp = (unsigned char **)&g_aSpuVoiceRows[0].pPlaying;
  int off = 0;
top:
  if (*ownerp == owner) {
    if (mode == one || (mode == two && (*flagsp & VOICE_ROW_LOOPING) != 0)) {
      kmask |= one << i;
      if (*playingp != 0) {
        **playingp = silence;
      }
      *playingp = 0;
      *flagsp = idle;
      *ownerp = 0;
      VOICE_ROW_RAW(off, int, nPitchStep) = 0;
      VOICE_ROW_RAW(off, unsigned char, nSampleId) = noSample;
    }
  }
  ownerp = (int *)((SPUVOICEROW *)ownerp + 1);
  flagsp = (unsigned short *)((SPUVOICEROW *)flagsp + 1);
  playingp = (unsigned char **)((SPUVOICEROW *)playingp + 1);
  i += 1;
  off += 0x1C;
  if (i < 24) {
    goto top;
  }
  {
    unsigned int *pending = g_adwSpuPendingKeyOffBlock;
    *pending |= kmask;
  }
}
