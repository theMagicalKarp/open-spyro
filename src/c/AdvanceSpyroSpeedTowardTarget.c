#include "globals.h"

/* held-base view of g_nSpyroSpeedTarget (0x80078b20): [0] is the target,
   [0x14] (+0x50) is g_nSpyroMotionSpeed. */
extern int g_anSpyroSpeedBlock[];

/* 0x8003d92c (76 bytes) — move the motion speed one substep toward the target:
   accelerate by `accel` while under it, decelerate by `decel` while over it,
   and clamp to the target on the step that crosses it.

   The clamp store is written per arm so every access stays `off(a3)` off the
   held base (A201; jump.c cross-jumps the two tails back together), and the
   A195 depth-2 wrapper lifts `stepped` over `speed`/`target`.  The decelerate
   arm uses its own `down` local: global.c expand_preferences merges hard-reg
   preferences across every insn where a register dies, so a shared `stepped`
   would carry accel's $a0 preference over to `speed` through `speed - decel`
   and cost a `move v1,a0`. */
void AdvanceSpyroSpeedTowardTarget(int accel, int decel) {
  int stepped;
  int target;
  int speed;
  int down;

  target = g_anSpyroSpeedBlock[0];
  do {
  } while (0);
  speed = g_anSpyroSpeedBlock[0x14];
  if (speed < target) {
    do {
      do {
        stepped = speed + accel;
      } while (0);
    } while (0);
    g_anSpyroSpeedBlock[0x14] = stepped;
    if (target < stepped) {
      g_anSpyroSpeedBlock[0x14] = target;
    }
  } else {
    down = speed - decel;
    g_anSpyroSpeedBlock[0x14] = down;
    if (down < target) {
      g_anSpyroSpeedBlock[0x14] = target;
    }
  }
}
