/* func_level_5_8007B1FC (0x8007B1FC, level_5_artisans_sunny_flight overlay).
 *
 * Flight-level end-of-run menu tick: silences all voices once
 * (func_80056B28 = StopAllSoundExceptMask when the idle counter is fresh),
 * scrolls the HUD bob phase (D_80077FE4) and sun vector (func_80058CC0),
 * then runs the retry/exit menu. D_800757CC paces D_80075744 (menu reveal
 * timer, +1 every 5 ticks, capped 9); four HUD sprite records at
 * D_80078032 (+0x58 stride) spin their +0x32 angle byte. Once revealed
 * (timer >= 9): select/start (pad & 0x5000) toggles the cursor
 * (D_80075720) with a blip; confirm (pad & 0x840) either dispatches the
 * cursor action (0 = restart flight via D_800757A8(1) +
 * func_800144C8/LoadAndStartLevelFromCd, 1 = exit via D_800757A8(0) +
 * func_8002C618/BeginPauseToMenuTransition) or, when the course was
 * completed (collectible tallies D_80078630..3C >= 0x20 and the world
 * flag D_80078618[D_8007595C] set) locks the timer at 0x64. Before
 * reveal, confirm fast-forwards the timer to 9.
 * Identical body in the five flight-level overlays.
 */

extern void func_80056B28(int mask);
extern void func_80058CC0(int step);
extern void func_80055A78();
extern void func_800144C8(void);
extern void func_8002C618(void);
extern void (*D_800757A8)();

extern int D_8007568C;
extern int D_80075720;
extern int D_80075744;
extern int D_800757CC;
extern int D_8007595C;
extern unsigned char *D_800761D4;
extern unsigned int D_80077378[];
extern unsigned int D_80077FE4[];
extern unsigned char D_80078032[];
extern int D_80078618[];
extern int D_80078630;
extern int D_80078634;
extern int D_80078638;
extern int D_8007863C;

void func_level_5_8007B1FC(void) {
  unsigned int *bob;
  unsigned char *rec;
  int i;

  if (D_8007568C == 0) {
    func_80056B28(0);
  }
  bob = D_80077FE4;
  *bob = (*bob - 4) & 0xFF;
  func_80058CC0(3);
  D_8007568C += 1;
  D_800757CC += 1;
  if (D_800757CC >= 5) {
    if (D_80075744 < 9) {
      D_80075744 += 1;
    }
    D_800757CC = 0;
  }
  i = 0;
  rec = D_80078032;
  do {
    *rec += 8;
    i += 1;
    rec += 0x58;
  } while (i < 4);
  if (D_80075744 >= 9) {
    unsigned int *pad = D_80077378;
    if (pad[0] & 0x5000) {
      D_8007568C = 0;
      D_80075720 = 1 - D_80075720;
      func_80055A78(D_800761D4[0x2D], 0, 0x10, 0);
    }
    if (pad[0] & 0x840) {
      if (D_80075744 >= 0x64 ||
          (D_80078630 + D_80078634 + D_80078638 + D_8007863C < 0x20 &&
           D_80078618[D_8007595C] == 0)) {
        switch (D_80075720) {
        case 0:
          D_800757A8(1);
          func_800144C8();
          break;
        case 1:
          D_800757A8(0);
          func_8002C618();
          break;
        }
        func_80055A78(D_800761D4[0x2D], 0, 0x10, 0);
      } else {
        D_80075744 = 0x64;
        func_80055A78(D_800761D4[0x2D], 0, 0x10, 0);
      }
    }
  } else if (D_80077378[0] & 0x840) {
    D_80075744 = 9;
    D_800757CC = 0;
    func_80055A78(D_800761D4[0x2D], 0, 0x10, 0);
  }
}
