#include "globals.h"

/* Per-frame tick of a gem-pickup burst particle entity (0x8003c6e4,
   0x178). Follows the burst record hung off actor[0]: while its lifetime
   byte runs and the vertical velocity stays under the actor's, integrates
   position/velocity (gravity -6 clamped at -0x80), fades the RGB bytes,
   and spawns a sparkle every other tick; otherwise emits the pop particle
   and despawns the actor. */
extern unsigned int GetRandomU32(void);
extern void DespawnActorRecord(int *actor);

void TickGemPickupBurstEntity(int *actor) {
  int burst[3];
  int *rec;
  int t;
  int b;

  rec = (int *)actor[0];
  if (*((unsigned char *)rec + 0x13) != 0 && actor[5] > rec[3]) {
    actor[3] += rec[0];
    actor[4] += rec[1];
    t = rec[2] - 6;
    rec[2] = t;
    if (t < -0x80) {
      rec[2] = -0x80;
    }
    actor[5] += rec[2];
    *((unsigned char *)actor + 0x44) += *((unsigned char *)rec + 0x10);
    *((unsigned char *)actor + 0x45) += *((unsigned char *)rec + 0x11);
    *((unsigned char *)actor + 0x46) += *((unsigned char *)rec + 0x12);
    b = *((unsigned char *)rec + 0x13) - 1;
    *((unsigned char *)rec + 0x13) = b;
    if ((b & 1) == 0) {
      burst[0] = GetRandomU32() & 3;
      burst[1] = GetRandomU32() & 3;
      burst[2] = 0x14;
      ((void (*)(int, int, int *, int *))g_pfnLevelOverlayParticleSpawn)(
          1, 1, actor + 3, burst);
    }
  } else {
    ((void (*)(int, int, int *, int))g_pfnLevelOverlayParticleSpawn)(
        3, 0x46, actor + 3, 0x10);
    DespawnActorRecord(actor);
  }
}
