#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"

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

void GamestateUpdate(void) {
  g_nGamePaused = 0;
  TickActiveSoundVoices();
  TickCdMusicStream();

  if (g_nGamestate != GS_GAMEPLAY) {
    if (g_nGamestate == GS_LEVEL_LOAD) {
      LevelLoad_Update();
    } else if (g_nGamestate == GS_PAUSE_MENU) {
      PauseMenu_Update();
    } else if (g_nGamestate == GS_INVENTORY) {
      SubMenu_Update();
    } else if (g_nGamestate == GS_RESPAWN) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == GS_GAME_OVER) {
      RespawnOrGameOver_Update();
    } else if (g_nGamestate == GS_OVERLAY_SCREEN) {
      OverlayScreen_Update();
    } else if (g_nGamestate == GS_OVERLAY_HOOK) {
      (*(void (*)())g_pfnGamestate7Handler)();
    } else if (g_nGamestate == GS_DRAGON_RESCUE) {
      DragonRescue_Update();
    } else if (g_nGamestate == GS_ARRIVAL_CAMERA) {
      CameraSettle_Update();
    } else if (g_nGamestate == GS_MENU_TRANSITION) {
      SpyroStateDelay_Update();
    } else if (g_nGamestate == GS_SAVE_MENU) {
      SaveMenu_Update();
    } else if (g_nGamestate == GS_ACTOR_CUTSCENE) {
      ActorCutscene_Update();
    } else if (g_nGamestate == GS_TITLE) {
      if (g_nTitleMode != 3) {
        Titlescreen_Update();
      } else {
        Title_Update();
      }
    } else if (g_nGamestate == GS_CUTSCENE) {
      Cutscene_Update();
    } else if (g_nGamestate == GS_CREDITS) {
      if (g_nCreditsMode >= 0x63) {
        CreditsSequence_Update();
      } else {
        Credits_Update();
      }
    }
  } else {
    g_dwGamestateFrames += 1;
    if (g_nDemoMode != 0) {
      if (TickDemoPlayback()) {
        return;
      }
    }

    TickWorldChunkAnimations(g_nFrameStep);
    (*(void (*)())g_pfnLevelUpdateActors)();

    if (g_nGamestate == GS_SAVE_MENU || g_nGamestate == GS_ACTOR_CUTSCENE ||
        g_nGamestate == GS_OVERLAY_HOOK || g_nGamestate == GS_LEVEL_LOAD) {
      return;
    }
    TickSpyroGameplayFrame();
    if (g_nGamestate == GS_RESPAWN || g_nGamestate == GS_GAME_OVER ||
        g_nGamestate == GS_OVERLAY_HOOK || g_nGamestate == GS_LEVEL_LOAD) {
      return;
    }

    (*(void (*)())g_pfnLevelAdvanceEmitList)(g_nFrameStep);
    if (!g_nFlightLevelActive) {
      UpdateHudCounters();
    }
    ComputeBillboardBasisVectors();
    UpdateCameraFrame();

    if (g_nScreenFadeLevel != 0) {
      g_nScreenFadeLevel -= g_nFrameStep;
      if (g_nScreenFadeLevel < 0) {
        g_nScreenFadeLevel = 0;
      }
    }

    if (g_nGamestate == GS_GAMEPLAY && g_nGameplayBlocked == 0 &&
        g_nSparxHealth >= 0 &&
        g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
      if ((unsigned int)g_nPadType < 2 || (g_dwPadPressed & PAD_START)) {
        EnterPauseMenu(1);
      } else if (g_dwPadPressed & PAD_SELECT) {
        EnterInventoryMenu(1);
      }
    }
  }

  TickSpuPerFrame();
}
