#include "actor_type.h"
#include "globals.h"
#include "vecmath.h"

/* Find the path point nearest to Spyro: list[0] is the point count and each
   0x10-byte record starts at offset 8. Returns the smallest 2D distance and,
   when `out_index` is non-NULL, stores the winning record's index. */
int FindNearestPathPointToSpyro(PathData *path, int *out_index) {
  int best = 0xFFFFFF;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(g_anSpyroWorldPos, &path->nodes[i].x);
    if (d < best) {
      best = d;
      best_i = i;
    }
  }
  if (out_index != 0) {
    *out_index = best_i;
  }
  return best;
}
