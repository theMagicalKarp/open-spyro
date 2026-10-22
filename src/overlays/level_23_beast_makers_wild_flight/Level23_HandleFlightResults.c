/* Level23_HandleFlightResults (0x8007AEDC, level_23_beast_makers_wild_flight overlay).
 *
 * Flight-level end-of-run results handler. When `reload` is set, re-uploads
 * the 0x100x0xE1 VRAM strip at (0x200,0) from the level image base
 * (g_pRenderScratchPrimTop - 0x1C200, LoadImage) and kicks DrawSync(0). Resets
 * the gamestate (g_nGamestate = 0), reseeds the light vector (InitLightVectorConstants),
 * then banks rewards: for each of the four collectible classes whose tally
 * (g_anFlightTargetTally..3C) hit 8, an unclaimed per-course flag
 * (g_abWorldPerLevelByteTable[course*5 + i]) pays the class bonus from the course reward
 * table (D_8006E920[course*5 + i]) into the level gem total
 * (g_anLevelGemsCollected[g_nLevelIntroIndex]) and running score (g_nWorldTreasureCollected), 12 digits
 * (g_nGemTallyIconCount) per payout. A perfect run (tallies sum to 0x20) pays the
 * fifth "all clear" bonus (D_8006E930[course*5], flag D_80078684) and
 * records a best time (g_anWorldPersistentFlags[course], lower is better, 0 = unset)
 * from the run clock g_nFlightRunClock. Finally spins the reward digit ring
 * (g_abGemTallyIconUvTable, 32 slots, glyphs from D_8006E998[i % 12]) and, if the
 * pending-scene word g_nCameraCurrentMode is 0x8000000E, latches the scene byte
 * g_abSpyroStateCameraMode[g_nSpyroState] into g_nCameraNextMode.
 * Identical body in the five flight-level overlays.
 */

extern void LoadImage();
extern void DrawSync();
extern void InitLightVectorConstants(void);

extern char *g_pRenderScratchPrimTop;
extern int g_nGamestate;
extern int g_nActiveWorldIndex;
extern int g_nLevelIntroIndex;
extern int g_nGemTallyIconCount;
extern int g_nWorldTreasureCollected;
extern int g_nFlightRunClock;
extern int g_nCameraCurrentMode;
extern int g_nCameraNextMode;
extern int g_anLevelGemsCollected[];
extern unsigned char g_abGemTallyIconUvTable[];
extern int g_anWorldPersistentFlags[];
extern int g_anFlightTargetTally;
extern int g_nFlightTargetTally1;
extern int g_nFlightTargetTally2;
extern int g_nFlightTargetTally3;
extern unsigned char g_abWorldPerLevelByteTable[];
extern unsigned char D_80078684[];
extern int g_nSpyroState;
extern int g_nSpyroStateFlags;
extern int D_8006E920[];
extern int D_8006E930[];
extern unsigned char D_8006E998[];
extern unsigned char g_abSpyroStateCameraMode[];

void Level23_HandleFlightResults(int reload) {
  int i;

  if (reload != 0) {
    struct {
      short x, y, w, h;
    } rect;
    int pad[2];
    rect.x = 0x200;
    rect.w = 0x100;
    rect.y = 0;
    rect.h = 0xE1;
    LoadImage(&rect, g_pRenderScratchPrimTop - 0x1C200);
    DrawSync(0);
  }
  g_nGamestate = 0;
  InitLightVectorConstants();
  for (i = 0; i < 4; i++) {
    if ((&g_anFlightTargetTally)[i] == 8) {
      int idx = g_nActiveWorldIndex * 5;
      int fbase = idx + (int)g_abWorldPerLevelByteTable;
      unsigned char *flag = (unsigned char *)fbase + i;
      if (*flag == 0) {
        int row;
        g_anLevelGemsCollected[g_nLevelIntroIndex] +=
            *(int *)((i << 2) + (row = (int)&D_8006E920[idx]));
        g_nWorldTreasureCollected += *(int *)((i << 2) + row);
        g_nGemTallyIconCount += 0xC;
        *flag = 1;
      }
    }
  }
  if (g_anFlightTargetTally + g_nFlightTargetTally1 + g_nFlightTargetTally2 + g_nFlightTargetTally3 == 0x20) {
    int idx = g_nActiveWorldIndex * 5;
    if (D_80078684[idx] == 0) {
      int off;
      g_anLevelGemsCollected[g_nLevelIntroIndex] +=
          *(int *)((char *)D_8006E930 + (off = idx << 2));
      g_nWorldTreasureCollected += *(int *)((char *)D_8006E930 + off);
      g_nGemTallyIconCount += 0xC;
      D_80078684[idx] = 1;
    }
    {
      int *best = &g_anWorldPersistentFlags[g_nActiveWorldIndex];
      if (*best == 0 || g_nFlightRunClock < *best) {
        *best = g_nFlightRunClock;
      }
    }
  }
  i = 0;
  if (g_nGemTallyIconCount > 0) {
    do {
      g_abGemTallyIconUvTable[i & 0x1F] = D_8006E998[i % 12];
      i += 1;
    } while (i < g_nGemTallyIconCount);
  }
  g_nSpyroStateFlags = 0;
  if (g_nCameraCurrentMode == 0x8000000E) {
    g_nCameraNextMode = g_abSpyroStateCameraMode[g_nSpyroState];
  }
}
