#include "globals.h"
#include "main_shared.h"
#include "spu.h"

extern SPUVOICEROW g_aSpuVoiceRows[]; /* the 24 mixer rows */

/* Stop every software voice row owned by `owner` (0x800562a4). mode 1
   (VOICE_STOP_ALL) stops unconditionally; mode 2 (VOICE_STOP_LOOPING) only
   stops rows whose nFlags has bit 0x100 set. A stopped row silences its live
   sample byte (0x7F), clears pPlaying and pOwner, parks nFlags at idle (0x40),
   zeros nPitchStep, resets nSampleId to 0xFF, and accumulates the voice bit
   into the pending hardware key-off mask. 24 rows of g_aSpuVoiceRows, walked as
   three field pointers. */

void StopSoundVoicesByOwner(int owner, int mode) {
  unsigned int kmask = 0;
  int i = 0;
  int one = 1;
  int two = 2;
  int silence = 0x7F;
  int idle = 0x40;
  int ff = 0xFF;
  int *po = (int *)&g_aSpuVoiceRows[0].pOwner;
  unsigned short *pf = &g_aSpuVoiceRows[0].nFlags;
  unsigned char **ps = (unsigned char **)&g_aSpuVoiceRows[0].pPlaying;
  int off = 0;
top:
  if (*po == owner) {
    if (mode == one || (mode == two && (*pf & 0x100) != 0)) {
      kmask |= one << i;
      if (*ps != 0) {
        **ps = silence;
      }
      *ps = 0;
      *pf = idle;
      *po = 0;
      VOICE_ROW_RAW(off, int, nPitchStep) = 0;
      VOICE_ROW_RAW(off, unsigned char, nSampleId) = ff;
    }
  }
  po = (int *)((SPUVOICEROW *)po + 1);
  pf = (unsigned short *)((SPUVOICEROW *)pf + 1);
  ps = (unsigned char **)((SPUVOICEROW *)ps + 1);
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
