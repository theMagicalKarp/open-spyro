#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* World-hub special-cutscene dispatcher (0x8002d440). World 1: full story-intro
   restart — clear VRAM, silence sound, reload actor meshes, gamestate 0xD with
   the 0x5C-byte mode block zeroed (mode 3 = intro message 1), fresh game state,
   level 0xA. World 2: credits without a mesh reload. World 3: credits with a
   mesh reload, seeding g_nCreditsPageIndex = 0xA so BeginCreditsSequence keeps
   it. */

void DispatchSpecialCutscene(void) {
  RECT rect;
  int world = g_nCurrentWorldId;
  if (world == 1) {
    int *mode = g_anGamestate0dBlock;
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
    g_nActiveLevelId = 0xA;
    g_nGamePaused = 1;
  } else if (world == 2) {
    BeginCreditsSequence(0);
    g_nGamePaused = 1;
  } else if (world == 3) {
    g_nCreditsPageIndex = 0xA;
    BeginCreditsSequence(1);
    g_nGamePaused = 1;
  }
}
