#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

extern unsigned char g_abDrawBufPool[]; /* draw buffer pool */
extern int
    g_nCdAudioVolume_blk[]; /* [0] music volume, -0x31c bytes SpuCommonAttr */

#define SFX(off)                                                               \
  PlaySoundEffect(((unsigned char *)g_pLevelSampleBankHeader)[off], 0, 0x10, 0)

/* 0x8002e12c (0xa00): GS_PAUSE_MENU tick. Substate 0 is the main pause menu
   (resume / options / inventory / quit), 1 the options page (SFX and music
   volume, mono/stereo, vibration, camera mode) and 2 the quit confirmation,
   which on "yes" tears the level down and reloads the title world. */
void PauseMenu_Update(void) {
  RECT r;
  unsigned int pad;
  int c;
  int last;

  if (!g_nFlightLevelActive) {
    UpdateHudCounters();
  }
  g_nHudLevelGemHoldTimer = 0;
  g_nHudDragonHoldTimer = 0;
  g_nHudLivesHoldTimer = 0;
  g_nHudWorldEggHoldTimer = 0;
  g_nHudLevelEggHoldTimer = 0;
  g_nPauseMenuIdleFrames++;
  RotateLightVectorXZ(3);
  if (g_nPauseMenuTransitionFrames < 5) {
    return;
  }

  pad = g_dwPadPressed;
  if (pad & 0x100) {
    EnterInventoryMenu(0);
    return;
  }

  if (g_nPauseMenuSubstate == 2) {
    if (pad & 0xA000) {
      SFX(0x2E);
      g_nPauseMenuCursor = 1 - g_nPauseMenuCursor;
      return;
    }
    if (!(pad & 0x50)) {
      return;
    }
    if (g_nPauseMenuCursor == 1 || (pad & 0x10)) {
      SFX(0x2D);
      g_nPauseMenuSubstate = 0;
      g_nPauseMenuCursor = 3;
      return;
    }
    StopAllSoundExceptMask(0);
    SFX(0x2D);
    TickSpuPerFrame();
    r.x = 0;
    r.y = 0;
    r.w = 0x200;
    r.h = 0x1E0;
    ClearImage(&r, 0, 0, 0);
    DrawSync(0);
    g_pDrawBufA = g_abDrawBufPool;
    g_pDrawBufB = g_abDrawBufPool;
    g_nCdStreamState = 0;
    g_nCurrentWorldId = 0;
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int count = *(volatile int *)&g_nLevelLoadChunkSize;
      int offset = *(volatile int *)&g_nLevelLoadByteOffset;
      void *dst = *(void *volatile *)&g_pSharedLoadBuffer;

      CdReadSyncSectors(lba, dst, count, offset, 600);
    }
    while (g_nCdStreamState < 10) {
      TickWorldBundleLoadStream();
      TickCdMusicStream();
    }
    BeginGemCutscene();
    BeginStoryIntro();
    g_nGamePaused = 1;
    return;
  }

  if (g_nPauseMenuSubstate == 1) {
    if (pad & 0x4000) {
      SFX(0x2D);
      c = g_nPauseMenuCursor;
      g_nPauseMenuIdleFrames = 0;
      g_nPauseMenuCursor = c + 1;
      if (c + 1 >= 6) {
        g_nPauseMenuCursor = 0;
      } else if (c + 1 == 3 && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c + 2;
      }
    } else if (pad & 0x1000) {
      SFX(0x2D);
      c = g_nPauseMenuCursor;
      g_nPauseMenuIdleFrames = 0;
      g_nPauseMenuCursor = c - 1;
      if (c - 1 < 0) {
        g_nPauseMenuCursor = 5;
      } else if (c - 1 == 3 && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c - 2;
      }
    }
    if (g_dwPadPressed & 0x10) {
      goto back;
    }
    switch (g_nPauseMenuCursor) {
    case 0:
      if ((g_dwPadPressed & 0x8000) && g_nSfxVolume > 0) {
        SFX(0x2E);
        g_nSfxVolume--;
      } else if ((g_dwPadPressed & 0x2000) && g_nSfxVolume < 10) {
        SFX(0x2E);
        g_nSfxVolume++;
      }
      g_nSfxMixVolume_blk[0] = g_nSfxVolume * 0x3FFF / 10;
      g_nSoundMasterVolume = (g_nSfxMixVolume_blk[0] << 12) / 0x3FFF;
      return;
    case 1:
      if ((g_dwPadPressed & 0x8000) && g_nMusicVolume > 0) {
        SFX(0x2E);
        g_nMusicVolume--;
      } else if ((g_dwPadPressed & 0x2000) && g_nMusicVolume < 10) {
        SFX(0x2E);
        g_nMusicVolume++;
      }
      {
        int *p = g_nCdAudioVolume_blk;
        int *common = (int *)((char *)p - 0x31C);
        int vol;

        *common = 0xC0;
        vol = g_nMusicVolume << 11;
        *p = vol;
        g_nCdDaVolumeCurrent = vol;
        g_nSpuCommonCdVolumeR = vol;
        SetSpuCommonAttr(common);
      }
      return;
    case 2:
      if (g_dwPadPressed & 0xA040) {
        SFX(0x2E);
        if ((*g_anSoundMonoMixFlagBlock = 1 - *g_anSoundMonoMixFlagBlock) !=
            0) {
          g_bCdMixRightToLeft = 0x3F;
          g_bCdMixRightToRight = 0x3F;
          g_bCdMixLeftToRight = 0x3F;
          g_bCdMixLeftToLeft = 0x3F;
        } else {
          g_bCdMixRightToRight = 0x7F;
          g_bCdMixLeftToLeft = 0x7F;
          g_bCdMixRightToLeft = 0;
          g_bCdMixLeftToRight = 0;
        }
        CdMix(&g_bCdMixLeftToLeft);
      }
      return;
    case 3:
      if (g_nPadIsDualshockFlag) {
        if (g_dwPadPressed & 0xA040) {
          SFX(0x2E);
          g_nOptionVibrationEnabled = 1 - g_nOptionVibrationEnabled;
          if (g_nOptionVibrationEnabled && g_nVibrationLevel < 10) {
            g_nVibrationLevel = 10;
          }
        }
      } else {
        g_nPauseMenuCursor = 4;
      }
      return;
    case 4:
      if (g_dwPadPressed & 0xA040) {
        SFX(0x2E);
        if (g_dwActiveCameraOptions == 2) {
          g_dwActiveCameraOptions = 0x52;
        } else {
          g_dwActiveCameraOptions = 2;
        }
      }
      return;
    case 5:
      if (g_dwPadPressed & 0x40) {
      back:
        SFX(0x2E);
        g_nPauseMenuSubstate = 0;
        g_nPauseMenuCursor = 0;
      }
      return;
    }
    return;
  }

  last = 3;
  if (pad & 0x4000) {
    SFX(0x2D);
    g_nPauseMenuIdleFrames = 0;
    if (last < ++g_nPauseMenuCursor) {
      g_nPauseMenuCursor = 0;
    }
  } else if (pad & 0x1000) {
    SFX(0x2D);
    g_nPauseMenuIdleFrames = 0;
    if (--g_nPauseMenuCursor < 0) {
      g_nPauseMenuCursor = last;
    }
  }
  if (!(g_dwPadPressed & 0x840)) {
    return;
  }
  switch (g_nPauseMenuCursor) {
  case 0:
    SFX(0x2E);
    ExitPauseMenuToGame(1);
    return;
  case 1:
    SFX(0x2E);
    g_nPauseMenuSubstate = 1;
    g_nPauseMenuCursor = 0;
    return;
  case 2:
    EnterInventoryMenu(0);
    return;
  case 3:
    SFX(0x2E);
    if (g_nActiveLevelId % 10 == 0) {
      g_nPauseMenuCursor = 1;
      g_nPauseMenuSubstate = 2;
      return;
    }
    ZeroVector(&g_nSpyroSwingEulerPitchTarget);
    g_nSpyroScriptedMoveMode = 0;
    if (g_nFlightLevelActive) {
      g_nGamestate = 7;
      g_nPauseMenuCursor = 0;
      g_nPauseMenuIdleFrames = 0;
      g_nFlightQuitFlag = 1;
      ((void (*)(void))g_pfnLevelUpdateActors)();
    } else {
      BeginPauseToMenuTransition();
    }
    return;
  }
}
