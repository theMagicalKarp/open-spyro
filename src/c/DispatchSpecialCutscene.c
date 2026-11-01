#include "cutscene_id.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "title_state.h"

/* Special-cutscene dispatcher (0x8002d440), by g_nCutsceneId, at the end of a
   cutscene. The opening: full story-intro restart — clear VRAM, silence sound,
   reload actor meshes, gamestate 0xD with the 0x5C-byte mode block zeroed
   (TITLE_MODE_STORY_INTRO, exit path TITLE_EXIT_LOADED_GAME), fresh game state,
   level 0xA. The Gnasty ending: credits without a mesh reload. The Loot ending:
   credits with a mesh reload, seeding g_nCreditsPageIndex = 0xA so
   BeginCreditsSequence keeps it. */

void DispatchSpecialCutscene(void) {
  RECT rect;
  int id = g_nCutsceneId;
  if (id == CUTSCENE_OPENING) {
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
    mode[0] = TITLE_MODE_STORY_INTRO;
    g_nStoryIntroSubstate = STORY_INTRO_ARM;
    g_nTitleExitPath = TITLE_EXIT_LOADED_GAME;
    InitNewGameState();
    g_nActiveLevelId = LEVEL_ARTISANS;
    g_nGamePaused = 1;
  } else if (id == CUTSCENE_GNASTY_ENDING) {
    BeginCreditsSequence(0);
    g_nGamePaused = 1;
  } else if (id == CUTSCENE_LOOT_ENDING) {
    g_nCreditsPageIndex = 0xA;
    BeginCreditsSequence(1);
    g_nGamePaused = 1;
  }
}
