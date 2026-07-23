#include "globals.h"

extern void SamplePadForDeathReplayFrame(void);
extern void StopAllSoundExceptMask(int mask);
extern void TickSpuPerFrame(void);
extern void ClearImage(short *rect, int r, int g, int b);
extern void DrawSync(int mode);
extern void CdReadSyncSectors(int lba, void *dst, int count, int offset,
                              int marker);
extern void TickWorldBundleLoadStream();
extern void TickCdMusicStream(void);
extern void BeginStoryIntro(void);
extern int PlaySoundEffect(int id, int actor, int vol, void *marker);
extern void StartCdReadAsync(int lba, int dst, int count, int offset,
                             int marker);

extern void *D_800113A0; /* asset-directory buffer pointer */
extern int D_8007A6E4;   /* level-load chunk size */
extern int D_8007A6E0;   /* level-load byte offset */
extern int D_8007A6D8;   /* async-read byte offset */
extern int D_8007DDE8;   /* draw-buffer sentinel */
extern int D_800778F0;   /* death-cutscene sample-bank record */
extern int D_800761DC;
extern short D_800761E8;
extern short D_800761EA;
extern int D_80078D88;

/* The sample-bank stores go through the reloaded global pointer each field
   (the original never proves it points at &D_800778F0); a volatile read of the
   pointer defeats gcc's constant-propagation of the just-assigned address. */
#define BANK (*(char *volatile *)&g_pLevelSampleBankEntries)

/* Death-plane watcher (GS state-0 path, g_nDeathState != 0). Ramps the fade
   frame each tick; at 16 fades to black, tears down the level, sync-loads the
   death-cutscene bundle, sets g_Gamestate=0xE (gem cutscene) and advances the
   death-try index. Returns 1 when the death sequence fires, else 0.
   (0x800334d4, 904 bytes.) */
int CheckDeathPlane(void) {
  short rect[8];

  SamplePadForDeathReplayFrame();
  if (g_nDeathState != 2) {
    if (g_nDeathFadeFrame == 0) {
      if (g_anDeathReplayFrameThreshold[g_nDeathTryIndex] - 8 <=
              (int)g_dwGamestateFrames ||
          ((int)g_dwGamestateFrames > 0xf && g_abPadRawReport[0] == 0 &&
           (g_abPadRawReport[2] != 0xff || g_abPadRawReport[3] != 0xff))) {
        g_nDeathFadeFrame = 1;
      }
    } else {
      g_nDeathFadeFrame = g_nDeathFadeFrame + 1;
      g_nGenericCountdown = g_nDeathFadeFrame * 2;
    }
    if (g_nDeathFadeFrame == 0x10) {
      StopAllSoundExceptMask(0);
      TickSpuPerFrame();
      rect[2] = 0x200;
      rect[0] = 0;
      rect[1] = 0;
      rect[3] = 0x1e0;
      ClearImage(rect, 0, 0, 0);
      DrawSync(0);
      g_pDrawBufA = &D_8007DDE8;
      g_pDrawBufB = &D_8007DDE8;
      g_nHitRumbleTimer = 0;
      g_nVibrationLevel = 0;
      g_nPulseRumbleTimer = 0;
      g_nPulseRumbleAmount = 0;
      g_nDeathState = 0;
      g_nCdStreamState = 0;
      g_nCurrentWorldId = 0;
      g_nFlightLevelActive = 0;
      g_nDeathRespawnPending = 0;
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
      g_nGamestate = 0xe;
      g_nGenericCountdown = 0xf;
      BeginStoryIntro();
      if ((int)g_dwGamestateFrames >=
          g_anDeathReplayFrameThreshold[g_nDeathTryIndex]) {
        *(int *)g_pPathTableHead = 0x2d0;
        g_pLevelSampleBankEntries = &D_800778F0;
        *(int *)(BANK + 0x0) = 0x262d0;
        *(int *)(BANK + 0x4) = -1;
        *(short *)(BANK + 0x8) = 0x50;
        *(short *)(BANK + 0xa) = g_nWorldCutsceneSamplePitch;
        *(short *)(BANK + 0xc) = 0;
        *(short *)(BANK + 0xe) = 0;
        *(int *)(BANK + 0x10) = 0;
        D_800761DC = 1;
        D_800761E8 = 0x3fff;
        D_800761EA = 0x3fff;
        PlaySoundEffect(0, 0, 0x10, &g_bGemPickupSfxVoiceMarker);
      } else {
        *(int *)g_pPathTableHead = 0x490;
      }
      D_80078D88 = 1;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int offset = *(volatile int *)&D_8007A6D8;
        int dst =
            -0x40000 + (int)*(void *volatile *)&g_pRenderScratchRegionBase;
        StartCdReadAsync(lba, dst, 0x40000, offset, 600);
      }
      g_nGamePaused = 1;
      g_nDeathTryIndex = (g_nDeathTryIndex + 1) & 3;
      return 1;
    }
  }
  return 0;
}
