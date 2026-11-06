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
extern struct Model *g_apActorMeshTable[]; /* per-type model table, indexed by ->type (model.h) */

/* Actor-record lifecycle (main, actor.h spells them with the typedef). */
extern struct Actor *AllocActorRecordSlot(void);
extern void InitActorRenderDefaults(struct Actor *actor);
extern void InsertActorIntoSpatialGrid(struct Actor *actor);
extern void HideActorRenderRecord(struct Actor *actor);
extern void EncodeHitNormalToActorGroundCode(struct Actor *actor);
extern void DespawnActorRecord(struct Actor *actor);

extern int FindGroundHeightBelow(int *pos, int depth);

/* pos: a world vec3, or the Actor * for actor-anchored types; effectArg: a
 * colour / radius / life word, or a velocity pointer cast to int (EmitEffect). */
extern void (*g_pfnLevelEmitParticles)(int count, int type, void *pos, int effectArg);

extern short g_anCosineLut[]; /* unit-circle x table (8-bit angle) */

extern short g_anSineLut[]; /* unit-circle y table (8-bit angle) */

extern int g_anFlightClearTimes[];   /* per flight level first-clear time */

extern int RandomIntInRange(int lo, int hi);
extern int RandomSignedInRange(int lo, int hi);
extern int StepAndLerpAngle8(int to, int from, int maxStep, int frac);

/* Declared identically (by type) in two or more including files; hoisted here. */
extern int g_anFlightTargetTally;
extern char *g_apSpritePrimQueue[];
extern int g_dwHudIconSpinAngle;
extern char g_apMainDrawEntityList[];
extern char *g_pRenderScratchRegionBase;
extern struct Actor *g_pSpriteRecordWriteCursor; /* sprite-record queue tail (grows down) */

#endif /* OPEN_SPYRO_LEVEL_SPAWN_ACTOR_H */
