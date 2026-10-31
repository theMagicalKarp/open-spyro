#include "actor_type.h"
#include "funcs.h"
#include "globals.h"

/* 0x8003a79c: the distance from the actor to its path's current node,
   divided by divisor. */
int MeasureOffsetToPathPoint(Actor *actor, PathData *path, int divisor) {
  int v[3];

  v[0] = path->nodes[path->cur].x - actor->posX;
  v[1] = path->nodes[path->cur].y - actor->posY;
  v[2] = path->nodes[path->cur].z - actor->posZ;

  return (int)VectorLength(v, VEC_LEN_XYZ) / divisor;
}
