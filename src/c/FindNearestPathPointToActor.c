#include "actor_type.h"
#include "globals.h"
#include "vecmath.h"

/* Find the path point nearest to `actor`'s position. Returns the
   smallest 2D distance and, when `out_index` is non-NULL, stores the winning
   record's index. */
int FindNearestPathPointToActor(Actor *actor, PathData *path, int *out_index) {
  int best = 0xFFFFFF;
  int best_i;
  int i;
  for (i = 0; i < path->count; i++) {
    int d = ApproxDist2D(&actor->posX, &path->nodes[i].x);
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
