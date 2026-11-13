#include "camera.h"
#include "frame_db.h"
#include "gamestate.h"
#include "level_flight_results.h"
#include "level_funcs.h"
#include "level_globals.h"
#include "level_spawn_actor.h"

/* Level23_FlightEndHook (0x8007AE08, level_23_beast_makers_wild_flight overlay).
 *
 * Flight-level end hook. Once per attempt (guarded by gamestate != 7): enter
 * GS_FLIGHT_RESULTS, clear the six in-flight progress globals, scale the run score
 * (g_nFlightRunClock) by ten, and if this level's best-score slot
 * (g_anFlightClearTimes[g_nActiveWorldIndex]) is empty and all four objective counters
 * (g_anFlightTargetTally..3C) sum to FLIGHT_TARGET_TOTAL, record the score there.
 * Identical body in the five flight-level overlays (level_5/11/17/23/29).
 */


void Level23_FlightEndHook(void) {
  if (g_nGamestate != GS_FLIGHT_RESULTS) {
    int score = g_nFlightRunClock;
    int five;
    int idx;
    int *base;
    int *slot;
    int cur;
    int ten;

    g_nGamestate = GS_FLIGHT_RESULTS;
    five = score * 5;
    idx = g_nActiveWorldIndex;
    base = g_anFlightClearTimes;
    g_nPauseMenuCursor = 0;
    g_nMenuTickFrames = 0;
    g_nMenuDrawFrames = 0;
    g_nSubMenuCursor = 0;
    g_nSubMenuScrollOffset = 0;
    g_nFlightResultsRevealStep = 0;
    slot = (int *)((idx << 2) + (int)base);
    cur = *slot;
    ten = five << 1;
    g_nFlightRunClock = ten;
    if (cur == 0) {
      if (g_anFlightTargetTally + g_nFlightTargetTally1 + g_nFlightTargetTally2 + g_nFlightTargetTally3 == FLIGHT_TARGET_TOTAL) {
        *slot = ten;
      }
    }
  }
}

/* Level23_HandleFlightResults (0x8007AEDC, level_23_beast_makers_wild_flight overlay).
 *
 * Flight-level end-of-run results handler. When `reload` is set, re-uploads
 * the 0x100x0xE1 VRAM strip at (0x200,0) from the level image base
 * (g_pRenderScratchPrimTop - 0x1C200, LoadImage) and kicks DrawSync(0). Resets
 * the gamestate (g_nGamestate = 0), reseeds the light vector (InitLightVectorConstants),
 * then banks rewards: for each of the four collectible classes whose tally
 * (g_anFlightTargetTally..3C) hit 8, an unclaimed per-course flag
 * (g_abFlightBonusPaid[course*5 + i]) pays the class bonus from the course reward
 * table (g_anFlightCourseBonusTable[course*5 + i]) into the level gem total
 * (g_anLevelGemsCollected[g_nLevelIndex]) and running score (g_nTotalTreasureCollected), 12 digits
 * (g_nGemTallyIconCount) per payout. A perfect run (tallies sum to FLIGHT_TARGET_TOTAL) pays the
 * fifth "all clear" bonus (g_anFlightCourseBonusTable_r4[course*5], flag g_abFlightBonusPaid_r4) and
 * records a best time (g_anFlightClearTimes[course], lower is better, 0 = unset)
 * from the run clock g_nFlightRunClock. Finally spins the reward digit ring
 * (g_abGemTallyIconUvTable, 32 slots, glyphs from g_abGemTallyIconGlyphs[i % 12]) and, if the
 * pending-scene word g_nCameraCurrentMode is CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE, latches the scene byte
 * g_abSpyroStateCameraMode[g_nSpyroState] into g_nCameraNextMode.
 * Identical body in the five flight-level overlays.
 */


extern char *g_pRenderScratchPrimTop;

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
    LoadImage(&rect, g_pRenderScratchPrimTop - MENU_SNAPSHOT_BYTES);
    DrawSync(0);
  }
  g_nGamestate = GS_GAMEPLAY;
  InitLightVectorConstants();
  for (i = 0; i < 4; i++) {
    if ((&g_anFlightTargetTally)[i] == FLIGHT_CLASS_TARGETS) {
      int idx = g_nActiveWorldIndex * 5;
      int fbase = idx + (int)g_abFlightBonusPaid;
      unsigned char *flag = (unsigned char *)fbase + i;
      if (*flag == 0) {
        int row;
        g_anLevelGemsCollected[g_nLevelIndex] +=
            *(int *)((i << 2) + (row = (int)&g_anFlightCourseBonusTable[idx]));
        g_nTotalTreasureCollected += *(int *)((i << 2) + row);
        g_nGemTallyIconCount += 0xC;
        *flag = 1;
      }
    }
  }
  if (g_anFlightTargetTally + g_nFlightTargetTally1 + g_nFlightTargetTally2 + g_nFlightTargetTally3 == FLIGHT_TARGET_TOTAL) {
    int idx = g_nActiveWorldIndex * 5;
    if (g_abFlightBonusPaid_r4[idx] == 0) {
      int off;
      g_anLevelGemsCollected[g_nLevelIndex] +=
          COURSE_ALL_CLEAR_BONUS(off = idx << 2);
      g_nTotalTreasureCollected += COURSE_ALL_CLEAR_BONUS(off);
      g_nGemTallyIconCount += 0xC;
      g_abFlightBonusPaid_r4[idx] = 1;
    }
    {
      int *best = &g_anFlightClearTimes[g_nActiveWorldIndex];
      if (*best == 0 || g_nFlightRunClock < *best) {
        *best = g_nFlightRunClock;
      }
    }
  }
  i = 0;
  if (g_nGemTallyIconCount > 0) {
    do {
      g_abGemTallyIconUvTable[i & 0x1F] = g_abGemTallyIconGlyphs[i % 12];
      i += 1;
    } while (i < g_nGemTallyIconCount);
  }
  g_nSpyroSubstate = 0;
  if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_SCRIPTED_VANTAGE)) {
    g_nCameraNextMode = g_abSpyroStateCameraMode[g_nSpyroState];
  }
}
