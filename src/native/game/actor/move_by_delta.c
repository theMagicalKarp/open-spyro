/* Native entry point for MoveActorByDelta (src/main/actor/move_by_delta.c).
 *
 * The definition takes its delta as a by-value struct, which the PS1 passes in
 * $a1-$a3; every caller therefore passes three ints (funcs.h, actor.h). wasm
 * passes a by-value struct as a pointer instead, so natively the definition is
 * renamed MoveActorByDelta_ByValue and this is what the callers reach. */

struct Actor;

typedef struct {
  int x;
  int y;
  int z;
} DeltaVec;

int MoveActorByDelta_ByValue(struct Actor *actor, DeltaVec delta,
                             int contactRadius, int radius, int flags);

int MoveActorByDelta(struct Actor *actor, int vx, int vy, int vz,
                     int contactRadius, int radius, int flags) {
  DeltaVec delta;

  delta.x = vx;
  delta.y = vy;
  delta.z = vz;
  return MoveActorByDelta_ByValue(actor, delta, contactRadius, radius, flags);
}
