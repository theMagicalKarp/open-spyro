#include "cd_music.h"
#include "funcs.h"
#include "globals.h"
#include "libcd.h"
#include "main_shared.h"

/* The CdMix attenuation bytes (g_bCdMixLeftToLeft.., 0x800776d0) reached
   from the music-stream block's held base, as the original addresses them. */
#define CD_MIX_ATV ((unsigned char *)g_anMusicStreamBlock + 0x21C)

/* Bring up the CD subsystem at boot: init the drive, set double-speed read
   mode (CdlSetmode 0x80), install the async read callback, then prime the
   music-stream status word and the CdMix attenuator buffer (0x7F,0,0x7F,0).
   (0x80012480, 140 bytes.) */
void InitCdSystem(void) {
  unsigned char mode;
  mode = CdlModeSpeed;
  CdInit();
  CdControl(CdlSetmode, &mode, 0);
  CdReadCallback((unsigned int)CdReadCompleteCallback);
  g_anMusicStreamBlock[0] = MUSIC_ST_IDLE;
  g_nLastCdMusicCommand = 0;
  g_nPendingMusicCommand = 0;
  g_bCdMixRightToRight = 0x7F;
  CD_MIX_ATV[0] = 0x7F; /* g_bCdMixLeftToLeft */
  g_bCdMixRightToLeft = 0;
  g_bCdMixLeftToRight = 0;
  CdMix(CD_MIX_ATV);
}
