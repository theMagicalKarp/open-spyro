#include "camera.h"
#include "cutscene_id.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "pad_buttons.h"
#include "pause_menu.h"
#include "sample_bank.h"
#include "vecmath.h"

extern unsigned char g_abDrawBufPool[]; /* draw buffer pool */
extern int
    g_nCdAudioVolume_blk[]; /* [0] music volume, -0x31c bytes SpuCommonAttr */

#define SFX(off) PlaySoundEffect(LEVEL_SFX(off), 0, SFX_MODE_PANNED, 0)

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
  g_nHudKeyHoldTimer = 0;
  g_nPauseMenuIdleFrames++;
  RotateLightVectorXZ(3);
  if (g_nPauseMenuTransitionFrames < 5) {
    return;
  }

  pad = g_dwPadPressed;
  if (pad & PAD_SELECT) {
    EnterInventoryMenu(0);
    return;
  }

  if (g_nPauseMenuSubstate == PAUSE_PAGE_QUIT_CONFIRM) {
    if (pad & (PAD_RIGHT | PAD_LEFT)) {
      SFX(SFX_MENU_SELECT);
      g_nPauseMenuCursor = 1 - g_nPauseMenuCursor;
      return;
    }
    if (!(pad & (PAD_TRIANGLE | PAD_CROSS))) {
      return;
    }
    if (g_nPauseMenuCursor == PAUSE_QUIT_NO || (pad & PAD_TRIANGLE)) {
      SFX(SFX_MENU_MOVE);
      g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
      g_nPauseMenuCursor = PAUSE_ROW_EXIT;
      return;
    }
    StopAllSoundExceptMask(0);
    SFX(SFX_MENU_MOVE);
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
    g_nCutsceneId = CUTSCENE_TITLE_BACKDROP;
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int count = *(volatile int *)&g_nLevelLoadChunkSize;
      int offset = *(volatile int *)&g_nLevelLoadByteOffset;
      void *dst = *(void *volatile *)&g_pSharedLoadBuffer;

      CdReadSyncSectors(lba, dst, count, offset, 600);
    }
    while (g_nCdStreamState < 10) {
      TickCutsceneBundleLoadStream();
      TickCdMusicStream();
    }
    BeginCutscene();
    BeginStoryIntro();
    g_nGamePaused = 1;
    return;
  }

  if (g_nPauseMenuSubstate == PAUSE_PAGE_OPTIONS) {
    if (pad & PAD_DOWN) {
      SFX(SFX_MENU_MOVE);
      c = g_nPauseMenuCursor;
      g_nPauseMenuIdleFrames = 0;
      g_nPauseMenuCursor = c + 1;
      if (c + 1 >= OPTIONS_ROW_COUNT) {
        g_nPauseMenuCursor = OPTIONS_ROW_SFX_VOLUME;
      } else if (c + 1 == OPTIONS_ROW_VIBRATION && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c + 2;
      }
    } else if (pad & PAD_UP) {
      SFX(SFX_MENU_MOVE);
      c = g_nPauseMenuCursor;
      g_nPauseMenuIdleFrames = 0;
      g_nPauseMenuCursor = c - 1;
      if (c - 1 < 0) {
        g_nPauseMenuCursor = OPTIONS_ROW_DONE;
      } else if (c - 1 == OPTIONS_ROW_VIBRATION && !g_nPadIsDualshockFlag) {
        g_nPauseMenuCursor = c - 2;
      }
    }
    if (g_dwPadPressed & PAD_TRIANGLE) {
      goto back;
    }
    switch (g_nPauseMenuCursor) {
    case OPTIONS_ROW_SFX_VOLUME:
      if ((g_dwPadPressed & PAD_LEFT) && g_nSfxVolume > 0) {
        SFX(SFX_MENU_SELECT);
        g_nSfxVolume--;
      } else if ((g_dwPadPressed & PAD_RIGHT) && g_nSfxVolume < 10) {
        SFX(SFX_MENU_SELECT);
        g_nSfxVolume++;
      }
      g_nSfxMixVolume_blk[0] = g_nSfxVolume * 0x3FFF / 10;
      g_nSoundMasterVolume = (g_nSfxMixVolume_blk[0] << 12) / 0x3FFF;
      return;
    case OPTIONS_ROW_MUSIC_VOLUME:
      if ((g_dwPadPressed & PAD_LEFT) && g_nMusicVolume > 0) {
        SFX(SFX_MENU_SELECT);
        g_nMusicVolume--;
      } else if ((g_dwPadPressed & PAD_RIGHT) && g_nMusicVolume < 10) {
        SFX(SFX_MENU_SELECT);
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
    case OPTIONS_ROW_SPEAKERS:
      if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
        SFX(SFX_MENU_SELECT);
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
    case OPTIONS_ROW_VIBRATION:
      if (g_nPadIsDualshockFlag) {
        if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
          SFX(SFX_MENU_SELECT);
          g_nOptionVibrationEnabled = 1 - g_nOptionVibrationEnabled;
          if (g_nOptionVibrationEnabled && g_nVibrationLevel < 10) {
            g_nVibrationLevel = 10;
          }
        }
      } else {
        g_nPauseMenuCursor = OPTIONS_ROW_CAMERA;
      }
      return;
    case OPTIONS_ROW_CAMERA:
      if (g_dwPadPressed & (PAD_CROSS | PAD_RIGHT | PAD_LEFT)) {
        SFX(SFX_MENU_SELECT);
        if (g_dwActiveCameraOptions == CAM_OPT_ACTIVE) {
          g_dwActiveCameraOptions = CAM_OPT_PASSIVE;
        } else {
          g_dwActiveCameraOptions = CAM_OPT_ACTIVE;
        }
      }
      return;
    case OPTIONS_ROW_DONE:
      if (g_dwPadPressed & PAD_CROSS) {
      back:
        SFX(SFX_MENU_SELECT);
        g_nPauseMenuSubstate = PAUSE_PAGE_MAIN;
        g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
      }
      return;
    }
    return;
  }

  last = PAUSE_ROW_EXIT;
  if (pad & PAD_DOWN) {
    SFX(SFX_MENU_MOVE);
    g_nPauseMenuIdleFrames = 0;
    if (last < ++g_nPauseMenuCursor) {
      g_nPauseMenuCursor = PAUSE_ROW_CONTINUE;
    }
  } else if (pad & PAD_UP) {
    SFX(SFX_MENU_MOVE);
    g_nPauseMenuIdleFrames = 0;
    if (--g_nPauseMenuCursor < 0) {
      g_nPauseMenuCursor = last;
    }
  }
  if (!(g_dwPadPressed & (PAD_CROSS | PAD_START))) {
    return;
  }
  switch (g_nPauseMenuCursor) {
  case PAUSE_ROW_CONTINUE:
    SFX(SFX_MENU_SELECT);
    ExitPauseMenuToGame(1);
    return;
  case PAUSE_ROW_OPTIONS:
    SFX(SFX_MENU_SELECT);
    g_nPauseMenuSubstate = PAUSE_PAGE_OPTIONS;
    g_nPauseMenuCursor = OPTIONS_ROW_SFX_VOLUME;
    return;
  case PAUSE_ROW_INVENTORY:
    EnterInventoryMenu(0);
    return;
  case PAUSE_ROW_EXIT:
    SFX(SFX_MENU_SELECT);
    if (g_nActiveLevelId % 10 == 0) {
      g_nPauseMenuCursor = PAUSE_QUIT_NO;
      g_nPauseMenuSubstate = PAUSE_PAGE_QUIT_CONFIRM;
      return;
    }
    ZeroVector(&g_nSpyroSwingEulerTargetX);
    g_nSpyroScriptedMoveMode = 0;
    if (g_nFlightLevelActive) {
      g_nGamestate = GS_OVERLAY_HOOK;
      g_nPauseMenuCursor = 0;
      g_nPauseMenuIdleFrames = 0;
      g_nFlightQuitFlag = 1;
      g_pfnLevelUpdateActors();
    } else {
      BeginPauseToMenuTransition();
    }
    return;
  }
}
