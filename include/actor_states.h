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

#endif /* OPEN_SPYRO_ACTOR_STATES_H */
