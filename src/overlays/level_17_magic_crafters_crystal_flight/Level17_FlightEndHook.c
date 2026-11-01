/* Level17_FlightEndHook (0x8007AE08, level_17_magic_crafters_crystal_flight overlay).
 *
 * Flight-level end hook. Once per attempt (guarded by gamestate != 7): enter
 * gamestate 7, clear the six in-flight progress globals, scale the run score
 * (g_nFlightRunClock) by ten, and if this level's best-score slot
 * (g_anFlightClearTimes[g_nActiveWorldIndex]) is empty and all four objective counters
 * (g_anFlightTargetTally..3C) sum to 0x20, record the score there.
 * Identical body in the five flight-level overlays (level_5/11/17/23/29).
 */

#include "level_globals.h"
#include "level_spawn_actor.h"
#include "gamestate.h"


void Level17_FlightEndHook(void) {
  if (g_nGamestate != GS_OVERLAY_HOOK) {
    int score = g_nFlightRunClock;
    int five;
    int idx;
    int *base;
    int *slot;
    int cur;
    int ten;

    g_nGamestate = GS_OVERLAY_HOOK;
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
      if (g_anFlightTargetTally + g_nFlightTargetTally1 + g_nFlightTargetTally2 + g_nFlightTargetTally3 == 0x20) {
        *slot = ten;
      }
    }
  }
}
