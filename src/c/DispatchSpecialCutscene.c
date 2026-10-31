#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"

/* Special-cutscene dispatcher (0x8002d440), by g_nCutsceneId. Cutscene 1: full
   story-intro restart — clear VRAM, silence sound, reload actor meshes,
   gamestate 0xD with the 0x5C-byte mode block zeroed (mode 3 = intro message
   1), fresh game state, level 0xA. Cutscene 2: credits without a mesh reload.
   Cutscene 3: credits with a mesh reload, seeding g_nCreditsPageIndex = 0xA so
   BeginCreditsSequence keeps it. */

void DispatchSpecialCutscene(void) {
  RECT rect;
  int id = g_nCutsceneId;
  if (id == 1) {
    int *mode = g_anTitleModeBlock;
    rect.w = 0x200;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0x1E0;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    StopAllSoundExceptMask(0);
    TickSpuPerFrame();
    LoadActorMeshTables();
    g_nGamestate = GS_TITLE;
    FillWord(mode, 0, 0x5C);
    mode[0] = 3;
    g_nStoryIntroSubstate = 0;
    g_nStoryIntroMessage = 1;
    InitNewGameState();
    g_nActiveLevelId = LEVEL_ARTISANS;
    g_nGamePaused = 1;
  } else if (id == 2) {
    BeginCreditsSequence(0);
    g_nGamePaused = 1;
  } else if (id == 3) {
    g_nCreditsPageIndex = 0xA;
    BeginCreditsSequence(1);
    g_nGamePaused = 1;
  }
}
