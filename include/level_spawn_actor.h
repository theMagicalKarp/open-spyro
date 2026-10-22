/* level_spawn_actor.h — declarations shared by the *_SpawnActor.c clones, declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H
#define OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H

/* Collectible drop (types 0xE, 0xF, 0x53..0x57: life statue, life orb,
 * gems) — SpawnActor's view of UpdateActors' PickupState, with 0x0C split
 * into two bytes. */
typedef struct PickupDropState {
  int velX;              /* 0x00 pop-out velocity */
  int velY;              /* 0x04 */
  int velZ;              /* 0x08 */
  unsigned char unk0C;   /* 0x0C (PickupState: collision-tri index, short) */
  unsigned char unk0D;
  unsigned char sub;     /* 0x0E resting sub-state */
  unsigned char timer;   /* 0x0F sparkle timer */
  unsigned char bounces; /* 0x10 bounces left */
  unsigned char pitch;   /* 0x11 ground-normal tilt */
  unsigned char yaw;     /* 0x12 */
  unsigned char roll;    /* 0x13 */
  unsigned char sparkle; /* 0x14 g_aRotBillboards handle, 0xFF = none */
} PickupDropState;

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

/* Butterfly (type 0x10) — SpawnActor's view of UpdateActors' ChaseState. */
typedef struct ButterflyState {
  int dir;                    /* 0x00 */
  int anchor[3];              /* 0x04 flutter anchor */
  unsigned char vz;           /* 0x10 vertical step */
  unsigned char heading;      /* 0x11 */
  unsigned char headingTimer; /* 0x12 */
  unsigned char climbTimer;   /* 0x13 vertical-change timer */
  short unk14;
} ButterflyState;

/* Sparx (type 0x78) — SpawnActor's view of UpdateActors' FlierState. */
typedef struct SparxState {
  int timer;  /* 0x00 idle / fetch timer */
  short vx;   /* 0x04 hover offset */
  short vy;   /* 0x06 */
  short vz;   /* 0x08 */
  short unk0A;
  int fx;     /* 0x0C glow node (FxNode *) */
  int rider;  /* 0x10 actor being fetched (Actor *) */
} SparxState;

/* Chest / object fragment (types 0x43..0x45, 0xFF..0x101, 0x135..0x137 and
 * per-level ranges) — UpdateActors' SparkState / TumbleState. */
typedef struct FragmentState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  int life;     /* 0x0C ticks left */
  int floorZ;   /* 0x10 retire height */
} FragmentState;

/* Thrown-shard state block (types 0x120..0x121). */
typedef struct ShardState {
  short vel[3]; /* 0x00 */
  short spinX;  /* 0x06 */
  short spinY;  /* 0x08 */
  short spinZ;  /* 0x0A */
  unsigned char unk0C;
  unsigned char unk0D;
  short unk0E;
  int unk10;
} ShardState;

extern int FindGroundHeightBelow(int *pos, int drop);

extern void (*g_pfnLevelEmitParticles)(int kind, int count, int *pos, int spread); /* SpawnPuff */

extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */

extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */

extern int g_anWorldPersistentFlags[];   /* per-slot flight-race object table */

extern int RandomIntInRange(int lo, int hi);       /* RandomInRange */

#endif /* OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H */
