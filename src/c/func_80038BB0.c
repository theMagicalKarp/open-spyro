#include "globals.h"

extern int ApproxDist2D(int *a, int *b);

/* Index of the list point farthest from Spyro: list[0] is the point count and
   each 0x10-byte record starts at offset 8. Returns the winning record's
   index (the running maximum 2D distance starts at 0). */
int func_80038BB0(unsigned char *list) {
  int best = 0;
  int best_i;
  int i;
  for (i = 0; i < list[0]; i++) {
    int d = ApproxDist2D(g_anSpyroWorldPos, (int *)(list + 8 + i * 0x10));
    if (best < d) {
      best = d;
      best_i = i;
    }
  }
  return best_i;
}
