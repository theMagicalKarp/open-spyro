#include "cutscene_path.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"

/* Enter the scripted-cutscene gamestate (0x8002d338, gamestate 0xE). The
   cutscene id is g_nCutsceneId: 0 title backdrop, 1 opening story, 2/3
   ending. Steps: reset the
   generic countdown fade and the path-table head, raise the storm flag for
   the opening story (id 1), then build the cutscene sample-bank entry at
   g_CutsceneSoundDef (sample 0x1010, per-id pitch from
   g_nCutsceneSamplePitch) and trigger it via PlaySoundEffect with the
   dragon-rescue voice marker. */
void BeginCutscene(void) {
  register int id;
  CutscenePath *head;
  int *entry;

  g_nGamestate = GS_CUTSCENE;
  g_nScreenFadeLevel = 0;
  head = g_pPathTableHead;
  id = g_nCutsceneId;
  head->frame = 0;
  if (id == 1) {
    g_nPortalDistanceFogEnabled = 1;
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
      (&g_nCutsceneSamplePitch)[id];
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchSpread) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, short, nPitchStep) = 0;
  SAMPLE_RAW(g_pActiveCutsceneSoundDef, int, nPitchMode) = 0;
  g_nSfxOverrideVolL = 0x3FFF;
  g_nSfxOverrideVolR = 0x3FFF;
  g_nSfxOverrideFlags = 1;

  PlaySoundEffect(0, 0, SFX_MODE_PANNED, &g_bSceneSfxVoiceMarker);
}
