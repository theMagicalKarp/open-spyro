#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Enter the gem-cutscene gamestate (0x8002d338, gamestate 0xE): reset the
   generic countdown fade and the path-table head, raise the storm flag when
   entering from world 1, then build the cutscene sample-bank entry at
   g_CutsceneSoundDef (sample 0x1010, per-world pitch from
   g_nWorldCutsceneSamplePitch) and trigger it via PlaySoundEffect with the
   gem-pickup voice marker. */
/* volatile alias of g_pLevelSampleBankEntries — the entry writes below reload
   the pointer global before every store */

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

  entry = g_CutsceneSoundDef;
  g_pActiveCutsceneSoundDef = (char *)entry;
  *((int *)g_pActiveCutsceneSoundDef) = 0x1010;
  *((int *)(g_pActiveCutsceneSoundDef + 4)) = -1;
  *((short *)(g_pActiveCutsceneSoundDef + 8)) = 0x50;
  *((short *)(g_pActiveCutsceneSoundDef + 0xA)) =
      (&g_nWorldCutsceneSamplePitch)[world];
  *((short *)(g_pActiveCutsceneSoundDef + 0xC)) = 0;
  *((short *)(g_pActiveCutsceneSoundDef + 0xE)) = 0;
  *((int *)(g_pActiveCutsceneSoundDef + 0x10)) = 0;
  g_nSfxOverrideVolL = 0x3FFF;
  g_nSfxOverrideVolR = 0x3FFF;
  g_nSfxOverrideFlags = 1;

  PlaySoundEffect(0, 0, 0x10, &g_bGemPickupSfxVoiceMarker);
}
