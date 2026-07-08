#include "globals.h"

/* Stop every software voice row owned by `owner` (0x800562a4). mode 1 stops
   unconditionally; mode 2 only stops rows whose flags halfword (+0x36) has
   bit 0x100 set. A stopped row silences its live sample byte (0x7F), clears
   the sample pointer (+0x40) and owner word (+0x28), parks the flags at
   idle (0x40), zeros +0x3C, resets the priority byte (+0x35) to 0xFF, and
   accumulates the voice bit into the pending hardware key-off mask. Records
   are 0x1C bytes, 24 voices. */
extern unsigned int g_adwSpuPendingKeyOffBlock[];

void StopSoundVoicesByOwner(int owner, int mode) {
  unsigned int kmask = 0;
  int i = 0;
  int one = 1;
  int two = 2;
  int silence = 0x7F;
  int idle = 0x40;
  int ff = 0xFF;
  int *po = (int *)((char *)g_abSpuCommonAttr + 0x28);
  unsigned short *pf = (unsigned short *)((char *)po + 0xE);
  unsigned char **ps = (unsigned char **)((char *)po + 0x18);
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
      *(int *)((char *)g_abSpuCommonAttr + off + 0x3C) = 0;
      *(unsigned char *)((char *)g_abSpuCommonAttr + off + 0x35) = ff;
    }
  }
  po = (int *)((char *)po + 0x1C);
  pf = (unsigned short *)((char *)pf + 0x1C);
  ps = (unsigned char **)((char *)ps + 0x1C);
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
