/* actor.h — the 0x58-byte actor ("moby") record and the main-EXE actor API
 * the level overlays call.
 *
 * Every actor class shares this record: ->type picks the behaviour, ->state
 * points at the class's own block, and 0x3C..0x42 hold the animation state.
 * Field comments are the per-level analysis carried over from the overlay
 * megafunctions; unkNN names keep their offset until a field is confirmed.
 *
 * Files whose matched source views the record with a different layout keep
 * their own typedef (cookbook A400: the view's form can steer scheduling). */

#ifndef OPEN_SPYRO_ACTOR_H
#define OPEN_SPYRO_ACTOR_H

#include "actor_type.h"

/* Named actor-system entry points. */
void BeginDragonRescue(Actor *actor);
void DespawnActorRecord(Actor *actor);
void EncodeCachedVecToActorDirCode(Actor *actor);
int GetSoundVoiceStatusByOwner(Actor *actor, int a);
void HideActorRenderRecord(Actor *actor);
void InitActorRenderDefaults(Actor *actor);
void InsertActorIntoSpatialGrid(Actor *actor);
unsigned int PlaySoundEffect(unsigned int sampleId, Actor *owner, unsigned int mode, unsigned char *pPlaying);
void StopSoundVoicesByOwner(Actor *actor, int mode);
void TickActorAnims(struct Actor **list, int mode);
void TickStatueBurstShard(Actor *actor);

/* Actor behaviour helpers (main 0x80037E98..0x8003C994 and neighbours), not
 * yet named. */
void BeginSaveMenuFromWorld(Actor *actor);
void OnActorAnimQueued(Actor *actor, ...); /* no-op; SteerActorYawTowardSpyro passes two extra args */
int ProbeActorGroundHeight(Actor *actor);
int IsActorGroundFlat(Actor *actor);
int ProbeActorGroundHeightRaised(Actor *actor, int a);
void SnapActorToGround(Actor *actor);
int IsActorInRunningKillSet(Actor *actor);
void PlayActorMeshVoice(Actor *actor, int voiceOffset, void *owner);
void BurstActorWithSound(Actor *actor, int a);
int TurnActorYawTowardAngle(Actor *actor, int heading, int a, int b, int c);
int SteerActorYawTowardSpyro(Actor *actor, int *a, int *b, int c, int d, int e);
int MoveActorByDelta(Actor *actor, int vx, int vy, int vz, int a, int b, int c);
int MoveActorAlongHeading(Actor *actor, int a, int b, int c, int d);
int MoveActorAlongAngle(Actor *actor, int a, int b, int c, int d, int e);
int DriveActorTrackedMove(Actor *actor, int *a, int heading, int *b, int c, int d);
void UpdateActorWander(Actor *actor, WanderBlock *wb);
int SelectActorLodBand(Actor *actor, int band);
void TickActorFlameGlow(Actor *actor);
int SpawnActorRotBillboard(Actor *actor, int *offset);
Actor *SpawnActorDeathDrop(Actor *src, int kind, int *hint, int *alt);
void SetActorGroupState(Actor *actor, int a);
void OrActorGroupFlagsInCone(Actor *actor, int group, unsigned int bits,
                             int distLim, int zLim, int xyLim, int angLim);
void SetActorGroupStateExcept(Actor *actor, int a, int b);
void ReplaceActorGroupState(Actor *actor, int a, int b);
void SetActorGroupSubstate(Actor *actor, int a);
void MarkActorCommittedKill(Actor *actor);
void CollectPickupActor(Actor *actor);
int TryMoveActorForward(Actor *actor, int a, int b, int c, int d);
void SpawnNameLetterActors(Actor *actor, int mode);
void FollowActorWaypointPath(Actor *actor, PathData *path, int threshold, int cap, int radius, int yawLim, int pitchLim);
void RefreshActorDerivedState(Actor *actor, int mode);
void SetSpawnAnchorFromActor(Actor *actor, int heading);

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern Actor *AllocActorRecordSlot(void);
extern Actor *g_pActorCutsceneRecord;

#endif /* OPEN_SPYRO_ACTOR_H */
