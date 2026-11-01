/* level_spawn_actor.h — declarations shared by the *_SpawnActor.c clones, declared once.
 *
 * Each item is spelled identically in every file that includes this header;
 * files that declare any of these names differently keep their own
 * declarations and do not include it. */

#ifndef OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H
#define OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H

#include "actor_states.h"
#include "dragon_rescue.h"

/* The level actor pool, by struct tag so files that carry their own Actor
 * view (A400) can include this header too. */
struct Actor;
extern struct Actor *g_pActorListBase;

/* Actor-record lifecycle (main, actor.h spells them with the typedef). */
extern struct Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(struct Actor *actor);
extern void InsertActorIntoSpatialGrid(struct Actor *actor);
extern void HideActorRenderRecord(struct Actor *actor);
extern void EncodeCachedVecToActorDirCode(struct Actor *actor);
extern void DespawnActorRecord(struct Actor *actor);

/* Collectible drop (types 0xE, 0xF, 0x53..0x57: life statue, life orb,
 * gems) — SpawnActor's view of UpdateActors' PickupState, with 0x0C split
 * into two bytes. */
typedef struct PickupDropState {
  int velX;              /* 0x00 pop-out velocity */
  int velY;              /* 0x04 */
  int velZ;              /* 0x08 */
  unsigned char pad0C;   /* 0x0C (PickupState: groundTri, a short) */
  unsigned char pad0D;
  unsigned char sub;     /* 0x0E resting sub-state */
  unsigned char timer;   /* 0x0F sparkle timer */
  unsigned char bounces; /* 0x10 bounces left */
  unsigned char pitch;   /* 0x11 ground-normal tilt */
  unsigned char yaw;     /* 0x12 */
  unsigned char roll;    /* 0x13 */
  unsigned char sparkle; /* 0x14 g_aRotBillboards handle, 0xFF = none */
} PickupDropState;

/* Butterfly (type 0x10) — SpawnActor's view of UpdateActors' ChaseState. */
typedef struct ButterflyState {
  int dir;                    /* 0x00 */
  int anchor[3];              /* 0x04 flutter anchor */
  unsigned char vz;           /* 0x10 vertical step */
  unsigned char heading;      /* 0x11 */
  unsigned char headingTimer; /* 0x12 */
  unsigned char climbTimer;   /* 0x13 vertical-change timer */
  short idleTimer;
} ButterflyState;

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

/* Thrown-shard state block (types 0x120..0x121, 0x1F6) — UpdateActors'
 * BounceState. */
typedef struct ShardState {
  short vel[3];       /* 0x00 */
  short spinX;        /* 0x06 */
  short spinY;        /* 0x08 */
  short spinZ;        /* 0x0A */
  unsigned char life; /* 0x0C */
  unsigned char slow; /* 0x0D frames until the push dies off */
  short sway;         /* 0x0E */
  int mode;           /* 0x10 */
} ShardState;

extern int FindGroundHeightBelow(int *pos, int drop);

extern void (*g_pfnLevelEmitParticles)(int count, int type, int *pos, int effectArg);

extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */

extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */

extern int g_anFlightClearTimes[];   /* per flight level first-clear time */

extern int RandomIntInRange(int lo, int hi);       /* RandomInRange */
extern int RandomSignedInRange(int lo, int hi);
extern int StepAndLerpAngle8(int ang, int facing, int maxStep, int frac);

/* Declared identically (by type) in two or more including files; hoisted here. */
extern int g_anFlightTargetTally;
extern char *g_apSpritePrimQueue[];
extern int g_dwHudBobSinePhase;
extern char g_pOtDepthBinHead0[];
extern char *g_pRenderScratchRegionBase;
extern struct Actor *g_pSpriteRecordWriteCursor; /* sprite-record queue tail (grows down) */

#endif /* OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H */
