#include "funcs.h"
#include "globals.h"

/* 0x8003b9d4 (0xFC) — on picking up actor `a` (a fodder/pickup): record its
   kind byte in the gem-tally icon UV ring, bump the ring count, special-case
   the egg-thief actor (g_pThiefEggDrop -> 3 eggs), notify the pickup handler,
   spawn the level's particle burst for the kind, then queue the matching sound
   effect for kinds 0x53..0x57. */
void CollectPickupActor(Actor *a) {
  short kind;
  int pad[4];

  g_abGemTallyIconUvTable[g_nGemTallyIconCount & 0x1F] = a->type;
  g_nGemTallyIconCount += 1;
  if ((unsigned char *)a == g_pThiefEggDrop) {
    g_nLevelEggCount = 3;
  }
  RefreshActorDerivedState(a, 4);
  ((void (*)(int, int, Actor *, int))g_pfnLevelEmitParticles)(
      6, 0xC, a, g_adwActorBurstColors[a->type]);

  kind = a->type;
  if (kind == ACTOR_TYPE_GEM_1) {
    CommitActorKillGems(1, (unsigned char *)a);
  } else if (kind == ACTOR_TYPE_GEM_2) {
    CommitActorKillGems(2, (unsigned char *)a);
  } else if (kind == ACTOR_TYPE_GEM_5) {
    CommitActorKillGems(5, (unsigned char *)a);
  } else if (kind == ACTOR_TYPE_GEM_10) {
    CommitActorKillGems(0xA, (unsigned char *)a);
  } else if (kind == ACTOR_TYPE_GEM_25) {
    CommitActorKillGems(0x19, (unsigned char *)a);
  }
}
