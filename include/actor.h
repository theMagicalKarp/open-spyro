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

typedef struct Actor {
  void *state;         /* 0x00 class-specific state block */
  int unk04;           /* 0x04 */
  int unk08;           /* 0x08 collision group */
  int posX;            /* 0x0C position */
  int posY;            /* 0x10 */
  int posZ;            /* 0x14 */
  int flags;           /* 0x18 damage flags */
  int unk1C;           /* 0x1C shadow distance */
  int unk20[5];        /* 0x20 rotation matrix */
  short unk34;         /* 0x34 collision region */
  short type;          /* 0x36 actor class */
  short unk38;         /* 0x38 floor distance */
  unsigned char unk3A; /* 0x3A dropped flag */
  unsigned char unk3B; /* 0x3B */
  unsigned char unk3C; /* 0x3C current animation */
  unsigned char unk3D; /* 0x3D next animation */
  unsigned char unk3E; /* 0x3E current frame */
  unsigned char unk3F; /* 0x3F next frame */
  unsigned char unk40; /* 0x40 frame progress */
  unsigned char unk41; /* 0x41 progress per frame */
  unsigned char unk42; /* 0x42 anim flags: bit 1 done, bit 0 new frame */
  unsigned char unk43; /* 0x43 pod (actor group) */
  unsigned char unk44; /* 0x44 rotation x */
  unsigned char unk45; /* 0x45 rotation y */
  unsigned char unk46; /* 0x46 rotation z */
  unsigned char unk47; /* 0x47 depth offset */
  unsigned char unk48; /* 0x48 state; >= 0x80 means dormant */
  unsigned char unk49; /* 0x49 sub-state */
  unsigned char unk4A; /* 0x4A sector index */
  unsigned char unk4B; /* 0x4B renderer */
  unsigned char unk4C; /* 0x4C */
  unsigned char unk4D;
  unsigned char unk4E;
  unsigned char unk4F;
  unsigned char unk50; /* 0x50 render radius */
  unsigned char unk51; /* 0x51 was drawn last frame */
  unsigned char unk52; /* 0x52 update distance */
  unsigned char unk53;
  unsigned char unk54; /* 0x54 sound channel */
  unsigned char unk55; /* 0x55 sound distance */
  unsigned char unk56; /* 0x56 actor index */
  unsigned char unk57; /* 0x57 scale override */
} Actor; /* 0x58 */

/* Named actor-system entry points. */
void BeginGemPickupOverlay(Actor *actor);
void DespawnActorRecord(Actor *actor);
void EncodeCachedVecToActorDirCode(Actor *actor);
int GetSoundVoiceStatusByOwner(Actor *actor, int a);
void HideActorRenderRecord(Actor *actor);
void InitActorRenderDefaults(Actor *actor);
void InsertActorIntoSpatialGrid(Actor *actor);
void PlaySoundEffect(int a, Actor *actor, int b, void *c);
void StopSoundVoicesByOwner(Actor *actor, int mode);
void TickGemCutsceneActorAnims(Actor **list, int mode);
void TickGemPickupBurstEntity(Actor *actor);

/* Actor behaviour helpers (main 0x80037E98..0x8003C994 and neighbours), not
 * yet named. */
void BeginSaveMenuFromWorld(Actor *actor);
void func_80037E98(Actor *actor);
int ProbeActorGroundHeight(Actor *actor);
int IsActorGroundFlat(Actor *actor);
int ProbeActorGroundHeightRaised(Actor *actor, int a);
void SnapActorToGround(Actor *actor);
int IsActorInRunningKillSet(Actor *actor);
void PlayActorMeshVoice(Actor *actor, int a, int b);
void func_800385BC(Actor *actor, int a);
int TurnActorYawTowardAngle(Actor *actor, int heading, int a, int b, int c);
int SteerActorYawTowardSpyro(Actor *actor, int *a, int *b, int c, int d, int e);
int MoveActorByDelta(Actor *actor, int vx, int vy, int vz, int a, int b, int c);
int MoveActorAlongHeading(Actor *actor, int a, int b, int c, int d);
int MoveActorAlongAngle(Actor *actor, int a, int b, int c, int d, int e);
int DriveActorTrackedMove(Actor *actor, int *a, int heading, int *b, int c, int d);
void UpdateActorWander(Actor *actor, void *st);
int SelectActorLodBand(Actor *actor, int handle);
void func_8003AA84(Actor *actor);
int SpawnActorRotBillboard(Actor *actor, int *tbl);
Actor *func_8003ABC0(Actor *actor, int a, int b, void *c);
void SetActorGroupState(Actor *actor, int a);
void OrActorGroupFlagsInCone(Actor *actor, int heading, int a, int b, int c, int d, int e);
void SetActorGroupStateExcept(Actor *actor, int a, int b);
void ReplaceActorGroupState(Actor *actor, int a, int b);
void SetActorGroupSubstate(Actor *actor, int a);
void MarkActorCommittedKill(Actor *actor);
void CollectPickupActor(Actor *actor);
int TryMoveActorForward(Actor *actor, int a, int b, int c, int d);
void func_8003C358(Actor *actor, int mode);
void FollowActorWaypointPath(Actor *actor, void *path, int a, int b, int c, int d, int e);
void func_800529E4(Actor *actor, int mode);
void func_80059474(Actor *actor, int heading);

#endif /* OPEN_SPYRO_ACTOR_H */
