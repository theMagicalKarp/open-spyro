#include "globals.h"

extern void UpdateHudCounters(void);
extern void RotateLightVectorXZ(int step);
extern int PlaySoundEffect(unsigned int id, void *owner, unsigned int mode,
                           void *pos);
extern void StopAllSoundExceptMask(int mask);
extern void TickSpuPerFrame(void);
extern int ClearImage(RECT *rect, int r, int g, int b);
extern int DrawSync(int mode);
extern void CdReadSyncSectors(int lbaBase, void *dst, int size, int offset,
                              int marker);
extern void TickWorldBundleLoadStream(void);
extern void TickCdMusicStream(void);
extern void BeginGemCutscene(void);
extern void BeginStoryIntro(void);
extern void EnterInventoryMenu(int fromMenu);
extern void ExitPauseMenuToGame(int playSfx);
extern void SetSpuCommonAttr(void *attr);
extern int CdMix(void *vol);
extern void ZeroVector(int *v);
extern void BeginPauseToMenuTransition(void);

extern void *D_800113A0;           /* asset-directory buffer pointer */
extern int D_8007A6E0;             /* level-load byte offset */
extern int D_8007A6E4;             /* level-load chunk size */
extern unsigned char D_8007DDE8[]; /* draw buffer pool */
extern int D_80076228;             /* SFX volume (0..0x3fff) */
extern int D_80076224_blk[]; /* [0] music volume, -0x31c bytes SpuCommonAttr */
extern int g_anSoundMonoMixFlagBlock[];
extern short D_80075F18;         /* SpuCommonAttr CD volume L */
extern short D_80075F1A;         /* SpuCommonAttr CD volume R */
extern unsigned char D_800776D0; /* CdMix attenuation L->L */
extern unsigned char D_800776D1; /* L->R */
extern unsigned char D_800776D2; /* R->R */
extern unsigned char D_800776D3; /* R->L */
extern int D_80075900;           /* flight-level quit flag */

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
    g_pDrawBufA = D_8007DDE8;
    g_pDrawBufB = D_8007DDE8;
    g_nCdStreamState = 0;
    g_nCurrentWorldId = 0;
    {
      int lba = *(volatile int *)&g_nCdBaseLba;
      int count = *(volatile int *)&D_8007A6E4;
      int offset = *(volatile int *)&D_8007A6E0;
      void *dst = *(void *volatile *)&D_800113A0;

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
      D_80076228 = g_nSfxVolume * 0x3FFF / 10;
      g_nSoundMasterVolume = (D_80076228 << 12) / 0x3FFF;
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
        int *p = D_80076224_blk;
        int *common = (int *)((char *)p - 0x31C);
        int vol;

        *common = 0xC0;
        vol = g_nMusicVolume << 11;
        *p = vol;
        D_80075F18 = vol;
        D_80075F1A = vol;
        SetSpuCommonAttr(common);
      }
      return;
    case 2:
      if (g_dwPadPressed & 0xA040) {
        SFX(0x2E);
        if ((*g_anSoundMonoMixFlagBlock = 1 - *g_anSoundMonoMixFlagBlock) !=
            0) {
          D_800776D3 = 0x3F;
          D_800776D2 = 0x3F;
          D_800776D1 = 0x3F;
          D_800776D0 = 0x3F;
        } else {
          D_800776D2 = 0x7F;
          D_800776D0 = 0x7F;
          D_800776D3 = 0;
          D_800776D1 = 0;
        }
        CdMix(&D_800776D0);
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
      D_80075900 = 1;
      ((void (*)(void))g_pfnGamestate0EarlyHook)();
    } else {
      BeginPauseToMenuTransition();
    }
    return;
  }
}
