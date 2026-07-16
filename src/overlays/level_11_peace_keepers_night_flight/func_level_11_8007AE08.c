/* func_level_11_8007AE08 (0x8007AE08, level_11_peace_keepers_night_flight overlay).
 *
 * Flight-level end hook. Once per attempt (guarded by gamestate != 7): enter
 * gamestate 7, clear the six in-flight progress globals, scale the run score
 * (D_800758F4) by ten, and if this level's best-score slot
 * (D_80078618[D_8007595C]) is empty and all four objective counters
 * (D_80078630..3C) sum to 0x20, record the score there.
 * Identical body in the five flight-level overlays (level_5/11/17/23/29).
 */

extern int D_800757D8;
extern int D_800758F4;
extern int D_8007595C;
extern int D_80075720;
extern int D_8007568C;
extern int D_800758B8;
extern int D_80075744;
extern int D_800757CC;
extern int D_8007569C;
extern int D_80078618[];
extern int D_80078630;
extern int D_80078634;
extern int D_80078638;
extern int D_8007863C;

void func_level_11_8007AE08(void) {
  if (D_800757D8 != 7) {
    int score = D_800758F4;
    int five;
    int idx;
    int *base;
    int *slot;
    int cur;
    int ten;

    D_800757D8 = 7;
    five = score * 5;
    idx = D_8007595C;
    base = D_80078618;
    D_80075720 = 0;
    D_8007568C = 0;
    D_800758B8 = 0;
    D_80075744 = 0;
    D_800757CC = 0;
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
