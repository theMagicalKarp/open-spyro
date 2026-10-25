/* actor_states.h — class-specific state blocks (Actor->state) that main and
 * the level overlays both read. They stand alone, without the Actor type, so
 * overlay headers that only forward-declare struct Actor can include this. */

#ifndef OPEN_SPYRO_ACTOR_STATES_H
#define OPEN_SPYRO_ACTOR_STATES_H

/* Dragon-statue burst shard (type 0xFB), ticked by TickGemPickupBurstEntity:
 * integrates vel with gravity while life runs and posZ stays above floorZ. */
typedef struct BurstShardState {
  int vel[3];          /* 0x00 */
  int floorZ;          /* 0x0C the statue's height */
  unsigned char spinX; /* 0x10 per-tick ->rotX/rotY/rotZ deltas */
  unsigned char spinY;
  unsigned char spinZ;
  unsigned char life;  /* 0x13 ticks left */
} BurstShardState;

/* Sparx (type 0x78, case 120 in the UpdateActors clones; g_pSparxActor):
 * hovers at an offset beside Spyro and chases one target at a time, a
 * butterfly to eat (g_nSparxHealth + 1) or a pickup to fetch. SpawnActor
 * zeroes it all. */
typedef struct SparxState {
  int timer;             /* 0x00 hover re-aim / chase countdown */
  short drift[3];        /* 0x04 hover offset from Spyro, x/y/z */
  char pad0A[0x2];
  struct FxNode *glow;   /* 0x0C sparkle node, shown at full health */
  struct Actor *target;  /* 0x10 butterfly or pickup being chased */
} SparxState;

#endif /* OPEN_SPYRO_ACTOR_STATES_H */
