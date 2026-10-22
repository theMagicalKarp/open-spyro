/* Level5_FlightEndHook (0x8007AE08, level_5_artisans_sunny_flight overlay).
 *
 * Flight-level end hook. Once per attempt (guarded by gamestate != 7): enter
 * gamestate 7, clear the six in-flight progress globals, scale the run score
 * (D_800758F4) by ten, and if this level's best-score slot
 * (g_anWorldPersistentFlags[D_8007595C]) is empty and all four objective counters
 * (D_80078630..3C) sum to 0x20, record the score there.
 * Identical body in the five flight-level overlays (level_5/11/17/23/29).
 */

extern int g_nGamestate;
extern int D_800758F4;
extern int D_8007595C;
extern int g_nPauseMenuCursor;
extern int g_nPauseMenuIdleFrames;
extern int g_nPauseMenuTransitionFrames;
extern int g_nSubMenuCursor;
extern int g_nSubMenuScrollOffset;
extern int D_8007569C;
extern int g_anWorldPersistentFlags[];
extern int D_80078630;
extern int D_80078634;
extern int D_80078638;
extern int D_8007863C;

void Level5_FlightEndHook(void) {
  if (g_nGamestate != 7) {
    int score = D_800758F4;
    int five;
    int idx;
    int *base;
    int *slot;
    int cur;
    int ten;

    g_nGamestate = 7;
    five = score * 5;
    idx = D_8007595C;
    base = g_anWorldPersistentFlags;
    g_nPauseMenuCursor = 0;
    g_nPauseMenuIdleFrames = 0;
    g_nPauseMenuTransitionFrames = 0;
    g_nSubMenuCursor = 0;
    g_nSubMenuScrollOffset = 0;
    D_8007569C = 0;
    slot = (int *)((idx << 2) + (int)base);
    cur = *slot;
    ten = five << 1;
    D_800758F4 = ten;
    if (cur == 0) {
      if (D_80078630 + D_80078634 + D_80078638 + D_8007863C == 0x20) {
        *slot = ten;
      }
    }
  }
}
