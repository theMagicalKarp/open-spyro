#include "actor_states.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* Per-frame tick of a dragon-rescue statue burst shard (0x8003c6e4, 0x178).
   Follows the BurstShardState at ->state: while its life runs and the shard
   is above floorZ, integrates position / velocity (gravity -6 clamped at
   -0x80), spins rotX / rotY / rotZ by the spin bytes and spawns a sparkle
   every other tick; otherwise emits the streak burst and despawns. */
void TickStatueBurstShard(Actor *actor) {
  int burst[3];
  BurstShardState *rec;
  int t;
  int b;

  rec = actor->state;
  if (rec->life != 0 && actor->posZ > rec->floorZ) {
    actor->posX += rec->vel[0];
    actor->posY += rec->vel[1];
    t = rec->vel[2] - 6;
    rec->vel[2] = t;
    if (t < -0x80) {
      rec->vel[2] = -0x80;
    }
    actor->posZ += rec->vel[2];
    actor->rotX += rec->spinX;
    actor->rotY += rec->spinY;
    actor->rotZ += rec->spinZ;
    b = rec->life - 1;
    rec->life = b;
    if ((b & 1) == 0) {
      burst[0] = GetRandomU32() & 3;
      burst[1] = GetRandomU32() & 3;
      burst[2] = 0x14;
      g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)burst);
    }
  } else {
    g_pfnLevelEmitParticles(3, EMIT_STREAK_BURST, &actor->posX, 0x10);
    DespawnActorRecord(actor);
  }
}
