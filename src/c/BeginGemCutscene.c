#include "globals.h"

/* Enter the gem-cutscene gamestate (0x8002d338, gamestate 0xE): reset the
   generic countdown fade and the path-table head, raise the storm flag when
   entering from world 1, then build the cutscene sample-bank entry at
   D_800778F0 (sample 0x1010, per-world pitch from g_nWorldCutsceneSamplePitch)
   and trigger it via PlaySoundEffect with the gem-pickup voice marker. */
extern int PlaySoundEffect(unsigned int sample, int a, unsigned int b, void *c);
extern int D_800778F0[];
/* volatile alias of g_pLevelSampleBankEntries — the entry writes below reload
   the pointer global before every store */
extern char *volatile D_800761D0;
extern int D_800761DC;
extern short D_800761E8;
extern short D_800761EA;

void BeginGemCutscene(void) {
  register int world;
  int *head;
  int *entry;

  g_nGamestate = 0xE;
  g_nGenericCountdown = 0;
  head = (int *)g_pPathTableHead;
  world = g_nCurrentWorldId;
  *head = 0;
  if (world == 1) {
    g_nLevelStormFlag = 1;
  }

  entry = D_800778F0;
  D_800761D0 = (char *)entry;
  *((int *)D_800761D0) = 0x1010;
  *((int *)(D_800761D0 + 4)) = -1;
  *((short *)(D_800761D0 + 8)) = 0x50;
  *((short *)(D_800761D0 + 0xA)) = (&g_nWorldCutsceneSamplePitch)[world];
  *((short *)(D_800761D0 + 0xC)) = 0;
  *((short *)(D_800761D0 + 0xE)) = 0;
  *((int *)(D_800761D0 + 0x10)) = 0;
  D_800761E8 = 0x3FFF;
  D_800761EA = 0x3FFF;
  D_800761DC = 1;

  PlaySoundEffect(0, 0, 0x10, &g_bGemPickupSfxVoiceMarker);
}
