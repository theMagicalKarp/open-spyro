#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"

/* Enter the gem-cutscene gamestate (0x8002d338, gamestate 0xE): reset the
   generic countdown fade and the path-table head, raise the storm flag when
   entering from world 1, then build the cutscene sample-bank entry at
   g_CutsceneSoundDef (sample 0x1010, per-world pitch from
   g_nWorldCutsceneSamplePitch) and trigger it via PlaySoundEffect with the
   gem-pickup voice marker. */
void BeginGemCutscene(void) {
  register int world;
  int *head;
  int *entry;

  g_nGamestate = GS_GEM_CUTSCENE;
  g_nGenericCountdown = 0;
  head = (int *)g_pPathTableHead;
  world = g_nCurrentWorldId;
  *head = 0;
  if (world == 1) {
    g_nLevelStormFlag = 1;
  }

  entry = g_CutsceneSoundDef;
  g_pActiveCutsceneSoundDef = (char *)entry;
  /* g_pActiveCutsceneSoundDef is the volatile view of
     g_pLevelSampleBankEntries: the original reloads the pointer before every
     field store. */
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nSampleAddr) = 0x1010;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nLoopAddr) = -1;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nVolume) = 0x50;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitch) =
      (&g_nWorldCutsceneSamplePitch)[world];
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchSpread) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchStep) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nPitchMode) = 0;
  g_nSfxOverrideVolL = 0x3FFF;
  g_nSfxOverrideVolR = 0x3FFF;
  g_nSfxOverrideFlags = 1;

  PlaySoundEffect(0, 0, 0x10, &g_bGemPickupSfxVoiceMarker);
}
