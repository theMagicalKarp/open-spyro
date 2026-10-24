#include "funcs.h"
#include "globals.h"
#include "vecmath.h"

/* Per-frame tick of a gem-pickup burst particle entity (0x8003c6e4,
   0x178). Follows the burst record hung off actor[0]: while its lifetime
   byte runs and the vertical velocity stays under the actor's, integrates
   position/velocity (gravity -6 clamped at -0x80), fades the RGB bytes,
   and spawns a sparkle every other tick; otherwise emits the pop particle
   and despawns the actor. */
void TickGemPickupBurstEntity(Actor *actor) {
  int burst[3];
  int *rec;
  int t;
  int b;

  rec = (int *)actor->state;
  if (*((unsigned char *)rec + 0x13) != 0 && actor->posZ > rec[3]) {
    actor->posX += rec[0];
    actor->posY += rec[1];
    t = rec[2] - 6;
    rec[2] = t;
    if (t < -0x80) {
      rec[2] = -0x80;
    }
    actor->posZ += rec[2];
    actor->rotX += *((unsigned char *)rec + 0x10);
    actor->rotY += *((unsigned char *)rec + 0x11);
    actor->rotZ += *((unsigned char *)rec + 0x12);
    b = *((unsigned char *)rec + 0x13) - 1;
    *((unsigned char *)rec + 0x13) = b;
    if ((b & 1) == 0) {
      burst[0] = GetRandomU32() & 3;
      burst[1] = GetRandomU32() & 3;
      burst[2] = 0x14;
      ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
          1, 1, &actor->posX, burst);
    }
  } else {
    ((void (*)(int, int, int *, int))g_pfnLevelEmitParticles)(
        3, 0x46, &actor->posX, 0x10);
    DespawnActorRecord(actor);
  }
}
