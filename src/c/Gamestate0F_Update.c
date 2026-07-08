#include "globals.h"

/* Gamestate 0x0F updater (0x800333dc) — the credits/return-home transition.
   Spins the light vector each frame. Mode 0x63 arms the CD stream and picks
   the destination level: a pending credits-return level if one is latched,
   otherwise Gnasty's Loot (0x3C), or Dream Weavers home (0xA) once every
   treasure (0x36B0 gems) is collected; then clears the scripted-cutscene
   state and advances to mode 0x64, which streams the level transition. */
extern void RotateLightVectorXZ();
extern void TickLevelTransitionStream();

void Gamestate0F_Update(void) {
  RotateLightVectorXZ(3);
  switch (g_nGamestate0fMode) {
  case 0x63:
    g_nCdStreamState = 1;
    if (g_nCreditsReturnLevelId != -1) {
      g_nCurrentLevelId = g_nCreditsReturnLevelId;
      g_nCreditsReturnLevelId = -1;
    } else {
      if (g_nWorldTreasureCollected == 0x36B0) {
        g_nCurrentLevelId = 0xA;
      } else {
        g_nCurrentLevelId = 0x3C;
      }
    }
    g_nScriptedMobTag = 0;
    g_nScriptedRespawnFlag = 0;
    g_nReturningHomePhase = 0;
    g_nGamestate0fMode += 1;
    break;
  case 0x64:
    g_nReturningHomePhase += g_nFrameStep;
    TickLevelTransitionStream(1);
    break;
  }
}
