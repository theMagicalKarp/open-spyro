/* Level17_FlightResultsMenu_Update (0x8007B1FC, level_17_magic_crafters_crystal_flight overlay).
 *
 * Flight-level end-of-run menu tick: silences all voices once
 * (StopAllSoundExceptMask = StopAllSoundExceptMask when the idle counter is fresh),
 * scrolls the HUD bob phase (g_dwHudBobSinePhase) and sun vector (RotateLightVectorXZ),
 * then runs the retry/exit menu. g_nSubMenuScrollOffset paces g_nSubMenuCursor (menu reveal
 * timer, +1 every 5 ticks, capped 9); four HUD sprite records at
 * D_80078032 (+0x58 stride) spin their +0x32 angle byte. Once revealed
 * (timer >= 9): select/start (pad & 0x5000) toggles the cursor
 * (g_nPauseMenuCursor) with a blip; confirm (pad & 0x840) either dispatches the
 * cursor action (0 = restart flight via g_pfnLevelHandleFlightResults(1) +
 * LoadAndStartLevelFromCd/LoadAndStartLevelFromCd, 1 = exit via g_pfnLevelHandleFlightResults(0) +
 * BeginPauseToMenuTransition/BeginPauseToMenuTransition) or, when the course was
 * completed (collectible tallies g_anFlightTargetTally..3C >= 0x20 and the world
 * flag g_anWorldPersistentFlags[g_nActiveWorldIndex] set) locks the timer at 0x64. Before
 * reveal, confirm fast-forwards the timer to 9.
 * Identical body in the five flight-level overlays.
 */

#include "level_globals.h"

extern void StopAllSoundExceptMask(int mask);
extern void RotateLightVectorXZ(int step);
extern void PlaySoundEffect();
extern void LoadAndStartLevelFromCd(void);
extern void BeginPauseToMenuTransition(void);
extern void (*g_pfnLevelHandleFlightResults)();

extern unsigned char *g_pLevelSampleBankHeader;
extern unsigned int g_dwPadPressed[];
extern unsigned int g_dwHudBobSinePhase[];
extern int g_anWorldPersistentFlags[];
extern int g_anFlightTargetTally;

void Level17_FlightResultsMenu_Update(void) {
  unsigned int *bob;
  unsigned char *rec;
  int i;

  if (g_nPauseMenuIdleFrames == 0) {
    StopAllSoundExceptMask(0);
  }
  bob = g_dwHudBobSinePhase;
  *bob = (*bob - 4) & 0xFF;
  RotateLightVectorXZ(3);
  g_nPauseMenuIdleFrames += 1;
  g_nSubMenuScrollOffset += 1;
  if (g_nSubMenuScrollOffset >= 5) {
    if (g_nSubMenuCursor < 9) {
      g_nSubMenuCursor += 1;
    }
    g_nSubMenuScrollOffset = 0;
  }
  i = 0;
  rec = D_80078032;
  do {
    *rec += 8;
    i += 1;
    rec += 0x58;
  } while (i < 4);
  if (g_nSubMenuCursor >= 9) {
    unsigned int *pad = g_dwPadPressed;
    if (pad[0] & 0x5000) {
      g_nPauseMenuIdleFrames = 0;
      g_nPauseMenuCursor = 1 - g_nPauseMenuCursor;
      PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
    }
    if (pad[0] & 0x840) {
      if (g_nSubMenuCursor >= 0x64 ||
          (g_anFlightTargetTally + g_nFlightTargetTally1 + g_nFlightTargetTally2 + g_nFlightTargetTally3 < 0x20 &&
           g_anWorldPersistentFlags[g_nActiveWorldIndex] == 0)) {
        switch (g_nPauseMenuCursor) {
        case 0:
          g_pfnLevelHandleFlightResults(1);
          LoadAndStartLevelFromCd();
          break;
        case 1:
          g_pfnLevelHandleFlightResults(0);
          BeginPauseToMenuTransition();
          break;
        }
        PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
      } else {
        g_nSubMenuCursor = 0x64;
        PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
      }
    }
  } else if (g_dwPadPressed[0] & 0x840) {
    g_nSubMenuCursor = 9;
    g_nSubMenuScrollOffset = 0;
    PlaySoundEffect(g_pLevelSampleBankHeader[0x2D], 0, 0x10, 0);
  }
}
