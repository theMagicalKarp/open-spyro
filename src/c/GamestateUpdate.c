#include "globals.h"

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
extern void TickActiveSoundVoices(void);
extern void TickCdMusicStream(void);
extern void Gamestate01_Update(void);
extern void PauseMenu_Update(void);
extern void SubMenu_Update(void);
extern void RespawnOrGameOver_Update(void);
extern void Gamestate06_Update(void);
extern void GemPickup_Update(void);
extern void CameraSettle_Update(void);
extern void SpyroStateDelay_Update(void);
extern void Gamestate0B_Update(void);
extern void Gamestate0C_Update(void);
extern void Gamestate0D_Update(void);
extern void GemCutscene_Update(void);
extern void Gamestate0F_Update(void);
extern void func_8007AA50(void); /* credits overlay entry */
extern void func_8007ABAC(void); /* titlescreen overlay entry */
extern int CheckDeathPlane(void);
extern void TickWorldChunkAnimations(int step);
extern void TickSpyroGameplayFrame(void);
extern void UpdateHudCounters(void);
extern void ComputeBillboardBasisVectors(void);
extern void UpdateCameraFrame(void);
extern void EnterPauseMenu(int mode);
extern void EnterInventoryMenu(int mode);
extern void TickSpuPerFrame(void);

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
        func_8007ABAC();
      } else {
        Gamestate0D_Update();
      }
    } else if (g_nGamestate == 0xE) {
      GemCutscene_Update();
    } else if (g_nGamestate == 0xF) {
      if (g_nGamestate0fMode >= 0x63) {
        Gamestate0F_Update();
      } else {
        func_8007AA50();
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
    (*(void (*)())g_pfnGamestate0EarlyHook)();

    if (g_nGamestate == 0xB || g_nGamestate == 0xC || g_nGamestate == 7 ||
        g_nGamestate == 1) {
      return;
    }
    TickSpyroGameplayFrame();
    if (g_nGamestate == 4 || g_nGamestate == 5 || g_nGamestate == 7 ||
        g_nGamestate == 1) {
      return;
    }

    (*(void (*)())g_pfnGamestate0LateHook)(g_nFrameStep);
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
