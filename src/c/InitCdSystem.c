#include "globals.h"

extern int CdInit(void);
extern int CdControl(unsigned char cmd, unsigned char *param,
                     unsigned char *result);
extern int CdReadCallback(unsigned int cb);
extern int CdMix(unsigned char *atv);
extern void func_80016490(void);

extern int
    g_anMusicStreamBlock[]; /* shared-base view of g_dwMusicStreamStatus */
extern signed char D_800776D1;
extern signed char D_800776D2;
extern signed char D_800776D3;

/* Bring up the CD subsystem at boot: init the drive, set double-speed read
   mode (CdlSetmode 0x80), install the async read callback, then prime the
   music-stream status word and the CdMix attenuator buffer (0x7F,0,0x7F,0).
   (0x80012480, 140 bytes.) */
void InitCdSystem(void) {
  unsigned char mode;
  mode = 0x80;
  CdInit();
  CdControl(0xE, &mode, 0);
  CdReadCallback((unsigned int)func_80016490);
  g_anMusicStreamBlock[0] = 0x40;
  g_nLastCdMusicCommand = 0;
  g_nPendingMusicCommand = 0;
  D_800776D2 = 0x7F;
  *((unsigned char *)g_anMusicStreamBlock + 0x21C) = 0x7F;
  D_800776D3 = 0;
  D_800776D1 = 0;
  CdMix((unsigned char *)g_anMusicStreamBlock + 0x21C);
}
