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
int GetSoundVoiceStatusByOwner(Actor *actor, int subId);
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
int ProbeActorGroundHeightRaised(Actor *actor, int delta);
void SnapActorToGround(Actor *actor);
int IsActorInRunningKillSet(Actor *actor);
void PlayActorMeshVoice(Actor *actor, int voiceOffset, void *owner);
void BurstActorWithSound(Actor *actor, int mode);
#ifndef YAW_HOLD_IN_BAND
#define YAW_HOLD_IN_BAND 0 /* inside the threshold band: stop turning */
#define YAW_STEP_IN_BAND 1 /* inside the band: keep stepping onto the target */
#endif
int TurnActorYawTowardAngle(Actor *actor, int target, int step, int threshold, int flag);
int SteerActorYawTowardSpyro(Actor *actor, int *dirp, int *yawp, int speed, int thresh, int animId);
int MoveActorByDelta(Actor *actor, int vx, int vy, int vz, int contactRadius, int radius, int flags);
int MoveActorAlongHeading(Actor *actor, int speed, int contactRadius, int radius, int flags);
int MoveActorAlongAngle(Actor *actor, int angle, int speed, int contactRadius, int radius, int flags);
int DriveActorTrackedMove(Actor *actor, int *counter, int heading, int *timer, int step, int decay);
void UpdateActorWander(Actor *actor, WanderBlock *wb);
int SelectActorLodBand(Actor *actor, int band);
void TickActorFlameGlow(Actor *actor);
int SpawnActorRotBillboard(Actor *actor, int *offset);
Actor *SpawnActorDeathDrop(Actor *src, int kind, int *hint, int *alt);
void SetActorGroupState(Actor *actor, int value);
void OrActorGroupFlagsInCone(Actor *actor, int group, unsigned int bits, int distLim, int zLim, int xyLim, int angLim);
void SetActorGroupStateExcept(Actor *actor, int value, int match);
void ReplaceActorGroupState(Actor *actor, int value, int match);
void SetActorGroupSubstate(Actor *actor, int value);
void MarkActorCommittedKill(Actor *actor);
void CollectPickupActor(Actor *actor);
int TryMoveActorForward(Actor *actor, int speed, int contactR, int collideR, int flags);
#ifndef NAME_LETTERS_RETURN_HOME
#define NAME_LETTERS_RETURN_HOME 0 /* "RETURN HOME", facing Spyro (level exits) */
#define NAME_LETTERS_LEVEL       1 /* the sign's level name, along its yaw (portals) */
#endif
void SpawnNameLetterActors(Actor *actor, int named);
void FollowActorWaypointPath(Actor *actor, PathData *path, int threshold, int cap, int radius, int yawLim, int pitchLim);
void RefreshActorDerivedState(Actor *actor, int mode);
void SetSpawnAnchorFromActor(Actor *actor, int heading);

/* Hoisted: declared identically (bar parameter names) in every includer. */
extern Actor *AllocActorRecordSlot(void);
extern Actor *g_pActorCutsceneRecord;

#endif /* OPEN_SPYRO_ACTOR_H */
