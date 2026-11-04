#include "actor_type.h"
#include "globals.h"
#include "vecmath.h"

/* Index of the list point farthest from Spyro: list[0] is the point count and
   each 0x10-byte record starts at offset 8. Returns the winning record's
   index (the running maximum 2D distance starts at 0). */
int FindFarthestPathPointFromSpyro(PathData *path) {
  int best = 0;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(g_anSpyroWorldPos, &path->nodes[i].x);
    if (best < d) {
      best = d;
      best_i = i;
    }
  }
  return best_i;
}
