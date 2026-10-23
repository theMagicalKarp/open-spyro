#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"

extern void *g_pSharedLoadBuffer; /* asset-directory buffer pointer */
extern int g_CutsceneSoundDef;    /* death-cutscene sample-bank record */

/* The sample-bank stores go through the reloaded global pointer each field
   (the original never proves it points at &g_CutsceneSoundDef); a volatile read
   of the pointer defeats gcc's constant-propagation of the just-assigned
   address. */
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
      g_pDrawBufA = &g_abDrawBufPool;
      g_pDrawBufB = &g_abDrawBufPool;
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
        int count = *(volatile int *)&g_nLevelLoadChunkSize;
        int offset = *(volatile int *)&g_nLevelLoadByteOffset;
        void *dst = *(void *volatile *)&g_pSharedLoadBuffer;
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
        g_pLevelSampleBankEntries = &g_CutsceneSoundDef;
        SAMPLE_RAW(BANK, int, nSampleAddr) = 0x262d0;
        SAMPLE_RAW(BANK, int, nLoopAddr) = -1;
        SAMPLE_RAW(BANK, short, nVolume) = 0x50;
        SAMPLE_RAW(BANK, short, nPitch) = g_nWorldCutsceneSamplePitch;
        SAMPLE_RAW(BANK, short, nPitchSpread) = 0;
        SAMPLE_RAW(BANK, short, nPitchStep) = 0;
        SAMPLE_RAW(BANK, int, nPitchMode) = 0;
        g_nSfxOverrideFlags = 1;
        g_nSfxOverrideVolL = 0x3fff;
        g_nSfxOverrideVolR = 0x3fff;
        PlaySoundEffect(0, 0, 0x10, &g_bGemPickupSfxVoiceMarker);
      } else {
        *(int *)g_pPathTableHead = 0x490;
      }
      g_nTitleSubState = 1;
      {
        int lba = *(volatile int *)&g_nCdBaseLba;
        int offset = *(volatile int *)&g_nTitleSceneSectorOffset;
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
