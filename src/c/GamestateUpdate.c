#include "funcs.h"
#include "globals.h"
#include "main_shared.h"

/* Per-frame top-level update dispatcher (0x8003385C, 0x3f4).

   Runs the audio ticks, then dispatches the active gamestate's Update handler.
   Two of the arms hand off to the overlay resident at 0x8007xxxx: state 0xD
   runs the titlescreen entry unless its sub-mode reached 3, and state 0xF runs
   the credits entry until its sub-mode passes 0x63.

   State 0 is the gameplay frame itself: age the frame counter, bail out early
   when the death-plane check fires, run the world-chunk animations and the two
   level hooks around the Spyro frame tick, then the HUD/camera work. Both
   halves of the Spyro tick re-read g_nGamestate, since any of the handlers can
   switch state mid-frame. Finally, while genuinely in-level and unblocked, the
   Start/Select buttons open the pause and inventory menus (a disconnected or
   unknown pad type forces the pause menu). Every path ends in the SPU tick
   except the early returns. */
extern void Credits_Update(void);     /* credits overlay entry (0x8007AA50) */
extern void Titlescreen_Update(void); /* titlescreen overlay entry */

void GamestateUpdate(void) {
  g_nGamePaused = 0;
  TickActiveSoundVoices();
  TickCdMusicStream();

  if (g_nGamestate != 0) {
    if (g_nGamestate == 1) {
      Gamestate01_Update();
    } else if (g_nGamestate == 2) {
      PauseMenu_Update();
    } else if (g_nGamestate == 3) {
      SubMenu_Update();
    } else if (g_nGamestate == 4) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == 5) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == 6) {
      Gamestate06_Update();
    } else if (g_nGamestate == 7) {
      (*(void (*)())g_pfnGamestate7Handler)();
    } else if (g_nGamestate == 8) {
      GemPickup_Update();
    } else if (g_nGamestate == 9) {
      CameraSettle_Update();
    } else if (g_nGamestate == 0xA) {
      SpyroStateDelay_Update();
    } else if (g_nGamestate == 0xB) {
      Gamestate0B_Update();
    } else if (g_nGamestate == 0xC) {
      Gamestate0C_Update();
    } else if (g_nGamestate == 0xD) {
      if (g_nGamestate0dMode != 3) {
        Titlescreen_Update();
      } else {
        Gamestate0D_Update();
      }
    } else if (g_nGamestate == 0xE) {
      GemCutscene_Update();
    } else if (g_nGamestate == 0xF) {
      if (g_nGamestate0fMode >= 0x63) {
        Gamestate0F_Update();
      } else {
        Credits_Update();
      }
    }
  } else {
    g_dwGamestateFrames += 1;
    if (g_nDeathState != 0) {
      if (CheckDeathPlane()) {
        return;
      }
    }

    TickWorldChunkAnimations(g_nFrameStep);
    (*(void (*)())g_pfnLevelUpdateActors)();

    if (g_nGamestate == 0xB || g_nGamestate == 0xC || g_nGamestate == 7 ||
        g_nGamestate == 1) {
      return;
    }
    TickSpyroGameplayFrame();
    if (g_nGamestate == 4 || g_nGamestate == 5 || g_nGamestate == 7 ||
        g_nGamestate == 1) {
      return;
    }

    (*(void (*)())g_pfnLevelAdvanceEmitList)(g_nFrameStep);
    if (!g_nFlightLevelActive) {
      UpdateHudCounters();
    }
    ComputeBillboardBasisVectors();
    UpdateCameraFrame();

    if (g_nGenericCountdown != 0) {
      g_nGenericCountdown -= g_nFrameStep;
      if (g_nGenericCountdown < 0) {
        g_nGenericCountdown = 0;
      }
    }

    if (g_nGamestate == 0 && g_nGameplayBlocked == 0 &&
        g_nLevelReadyFlag >= 0 && g_nCameraCurrentMode != 0x8000000E) {
      if ((unsigned int)g_nPadType < 2 || (g_dwPadPressed & 0x800)) {
        EnterPauseMenu(1);
      } else if (g_dwPadPressed & 0x100) {
        EnterInventoryMenu(1);
      }
    }
  }

  TickSpuPerFrame();
}
