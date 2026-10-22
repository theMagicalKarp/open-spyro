/* Level26_UpdateActors -- per-frame actor update for level 26 (Lofty
 * Castle).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 2,
 * 4, 9, 16 and 30); the level-26 classes are the bomb-lobbing lizard and its
 * bomb (52, 55), the sinking block (61), the path guard (62), the escort (88),
 * the wandering critter (160), the bobbing platform and its rider (296, 298),
 * the spinning chest (390) and the homing spark (392).
 *
 * Load-bearing source forms (matched 2026-09-29):
 *   - `spy` is the only loop-top carrier. It ties with the hoisted constant 1
 *     for the last callee-saved register, so every Spyro read the original
 *     makes through fp must go through `spy` (cases 300 and 312 included).
 *   - Scratch arrays at function level in first-use order, with a BLKmode
 *     `idx[2]` after svE for case 62's FindNearestPathPointToActor out-argument.
 *   - Case 61 settles with `z ? z : (t = posZ - 0x200, t - depth)`: the comma
 *     temp stops fold from reassociating the subtraction.
 *   - Case 392's |dx| + |dy| goes through `d` (ABS2 form) and its tests are
 *     written positively; case 62 splits its knock-back test into two
 *     remove blocks and puts the out-of-range ENTER_POSE last.
 *   - Case 421 stores `st->unk14` (known to be 1) rather than a literal.
 *   - Case 298 copies the parent's rotation bytes as one 3-byte struct (lb).
 *   - Case 390 addresses the voice table as `g_anSpuVoiceRowPitch[k][0]` and its word 3
 *     through `g_anSpuVoiceRowPitchStep[k][0]`.
 *   - Case 52 reads its heading as `SPY.bodyRotZ`.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"

/* Node accessors for paths held as byte pointers (type 286). */
#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))
#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))
#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))

/* Type 286: ambient sound source; mode 2 also coasts along a path. */

extern int ComputeStepTowardPoint(int *from, int *to, int *vel, int a);

/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

/* Step a countdown timer; non-zero when it fires. */
extern int WrapAddAngle8(int a, int b);

/* Mark an item collected; the second argument is the actor or its id. */

extern int SteerActorAroundPoint(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);

extern void ModifyWorldChunkAnimFlags(int handle, int a, int b);
extern void SetWorldChunkAnimStep(int handle, int a, int b);
extern int GetWorldChunkAnimState(int handle); /* bits 8..15 are the anim marker */

#define AS_I32(sym) (((I32 *)&(sym))->v)

#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */

#define LIFT_STEP(st) (((st)->phase - 0x1A0) >> 1)

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */

#define CAM g_anWorldToCameraRotMtx

extern int FindGroundHeightBelow(int *v, int k);
extern int GetGroundTriggerRegionType(void);

extern int TrackEntityYawTowardSpyro(Actor *actor, int a, int b, int c);

/* Glow node owned by Sparx; follows its anchor position. */

extern int CastRayWorldAndActors(int *a, int *b);

/* Shift a vector left by n. */

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define ABS2(x) ((x) > 0 ? (x) : -(x))

/* Animation state: current/next animation (0x3C/0x3D), current/next frame
 * (0x3E/0x3F), frame progress (0x40) and progress per frame (0x41).
 *
 * ANIM_RESET restarts `anim` from frame 0 at the model's own speed. */
#define ANIM_RESET(a, id)                                                    \
  (a)->frameProgress = 0;                                                              \
  (a)->frameRate = TYPE_DESC(a, id);                                             \
  (a)->anim = (id);                                                         \
  (a)->nextAnim = (id);                                                         \
  (a)->frame = 0;                                                              \
  (a)->nextFrame = 1;

/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (next);                                                         \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = 0;                                                              \
  StubActorHook(a);

/* ENTER_POSE sets the state byte to `n`, advances to animation `n` unless it
 * is already queued, and moves on to the next actor. It ends in a bare
 * `continue`, so use it only inside braces. */
#define ENTER_POSE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */

/* Case 34 — the vortex/whirlwind pad that lifts Spyro out of the level. */
typedef struct VortexState {
  int idx;               /* 0x00 */
  short centre[3];       /* 0x04 pad centre */
  short apex[3];         /* 0x0A rise target */
  unsigned short radius; /* 0x10 */
  unsigned char phase;   /* 0x12 */
  unsigned char angle;   /* 0x13 */
} VortexState;

/* Cases 14/15/83..87 — the generic enemy: a walker with a chase/charge/knock
 * -back state machine driven by ->subMode. */
typedef struct EnemyState {
  int vel[3];           /* 0x00 velocity, also read as a vector */
  short node;           /* 0x0C current path node */
  unsigned char mode;   /* 0x0E sub-mode within ->subMode */
  unsigned char timer;  /* 0x0F */
  unsigned char bounce; /* 0x10 remaining knock-back bounces */
  signed char lean;     /* 0x11 body pitch */
  signed char roll;     /* 0x12 */
  unsigned char spin;   /* 0x13 */
  unsigned char attach; /* 0x14 attached smoke/spark handle, 0xFF = none */
} EnemyState;

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 pitch / yaw / roll per frame */
  int life;      /* 0x0C */
  int floor;     /* 0x10 */
} ShardState;

/* Case 398 — the level-exit vortex: drives the scripted camera and hands the
 * game over to the world map. */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int unk10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;

extern volatile unsigned char
    g_bSpyroPersistentEulerZ;          /* SPY.bodyRotZ, absolute form (A240) */
extern int g_nLevelReadyFlag[]; /* balloon sequence stage (== SPY.u164) */

#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y

extern void ModifyWorldChunkAnimFlags(int ch, int vol, int a);
extern void SetWorldChunkAnimStep(int ch, int n, int a);
extern int GetWorldChunkAnimState(int ch);
extern int ComputeStepTowardPoint(int *from, int *to, int *step, int speed);

/* An animation change that also clears the finished flag. */
#define ANIM_GO(a, n)                                                          \
  if ((a)->nextAnim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_ADVANCE(a, n);                                                        \
  }
#define ANIM_SET(a, n)                                                         \
  if ((a)->anim != (n)) {                                                     \
    g_nActorAnimDone = 0;                                                            \
    ANIM_RESET(a, n);                                                          \
  }

/* ENTER_POSE with the already-there test first. */
#define ENTER_POSE4(a, n)                                                      \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim == (n)) {                                                     \
    continue;                                                                  \
  }                                                                            \
  ANIM_ADVANCE(a, n);                                                          \
  continue

/* ==== Level 16 classes ==== */

extern int DispatchActorContactAtSphere(int *pos, int a, int b, int c, Actor *actor, int d);

/* Case 328 -- floor-snapped marker. */

extern int TweenActorAlongPath(Actor *actor, PathData *path, int *a, int *b, int c,
                         int d);

/* ENTER_POSE with the already-there test first. */
/* ==== Level 2 classes ==== */

/* An animation change at half speed, keyed on the current animation. */
#define ANIM_BLEND(a, n)                                                       \
  if ((a)->anim != (n)) {                                                     \
    (a)->frameProgress = 8;                                                            \
    (a)->frameRate = 8;                                                            \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }
extern int FindNearestPathPointToActor(Actor *actor, PostPath *path, int *idx);
/* ENTER_POSE with an empty loop at the head of the change: cse starts a new
 * block there, so the `unk3D = n` store takes its own copy of the constant
 * (merged again after loop.c). Keeps the constant's lifetime short enough
 * that loop.c does not hoist it (see case 115). */
#define ENTER_POSE_B(a, n)                                                     \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

/* ==== Level 9 classes ==== */

extern int SteerActorTowardPoint(Actor *actor, int *target, int a, int b, int c,
                          int d);

/* ==== Level 26 classes ==== */

#define sixteen 0x10
/* ==== The function ==== */
void Level26_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int dt;

  /* Scratch vectors, in first-use order. */
  int svA[3];
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int idx[2];
  int t144[3];
  int t160[3];
  int acc[3][3];
  int t216[3];
  int t232[3];
  int t248[3];
  int t264[3];
  int t280[3];
  int t296[3];
  int fx[6];
  int col[3];
  int t352[3];
  int t368[3];
  int t384[3];
  int t400[3];
  int t416[3];
  int ps[6];
  int t456[3];
  int t472[3];
  int col2[3];
  int t504[3];
  int t520[3];
  int t536[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  base = (char *)&g_nSpyroWorldPosZ;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    spy = (SpyroObj *)(base - 8);

    if (actor->mode >= 0x80) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Exit drop (DropState). */
    case 9: {
      DropState *st = actor->state;
      int dist = ApproxDist2D(&actor->posX, &CAM.posX);
      int i;

      if (actor->mode != 0 ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == 0) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = 1;
      } else {
        actor->mode = 0;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, 0x15, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, 0x4C, &actor->posX, (void *)tick);
        }
      }

      if (dist < 0x400) {
        int top = g_nSpyroWorldPosZ;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == 0x11 || top + 0xC00 < z + depth) {
            if (g_abHudCounterDisplayState == 2) {
              RefreshHudLevelGemCounter();
            }
            g_dwSpyroRequestMask = 0x80008000;
            g_pScriptedCameraActor = actor;
            g_dwSpyroTriggerEventFlags |= 0x800;
            g_nSpyroRequestTargetZ = *(int *)((char *)actor + 0x14) + 0x4000;
            CopyVector((int *)(base + 0x174), &actor->posX);

            if (depth < drop) {
              int yaw = g_nCameraEulerPitch & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -0x200) {
                g_abLevelGemPercent[g_nLevelIntroIndex] = 1;
                g_nCdStreamState = 0;
                g_nPreserveMobIdx = -1;
                g_nScriptedRespawnFlag = 1;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = 1;
                g_nGamePaused = 1;
                g_nScriptedMobTag = g_nActiveLevelId;
                g_nCurrentLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = 0x80000012;
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(0);
                g_nSpyroFallTrackingFlag = 0;
                g_nSpyroFlameBreathTimer = 0;
              }
            }
          }
        }
      }

      break;
    }

    /* Tethered follower (FollowState): rides a parent actor until the parent
     * goes dormant, then takes the parent's transform over and re-points the
     * other followers of that parent at itself. */
    case 13: {
      FollowState *st = actor->state;
      Actor *parent = &g_pActorListBase[st->parent];

      if (st->detached == 0) {
        if (st->trackPos != 0) {
          SubtractVector(st->pos, st->pos, &parent->posX);
          actor->updateDist = 0xFF;
        } else {
          actor->updateDist = 0x20;
        }

        if (st->active != 0 &&
            (parent->mode >= 0x80 || (parent->dropKind & 0x80))) {
          Actor *scan;
          int self;

          parent->dropKind = actor->dropKind;
          DespawnActorRecord(actor);

          if (parent->mode < 0x80) {
            DespawnActorRecord(parent);
          }

          /* Take the parent's whole record over (gcc's block_move_loop). */
          *actor = *parent;
          InsertActorIntoSpatialGrid(actor);

          actor->drawn = 1;
          self = actor - g_pActorListBase;
          parent = g_pActorListBase;

          for (; parent < g_pActorPoolDynBase; parent++) {
            if (parent != actor && parent->mode < 0x80 && parent->type == 13) {
              FollowState *other = parent->state;

              if (other->parent == st->parent) {
                other->parent = self;
              }
            }
          }

          actor->mode = 0;
          ANIM_RESET(actor, 0);
        } else {
          st->detached = 1;
          goto follow_carry;
        }
      } else {
      follow_carry:
        if (st->trackPos != 0) {
          CopyVector(&actor->posX, &parent->posX);
        }

        if (st->release != 0) {
          /* Released: drift away and pop. */
          if (TickCountdownByFrameDelta(&st->release, 4) != 0) {
            int *vel = st->pos;

            if (st->trackPos != 0) {
              AddVector(vel, vel, &actor->posX);
            } else {
              CopyVector(&actor->posX, &parent->posX);
            }

            st->pos[2] += 0x400;

            if (st->dropToSpyro != 0) {
              SpawnActorDeathDrop(actor, 4, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              SpawnActorDeathDrop(actor, 2, 0, st->pos);
            } else {
              SpawnActorDeathDrop(actor, 1, 0, 0);
            }
            MarkActorCommittedKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                *(unsigned short *)(g_pLevelSampleBankEntries + g_pLevelSampleBankHeader[0x37] * 20 + 0xA) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= 0x80) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < 0x80 && parent->type == 13 &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                *(unsigned short *)(g_pLevelSampleBankEntries + g_pLevelSampleBankHeader[0x37] * 20 + 0xA);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case 16: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.u164 < 3 && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pLevelOverlayInitRecord == 0) {
            if (SPY.u164 >= 0) {
              g_pLevelOverlayInitRecord = g_pfnLevelSpawnActor(120, actor);

              PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);

              SPY.u164 = 1;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pLevelOverlayInitRecord->subMode != 99) {
            ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) <
                  0) {
                st->dir = 1;
              } else {
                st->dir = -1;
              }

              g_pLevelOverlayInitRecord->subMode = 0;
              cs->owner = actor;
              cs->timer = RandomIntInRange(150, 210);

              actor->mode = 1;
            }
          }
        }
      }

      if (actor->cullRadius & 0x80) {
        break;
      }

      switch (actor->mode) {
      case 0: {
        if (TickCountdownByFrameDelta(&st->idleTimer, 2) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TickCountdownByFrameDelta(&st->headingTimer, 1) != 0) {
            int r = RandomSignedInRange(30, 90);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->headingTimer = RandomIntInRange(60, 140);
          }

          if (TickCountdownByFrameDelta(&st->climbTimer, 1) != 0) {
            st->vz = RandomSignedInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 300) {
            st->vz = RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (1024 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, 1);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, 0);

            st->heading = angle;
            st->headingTimer = 40;
          }
        } else {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      case 1: {
        SteerActorAroundPoint(actor, &spy->posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pLevelOverlayInitRecord == 0 || g_pLevelOverlayInitRecord->subMode == 99) {
          actor->mode = 0;
        } else {
          int refHeight;

          if (TickCountdownByFrameDelta(&st->climbTimer, 1)) {
            st->vz = RandomSignedInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 200) {
            st->vz = RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (1000 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }
        }
        break;
      }
      }

      break;
    }

    /* Vortex pad (VortexState): spins Spyro up and out of the level. */
    case 34: {
      VortexState *st = actor->state;
      unsigned int phase = actor->mode;

      if (phase != 0 && phase < 5) {
        g_dwSpyroRequestMask = 0x80002000;
        if (g_nSpyroState != 0 && g_nSpyroState != 0xC) {
          actor->mode = 5;
        }
      }

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode += 1;
        break;

      case 2: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x80) {
          int step = g_nFrameStep * 4;

          st->radius += step;
          st->angle += step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX +=
              (LookupCosine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (LookupSine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 8;
          actor->rotZ += g_nFrameStep * 4;
        } else {
          int cb;
          int cs;
          int x;
          int y;

          CopyVector(svA, &spy->posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          svA[0] += (y + x) * 4 >> 12;
          svA[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          svA[2] += 0x300;
          VectorToSVectorShr2(st->apex, svA);
          VectorToSVectorShr2(st->centre, &actor->posX);
          st->phase = 0;
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 3: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x40) {
          SVectorToVectorShl2(svB, st->centre);
          SVectorToVectorShl2(svC, st->apex);
          SubtractVector(svC, svC, svB);
          ScaleVector3Sat(svC, svC, st->phase);
          RShiftVector3(svC, 6);
          AddVector(&actor->posX, svB, svC);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(svD, &spy->posX);
          svD[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[2] += 0x300;
          VectorToSVectorShr2(st->centre, svD);
          st->radius = 0x200;
          st->angle = g_bSpyroPersistentEulerZ - 0x40;
          st->phase = 0;
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 4: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x80) {
          int step = g_nFrameStep * 4;

          st->radius -= step;
          st->angle -= step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX +=
              (LookupCosine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posY +=
              (LookupSine(st->angle * 0x10) * (short)st->radius) >> 12;
          actor->posZ -= st->phase * 4;
          actor->rotZ += g_nFrameStep * 4;
        } else {
          actor->mode += 1;
        }
        if (g_dwPadPressed & 0x40) {
          actor->mode = 5;
        }
        break;
      }

      case 5: {
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, 0x4D, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->idx);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Type 52: bomb-lobbing lizard. Hides until Spyro is in range, then
     * lobs type 55 bombs at him; knocked away when hit. */
    case 52: {
      L26Lobber *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode != 2) {
        st->speed = 0xF0;
        st->heading = SPY.bodyRotZ;
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && actor->drawn != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x2800) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0xA00
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0xA00) {
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        if (actor->nextFrame >= 6 && st->timer == 0) {
          RefreshActorDerivedState(actor, 4);
          g_pfnLevelEmitParticles(0x10, 0x1C, g_pfnLevelSpawnActor(0x37, actor), (void *)0x10);
          st->timer = st->reload;
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x28);
          DespawnActorRecord(actor);
          continue;
        }
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0x1F4, 0x2BC, 1);
          st->speed -= 0x10;
        }
        break;
      }
      break;
    }

    /* Type 55: lobbed bomb. Bursts on landing, on a timer or against
     * Spyro, then plays its burst and is removed. */
    case 55: {
      int *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (actor->posX < 0x400 || actor->posY < 0x400 ||
            actor->posZ < 0x400) {
          actor->mode = 1;
          break;
        }
        CopyVector(svD, &actor->posX);
        if (*st < 0x64) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, 0x20000, actor, 0);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 7) != 0 ||
            TickCountdownByFrameDelta(st, 4) != 0 ||
            TryMoveActorForward(actor, 0xE0, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(svD, &actor->posX) != 0) {
          actor->mode = 1;
        }
        g_pfnLevelEmitParticles(1, 0x1C, actor, (void *)8);
        break;

      case 1:
        g_pfnLevelEmitParticles(0x10, 0x1C, actor, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Type 61: sinking block. Struck, it frees its partner and sinks into
     * the floor. */
    case 61: {
      L26Sink *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode == 0) {
        st->depth = 0;
        g_pActorListBase[st->partner].mode = 1;
        PlayActorMeshVoice(&g_pActorListBase[st->partner], 0, 0);
        MarkActorCommittedKill(&g_pActorListBase[st->partner]);
        CommitActorKillGems(0, &g_pActorListBase[st->partner]);
        MarkActorCommittedKill(actor);
        CommitActorKillGems(0, actor);
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;
      }
      if (actor->mode == 1) {
        int z;
        int t;

        st->depth += 0xC;
        if (st->depth > 0x78) {
          st->depth = 0x78;
        }
        actor->posZ += 0x200;
        z = FindGroundHeightBelow(&actor->posX, st->depth + 0x200);
        actor->posZ = z ? z : (t = actor->posZ - 0x200, t - st->depth);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 62: path guard. Patrols its path (or stands at home), turns to
     * face Spyro and gives chase, and is knocked away when hit. */
    case 62: {
      L26Guard *st = actor->state;
      int d;
      int ang;
      int a;

      if ((actor->flags & 0xB0000) && actor->mode != 4) {
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        if (actor->flags & 0x80000) {
          st->speed = (spy->u118 >> 6) + 0x1C2;
          st->lift = 0x50;
        } else if (actor->flags & 0x20000) {
          st->speed = 0x1F4;
          st->lift = 0x20;
        } else {
          st->speed = 0x12C;
          st->lift = 0x10;
        }
        SpawnActorDeathDrop(actor, 4, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 4);
      }
      if (st->node < 0) {
        if (st->onPath != 0) {
          st->node = st->path->cur;
          st->path->reversed = -1;
        } else {
          st->node = 0;
        }
      }
      if (TickCountdownByFrameDelta(&st->sndTimer, 4) != 0 && actor->mode < 3) {
        int s = (int)GetRandomU32() % 3;

        while (s == st->lastSnd) {
          s = (int)GetRandomU32() % 3;
        }
        PlayActorMeshVoice(actor, s, 0);
        st->lastSnd = s;
        st->sndTimer = (int)GetRandomU32() % 45 + 0x5A;
      }

      switch (actor->mode) {
      case 0:
        if (st->onPath != 0) {
          FollowActorWaypointPath(actor, st->path, 0x400, 0x5C, 0x180, 4, 2);
        } else {
          int r;
          int e;

          TrackEntityYawTowardSpyro(actor, 3, 0, 0);
          r = actor->rotY;
          e = -r & 0xFF;
          if (e > 0x80) {
            e -= 0x100;
          }
          if (e > 2) {
            e = 2;
          }
          if (e < -2) {
            e = -2;
          }
          actor->rotY = r + e;
        }
        if (actor->drawn != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x800) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if ((dz > 0 ? dz < 0x800
                      : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x800) &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0)) <
                  0x20 &&
              CheckSpyroLineOfSightTo(&actor->posX) != 0) {
            st->timer = 0x78;
            ENTER_POSE(actor, 2);
          }
        }
        break;

      case 1:
        if (st->onPath != 0) {
          d = FindNearestPathPointToActor(actor, (PostPath *)st->path, idx);
          st->path->cur = idx[0];
          if (d < 0x200) {
            ENTER_POSE(actor, 0);
          }
          ang = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY,
                              0);
          a = (ang - actor->rotZ) & 0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS2(a) < 0x18) {
            d >>= 4;
            if (d > 0x5C) {
              d = 0x5C;
            }
            if (st->timer != 0) {
              if (SteerActorTowardPoint(actor, &st->path->nodes[st->path->cur].x, d,
                                4, 2, 0x180) != 0) {
                TickCountdownByFrameDelta(&st->timer, 4);
              } else {
                st->timer = 0x3C;
              }
            } else {
              SteerActorTowardPoint(actor, &st->path->nodes[st->path->cur].x, d, 4,
                            2, 0);
            }
            break;
          }
        } else {
          SubtractVector(svA, &actor->posX, st->home);
          d = VectorLength(svA, 1);
          if (d < 0x200) {
            ENTER_POSE(actor, 0);
          }
          ang = ArcTan2(st->home[0] - actor->posX,
                              st->home[1] - actor->posY, 0);
          a = (ang - actor->rotZ) & 0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS2(a) < 0x18) {
            d >>= 4;
            if (d > 0x5C) {
              d = 0x5C;
            }
            if (st->timer != 0) {
              if (SteerActorTowardPoint(actor, st->home, d, 4, 2, 0x180) != 0) {
                TickCountdownByFrameDelta(&st->timer, 4);
              } else {
                st->timer = 0x3C;
              }
            } else {
              SteerActorTowardPoint(actor, st->home, d, 4, 2, 0);
            }
            break;
          }
        }
        TurnActorYawTowardAngle(actor, ang, 4, 0, 0);
        break;

      case 2:
        SteerActorTowardPoint(actor, &spy->posX, 0x80, 4, 2, 0x180);
        if (actor->drawn != 0 && CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          if (AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          SPY.posY - actor->posY, 0)) < 0x18) {
            ENTER_POSE(actor, 3);
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->timer = 0x3C;
            ENTER_POSE(actor, 1);
          }
          break;
        }
        ENTER_POSE(actor, 1);

      case 3:
        SteerActorTowardPoint(actor, &SPY.posX, 0x40, 4, 2, 0x180);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x3C;
          ENTER_POSE(actor, 1);
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        if (DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC,
                          0x10) != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 5);
      break;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 255:
    case 256:
    case 423:
    case 424: {
      ShardState *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {

      if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
        int *nrm = &g_anCollisionProbeVec;
        int along;
        register int push asm("$7");

        SetVector3Magnitude(nrm, 0x1000);
        along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                (st->vel[2] * g_nCollisionProbeVecZ);
        push = along >> 11;

        if (push < 0) {
          ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - push);
          st->vel[0] += nrm[0];
          st->vel[1] += AS_I32(g_nCollisionProbeVecY);
          st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
        }
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      if (!(st->life & 3)) {
        svA[0] = GetRandomU32() & 3;
        svA[1] = GetRandomU32() & 3;
        svA[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, svA);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Falling chest fragments (TumbleState). */
    case 69:
    case 151:
    case 257:
    case 425: {
      TumbleState *st = actor->state;

      /* Retire when out of life, no longer drawn, or sunk to the floor. */
      if (st->life == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (actor->posZ <= st->floorZ) {
        DespawnActorRecord(actor);
        continue;
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];

      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];

      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      st->life -= 1;
      break;
    }

    /* The generic enemy: patrols, notices Spyro, charges, gets knocked back.
     * ->subMode is the outer state and st->mode the sub-state. */
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      EnemyState *st = actor->state;
      int toSpyro;

      toSpyro = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != 15) {
        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, 4);
          if (actor->type == 14) {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset5,
                          g_aRotBillboards[st->attach].pos);
          } else {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset3,
                          g_aRotBillboards[st->attach].pos);
          }
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);

          if (handle >= 0 && toSpyro < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x1F) + 0x21;
        }
      }

      if (actor->drawn != 0 || toSpyro <= 0x2000 || actor->subMode != 1 ||
          st->mode != 0) {
        switch (actor->subMode) {
        case 0:
          /* Idle: sample the floor normal and face along it. */
          CopyVector(t144, &actor->posX);
          t144[2] += 0x400;
          FindGroundHeightBelow(t144, 0x10000);
          {
            int *nrm = &g_anCollisionProbeVec;

            st->lean = -ArcTan2_8bit(
                IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                g_nCollisionProbeVecY);
            st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
          }

          if (st->lean != 0 || st->roll != 0) {
            actor->rotZ = 0;
          }
          actor->subMode = 1;
          goto enemy_pose;

        case 1: {

          if (st->mode == 0) {
          } else if (st->mode == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              {
                int *nrm = &g_anCollisionProbeVec;

                st->lean =
                    -ArcTan2_8bit(IntegerSqrt((nrm[0] * nrm[0]) +
                                                 (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                                   g_nCollisionProbeVecY);
                st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
              }
            }
          } else if (st->mode == 2) {
            UnpackWorldCollisionTri(st->node, acc[0]);
            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);
            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(t160, &actor->posX);
            t160[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(t160, 0x1000) > 0) {
              st->node = g_nLastCollisionHitChunkId;
              st->mode = 2;
            }
          }

          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pLevelOverlayInitRecord != 0 && g_nSpyroAirborneFrames == 0 &&
                CheckLineOfSightSegment(&g_pLevelOverlayInitRecord->posX, &actor->posX) != 0) {
              {
                BalloonState *bal = g_pLevelOverlayInitRecord->state;

                if (bal->gnorc == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pLevelOverlayInitRecord,
                          g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }
                  g_pLevelOverlayInitRecord->subMode = 4;
                  bal->gnorc = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

        case 2: {
          /* Knocked back: arc away, bounce, then settle. */
          int *at;
          int dist;

          ScaleVector3Sat(t216, (int *)st, g_nFrameStep);
          RShiftVector3(t216, 1);
          dist = VectorLength(t216, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(t216, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(t216, at, t216);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (t216[2] < 0) {
            if (actor->type != 14 && actor->type != 15) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          t216[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(t216, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(t232, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(t216, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(t232[2], VectorLength(t232, 0));

              if ((t216[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, t216);
                actor->posZ = floor;
                if (actor->type != 15) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((t232[0] * t232[0]) + (t232[2] * t232[2])),
                      t232[1]);
                  st->roll = -ArcTan2_8bit(t232[2], t232[0]);
                  if (st->lean != 0 || st->roll != 0) {
                    actor->rotZ = 0;
                  }
                }
                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->bounce += 1;
              }

              if (st->bounce == 0) {
                goto enemy_settle;
              }
            }

            g_nSfxOverrideFlags = 1;
            g_nSfxOverrideVolL = 0x3CCC >> (3 - st->bounce);
            g_nSfxOverrideVolR = 0x3CCC >> (3 - st->bounce);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

            if (ReflectVectorOffNormal((int *)st, t232, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            t216[2] -= 0xF0;
            CopyVector(at, t216);
            FindGroundHeightBelow(t216, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }

        enemy_settle:
          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pLevelOverlayInitRecord != 0 && g_nSpyroAirborneFrames == 0 &&
                st->bounce < 3) {
              {
                BalloonState *bal = g_pLevelOverlayInitRecord->state;

                if (bal->gnorc == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pLevelOverlayInitRecord,
                          g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }
                  g_pLevelOverlayInitRecord->subMode = 4;
                  bal->gnorc = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

          goto enemy_pose;

        case 3:
          /* Carried by the balloonist: pulled toward Spyro, then dropped. */
          if (st->timer >= 0x20) {
            CopyVector(&actor->posX, &SPY.posX);
          } else {
            SubtractVector(t248, &spy->posX, (int *)st);
            RShiftVector3(t248, 5);

            if (VectorLength(t248, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(t248, t248, st->timer);
              AddVector(&actor->posX, (int *)st, t248);
              actor->posZ += (short)(g_anSineLut[st->timer * 4] / 12);
            }
          }

          actor->rotX += (unsigned char)(st->lean - 7);
          do {
          } while (0);
          actor->rotY += (unsigned char)(st->roll - 7);
          do {
          } while (0);
          actor->rotZ += (unsigned char)(st->spin - 7);
          goto enemy_pose;

        case 4:
          continue;

        default:
        enemy_pose:
          if (actor->subMode < 3) {
            if (actor->type == 14) {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == 15) {
              actor->rotZ += 8;
              actor->rotY -= 6;
            } else {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 23) +
                  (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 23) +
                  (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            }
          }

          if ((st->timer >= 0x21 && toSpyro < 0x200 &&
               (unsigned int)((SPY.posZ - actor->posZ) + 0x17F) < 0x37F) ||
              (actor->subMode == 3 && st->timer >= 0x41)) {
            if (g_pLevelOverlayInitRecord != 0) {
              BalloonState *bal = g_pLevelOverlayInitRecord->state;

              if (bal->gnorc == actor) {
                bal->gnorc = 0;
              }
            }

            if (actor->type == 14) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
              RefreshActorDerivedState(actor, 4);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)8);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x10);
              if (g_nExtraLives < 0x63) {
                g_nExtraLives += 1;
              }
              CommitActorKillGems(0, actor);
              DespawnActorRecord(actor);
              continue;
            }

            if (actor->type == 15) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
              g_nSparxFoodCount += 1;
              DespawnActorRecord(actor);
            } else {

              CollectPickupActor(actor);
              DespawnActorRecord(actor);
              continue;
            }
          }
          break;
        }
      }

      break;
    }

    /* Type 88: escort. Follows its leader on a ring, joins its partner's
     * formation and flies off when the leader breaks away. */
    case 88: {
      L26Escort *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (actor->dropKind != 0) {
          CopyVector(&actor->posX, &g_pActorListBase[st->leader].posX);
          actor->mode = 2;
        }
        break;

      case 1:
        if (TweenActorAlongPath(actor, st->path, st->p0C, &st->timer, 0x20, 4) == 2) {
          actor->mode = 2;
        }
        break;

      case 2: {
        int ang = st->slot * 0x55;

        if (SteerActorAroundPoint(actor, &g_pActorListBase[st->leader].posX, 0x600, ang, 0,
                          0x80, 3, 0, 0xFF, 0xFF, 0, 0, 8) < 0x10) {
          actor->mode = 3;
        }
        goto follow;
      }

      case 3:
        if (st->partner >= 0) {
          g_pActorListBase[st->partner].subMode |= 1 << st->slot;
          if (g_pActorListBase[st->partner].mode == 4) {
            actor->mode = 4;
            st->speed = 0x20;
            st->timer = 0x78;
          }
        } else if (actor->subMode == 6 &&
                   ApproxDist2D(&g_pActorListBase[st->leader].posX, &SPY.posX) <
                       0x1800) {
          int *ls = g_pActorListBase[st->leader].state;

          actor->mode = 4;
          ls[2] = 0x10;
          st->speed = 0x20;
          st->timer = 0x78;
        }
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->leader].posX - actor->posX,
                          g_pActorListBase[st->leader].posY - actor->posY, 0),
            2, 0, 0);
      follow: {
        int z = actor->posZ;
        int t = z - 0x200;
        int d = g_pActorListBase[st->leader].posZ - t;

        if (d < -0x40) {
          d = -0x40;
        }
        if (d > 0x40) {
          d = 0x40;
        }
        actor->posZ = z + d;
        break;
      }

      case 4:
        SteerActorAroundPoint(
            actor, &g_pActorListBase[st->leader].posX, 0x600,
            (ArcTan2(actor->posX - g_pActorListBase[st->leader].posX,
                           actor->posY - g_pActorListBase[st->leader].posY, 0) +
             0x78) &
                0xFF,
            0, st->speed, 0x10, 0, 0xFF, 0xFF, 0, 0, 8);
        actor->posZ += (st->speed - 0x20) >> 3;
        st->speed += g_nFrameStep * 2;
        if (st->speed > 0x100) {
          st->speed = 0x100;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          g_pfnLevelEmitParticles(0x10, 0x50, &actor->posX, (void *)0xC);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 4);
      GetActorAttachPoint(actor, 0, t144);
      g_pfnLevelEmitParticles(2, 0x42, t144, (void *)2);
      break;
    }

    /* Lift plate (PlateState): rises once its trigger actor is gone, waits,
     * then drops back. */
    case 110: {
      PlateState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->trigger];
      Actor *rider = &g_pActorListBase[st->rider];

      switch (actor->mode) {
      case 0:
        actor->collision = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1:
        if (trigger->mode >= 0x80) {
          actor->mode = 2;
          st->timer = 0x10;
          st->baseZ = actor->posZ;
        }
        break;

      case 2:
        if (st->timer != 0) {
          st->timer -= 1;
        } else {
          actor->collision = 0;
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
              if (st->locked == 0 || rider->mode == 1) {
                int *at = &actor->posX;

                st->locked = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                             spy->posY - actor->posY, 0) -
                               0x80;
                actor->cullRadius = 0x20;
                actor->mode = 3;
                actor->scale = 0x60;
                g_pfnLevelEmitParticles(0x18, 0x50, at, (int *)0xC);
                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(at, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= 0x80000000;
              }
            }
          }
        }
        break;

      case 3:
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ += 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->step = 0x1E;
        }
        break;

      case 4: {
        int height;
        int drop;
        int close;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == 0 || g_nSpyroState == 0xD) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2401 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             close = drop > 0 ? drop < 0xC01 : (SPY.posZ - height) < 0xC01,
             close == 0)) {
          actor->mode = 6;
          break;
        }

        st->step -= 1;
        if (st->step == 0) {
          int reach = (GetRandomU32() & 0x3F) + 0x140;
          int bearing = ArcTan2(spy->posX - rider->posX,
                                      spy->posY - rider->posY, 0);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          do {
          } while (0);
          CopyVector(st->from, &rider->posX);
          aim &= 0xFF;
          aim *= 0x10;
          actor->mode = 5;
          st->from[0] -= (LookupCosine(aim) * reach) >> 12;
          st->from[1] -= (LookupSine(aim) * reach) >> 12;
          st->yaw = ArcTan2(spy->posX - st->from[0],
                                  spy->posY - st->from[1], 0);
          st->step = 0;
        }
        break;
      }

      case 5: {
        int height;
        int drop;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == 0 || g_nSpyroState == 0xD) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2801 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop >= 0xC01 : (SPY.posZ - height) >= 0xC01)) {
          actor->mode = 6;
          break;
        }
        {
          int *here = &actor->posX;
          int turn;
          int w0;
          int wSpan;
          int wDen;

          st->step += 1;
          SubtractVector(t264, st->from, here);
          turn = SignedAngleDelta8(st->yaw, actor->rotZ);
          w0 = LookupCosine((st->step - 1) << 7);
          wSpan = w0 - LookupCosine(st->step << 7);
          wDen = LookupCosine((st->step - 1) << 7) + 0x1000;
          actor->posX += (t264[0] * wSpan) / wDen;
          actor->posY += (t264[1] * wSpan) / wDen;
          actor->rotZ += (turn * wSpan) / wDen;
          FindGroundHeightBelow(here, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->step == 0x10) {
            actor->mode = 4;
            st->step = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }
        break;
      }

      case 6:
        if (actor->scale < 0x60) {
          actor->scale += 4;
          actor->rotZ += 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 0x10;
          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (int *)0xC);
          actor->groundCode &= 0x7FFFFFFF;
        }
        break;
      }

      break;
    }

    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (g_nLevelReadyFlag[0] >= 2) {
        t264[0] = 0;
        t264[1] = 0x64;
        t264[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], t264, t264);
        AddVector(t264, t264, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, t264, 0);
        t264[0] = 0;
        t264[1] = -0x64;
        t264[2] = 0;
        ApplyActiveGteRotation(t264, t264);
        AddVector(t264, t264, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, t264, 0);
      }

      if (g_nLevelReadyFlag[0] >= 3) {
        FxNode *fx = st->smoke;

        if (fx == 0) {
          fx = AllocWorldSparkle();
          st->smoke = fx;
          if (fx != 0) {
            fx->anchor = &actor->posX;
            st->smoke->size = 0x40;
            st->smoke->offX = 0;
            st->smoke->offY = 0;
            st->smoke->offZ = 0;
            st->smoke->r = 0xC0;
            st->smoke->g = 0xC0;
            st->smoke->b = 0x60;
            st->smoke->kind = 9;
            st->smoke->desc = &g_anSparkleGlowDesc;
          }
        } else {
          fx->size += 0x20;
          if (st->smoke->size >= 0x401) {
            st->smoke->size = 0x400;
          }
        }
      } else if (st->smoke != 0) {
        ClearWord(st->smoke);
        st->smoke = 0;
      }

      if (g_nLevelReadyFlag[0] <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          ApproxDist2D(&actor->posX, (int *)((char *)g_nLevelReadyFlag - 0x164)) >=
              0x2001) {
        st->timer = 0;
      }

      if (actor->subMode != 0x63) {
        if (st->gnorc != 0) {
          switch (actor->subMode) {
          case 0: {
            /* Reeling the gnorc in. */
            EnemyState *ge = st->gnorc->state;
            int high = actor->nextAnim & 1;
            int dist;
            int reach;
            int limit;

            CopyVector(t280, &st->gnorc->posX);

            if (high == 0) {
              t280[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              t280[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              t280[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              t280[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 22;
            }

            if (high != 0) {
              if (actor->nextFrame >= 7) {
                actor->subMode += 1;
              }

              {
                int tilt = actor->rotY;
                int back = -tilt;

                if ((unsigned int)((tilt - 3) & 0xFF) < 0xFB) {
                  if (back >= 3) {
                    back = 2;
                  }
                  if (back < -2) {
                    back = -2;
                  }
                  actor->rotY = tilt + back;
                }
              }
            }

            SubtractVector(t280, t280, &actor->posX);
            dist = VectorLength(t280, 1);

            if (high == 0 &&
                (TickCountdownByFrameDelta((void *)st, 4) != 0 ||
                 (st->timer < 0x50 &&
                  AbsAngleDelta8(
                      (actor->rotZ + (ge->vel[0] * 0x6E)) & 0xFF,
                      (((unsigned short)g_nCameraEulerYaw << 16 >> 20) + 0x80) &
                          0xFF) < 8))) {
              if (actor->nextAnim != actor->nextAnim + 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim + 1);
              }
            }

            reach = high != 0 ? 0x96 : 0x82;
            limit = reach + 5;
            if (limit < dist) {
              ScaleVector3ByRatio(t280, dist, limit);
            } else {
              ScaleVector3ByRatio(t280, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, t280);
            TurnActorYawTowardAngle(actor, ArcTan2(t280[0], t280[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(t280, 0), t280[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_nLevelReadyFlag[0] += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            actor->subMode += 1;
            DespawnActorRecord(st->gnorc);
            break;

          case 2:
            if (g_nActorAnimDone != 0) {
              if (actor->nextAnim != actor->nextAnim - 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim - 1);
              }
              st->gnorc = 0;
              actor->subMode = 0;
            }
            break;

          case 4: {
            /* Hauling an enemy up to the basket. */
            EnemyState *ge = st->gnorc->state;
            int dist;

            SubtractVector(t296, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(t296, 1);

            if (dist < 0x200) {
              ge->timer = 0;
              ge->lean = GetRandomU32() & 0xE;
              ge->roll = GetRandomU32() & 0xE;
              ge->spin = GetRandomU32() & 0xE;
              CopyVector((int *)ge, &st->gnorc->posX);
              st->gnorc->subMode = 3;

              if (st->gnorc->type == 14) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType14);
              } else if (st->gnorc->type == 15) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType15);
              } else {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_adwActorBurstColors[st->gnorc->type]);
              }
              st->gnorc = 0;
              do {
              } while (0);

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[0x2F]) == 0) {
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(t296, dist, lim);
              } else {
                ScaleVector3ByRatio(t296, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, t296);
              TurnActorYawTowardAngle(actor, ArcTan2(t296[0], t296[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(t296, 0), t296[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            t264[0] = -0x258;
            t264[1] = (GetRandomU32() & 0x310) - 0x188;
            t264[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = t264[0];
            st->drift[1] = t264[1];
            st->drift[2] = t264[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              t264[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              t264[1] = st->drift[1];
              t264[2] = st->drift[2];
              if (t264[2] < 0x64) {
                t264[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                t264[0] = st->drift[0] - 0x200;
              } else {
                t264[0] = st->drift[0];
              }
              t264[1] = st->drift[1];
              t264[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.u034, t264, t264);
            AddVector(t264, t264, &SPY.posX);
            at = &actor->posX;
            SubtractVector(t264, t264, at);
            RShiftVector3(t264, 2);
            AddVector(at, at, t264);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (t264[2] >= 0x21) {
              t264[2] = 0x20;
            }
            if (t264[2] < -0x20) {
              t264[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = t264[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, 0);
          }

          switch (SPY.u164) {
          case 1:
            if (actor->nextAnim != 0 && actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
            break;

          case 2:
            if (actor->nextAnim != 2 && actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
            break;

          case 3:
            if (actor->nextAnim != 4 && actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
            break;
          }
        }

        RefreshActorDerivedState(actor, 4);
      }

      break;
    }

    /* Types 152/153: sparking debris. Falls under gravity, spinning, and
     * pops when it runs out of life or reaches the floor. */
    case 152:
    case 153: {
      DebrisFallState *st = actor->state;

      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (--actor->subMode == 0 || actor->posZ < st->floor) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      AddVector(&actor->posX, &actor->posX, st->vel);
      st->vel[2] -= 0xC;
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];
      if (!(actor->subMode & 1)) {
        CopyVector(fx, &actor->posX);
        fx[3] = GetRandomU32() & 3;
        fx[4] = GetRandomU32() & 3;
        fx[5] = 0x14;
        col[0] = 0x80;
        col[1] = 0x70;
        col[2] = 0x40;
        g_pfnLevelEmitParticles(1, 0x11, fx, col);
      }
      break;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case 160: {
      L26Critter *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 1) {
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        {
          if (actor->flags & 0x10000) {
            st->speed = 0x8C;
          } else {
            st->speed = 0xFA;
          }
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;
      }

      switch (actor->mode) {
      case 0:
        UpdateActorWander(actor, st);
        break;

      case 1:
        if (st->speed >= 0x10) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x2BC, 5);
        }
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Ambient flyer (FlyState): circles its anchor and pops near Spyro. */
    case 173: {
      FlyState *st = actor->state;

      if (actor->subMode == 0) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
            int i;

            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            RefreshActorDerivedState(actor, 4);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, 0xC, actor, (int *)(0x8080 + (i << 24)));
            }

            g_nLevelEggCount = 1;
            actor->scale = 0x40;
            actor->cullRadius = 0;
            actor->updateDist = 0;
            actor->drawn = 0;
            actor->groundCode = 0;
            actor->visZone = 0xFF;
            actor->subMode = 2;
          }
        }

        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, 4);
          RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset4,
                        g_aRotBillboards[st->attach].pos);
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (handle >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }

        st->timer += g_nFrameStep;
      }

      break;
    }

    /* Type 174: the cage that carries Spyro (CageState). Shakes when hit;
     * once its flyer is freed, Spyro steps in and it lifts him up and away,
     * bursting when the ride ends. */
    case 174: {
      CageState *st = actor->state;
      Actor *fly = &g_pActorListBase[st->idx];

      if (actor->flags & 0x80000) {
        actor->subMode = 2;
        st->shake = 0xD2;
      }

      if (actor->subMode < 2) {
        if (st->shake != 0) {
          st->shake += g_nFrameStep;
          if (st->shake < 0x40) {
            int dx;
            int dy;

            actor->rotX = g_aShakeRotOffsets[st->shake >> 1].a + st->rotX;
            actor->rotY = g_aShakeRotOffsets[st->shake >> 1].b + st->rotY;
            dx = (signed char)g_aShakeRotOffsets[st->shake >> 1].a;
            dy = (signed char)g_aShakeRotOffsets[st->shake >> 1].b;
            if (dx < 0) {
              dx = -dx;
            }
            if (dy < 0) {
              dy = -dy;
            }
            dx += dy;
            actor->posZ = st->restZ + dx * 8;
          } else {
            st->shake = 0;
            actor->rotX = st->rotX;
            actor->rotY = st->rotY;
            actor->posZ = st->restZ;
          }
        } else if (actor->flags & 0xB0000) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      switch (actor->subMode) {
      case 0:
        if (g_nLevelEggCount == 0 || g_nLevelEggCount == 3) {
          g_nLevelEggCount = 0;
          actor->updateDist = sixteen;
          fly->updateDist = 0x40;
          fly->cullRadius = 0x18;
        } else {
          g_nLevelEggCount = 1;
          actor->subMode = 1;
          fly->scale = 0x40;
          fly->groundCode = 0;
          fly->visZone = 0xFF;
          fly->subMode = 2;
        }
        break;

      case 1: {
        int height;
        int drop;

        if (g_nLevelEggCount == 1 &&
            (SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
             SPY.state == 2) &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x600) {
          height = actor->posZ - actor->floorOffset;
          drop = height - SPY.posZ;
          if ((drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, 0)) < 0x30 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0)) < 0x30) {
            spy->controlFlags = 0x80002000;
            ResetSpyroLinearMotion();
            ZeroVector(&spy->headLook);
            actor->subMode = 2;
            if (st->shake > 0) {
              st->shake = 0;
              actor->rotX = st->rotX;
              actor->rotY = st->rotY;
              actor->posZ = st->restZ;
            }
            fly->rotX = 0;
            fly->rotY = 0;
            fly->rotZ = spy->bodyRotZ + 0x80;
          }
        }
        break;
      }

      case 2:
        if (g_dwPadPressed & 0x40) {
          st->shake = 0xD2;
        }
        g_dwSpyroRequestMask = 0x80002000;
        fly->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nGameplayBlocked = 1;

        if (st->shake < 0xC0) {
          t352[0] = 0x200;
          t352[1] = 0;
          t352[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, t352, t352);
          AddVector(t352, t352, &actor->posX);
          SubtractVector(t352, t352, &spy->posX);
          t352[2] -= 0x100;
          RShiftVector3(t352, 5);
          ScaleVector3Sat(t352, t352, st->shake);
          DivideVector3(t352, t352, 6);
          AddVector(t352, t352, &spy->posX);
          t352[2] += 0x100;
          t352[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&fly->posX, t352);
          g_nCameraScriptStickRequestY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

          if (st->shake < 0x40) {
            int rot;

            fly->rotY = -st->shake;
            rot = g_bSpyroPersistentEulerZ + 0x80;
            fly->rotZ = rot + st->shake * 4;
          } else if (st->shake < 0x80) {
            int rot;

            fly->rotY = 0xC0;
            rot = g_bSpyroPersistentEulerZ + 0x80;

            fly->rotZ = rot + (st->shake - 0x40) * 3;
          } else {
            int turn = AbsAngleDelta8(actor->rotZ,
                                     (unsigned char)(g_bSpyroPersistentEulerZ + 0x40)) *
                       (st->shake - 0x80);
            int rot = g_bSpyroPersistentEulerZ + 0x40;

            fly->rotZ = rot + turn / 64;
          }
        } else if (st->shake < 0xD0) {
          fly->rotZ = actor->rotZ;
          t368[0] = 0x200 - (st->shake - 0xC0) * 8;
          t368[1] = 0;
          t368[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, t368, t368);
          AddVector(&fly->posX, t368, &actor->posX);
        }

        if (st->shake >= 0xD0 ||
            !(SPY.state == 0 || SPY.state == 1 || SPY.state == 0x15 ||
              SPY.state == 2 || SPY.state == 3)) {
          int i;

          fly->cullRadius = 0;
          fly->updateDist = 0;
          fly->drawn = 0;
          g_nGameplayBlocked = 0;
          actor->soundRange = 0x20;
          PlayActorMeshVoice(actor, 0, 0);

          for (i = 0; i < 4; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }

          for (i = 0; i < 10; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }

          g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, (void *)0x18);
          g_pThiefEggDrop = SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          g_nLevelEggCount = 2;
          g_nCameraScriptStickRequestX = 0;
          g_nCameraScriptStickRequestY[0] = 0;
          g_nSpyroSwingEulerPitchTarget = 0;
        }

        RefreshActorDerivedState(fly, 4);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x5008080);
        }
        break;
      }

      st->handle = SelectActorLodBand(actor, st->handle);
      actor->flags = 0;
      continue;
    }

    /* Wooden chest (BreakState). */
    case 194: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->snapToGround != 0) {
        SnapActorToGround(actor);
      }
      if ((actor->flags & 0xB0000) == 0) {
        continue;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0xFF, actor);
        g_pfnLevelSpawnActor(0x100, actor);
      }
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x101, actor);
      }

      g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
      SpawnActorDeathDrop(actor, 3, 0, 0);
      MarkActorCommittedKill(actor);
      DespawnActorRecord(actor);
      continue;
    }

    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case 195: {
      WobbleState *st = actor->state;

      if (st->snapToGround != 0) {
        SnapActorToGround(actor);
      }

      if (st->phase != 0) {
        int phase = st->phase + g_nFrameStep;

        st->phase = phase;

        if (phase < 0x40) {
          int dx;
          int dy;

          actor->rotX = g_aShakeRotOffsets[st->phase >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[st->phase >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[st->phase >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[st->phase >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
        }
      }

      if (actor->flags & 0xA0000) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }

        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);

        if ((actor->flags & 0x10000) && st->phase == 0) {
          st->phase = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      actor->flags = 0;
      break;
    }

    /* Crystal dragon statue (DragonStatueState): shakes, then counts the rescue when Spyro is close. */
    case 250: {
      DragonStatueState *st = actor->state;
      int *at;

      if (actor->mode == 0) {
        if (st->partner != -1) {
          g_pActorListBase[st->partner].mode = 3;
        }
        actor->mode = 1;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->mode == 1) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x101) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, 1);
        } else if (st->phase >= 0xC0) {
          int dx;
          int dy;

          if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0]) == 0) {
            PlayActorMeshVoice(actor, 0, 0);
          }

          actor->rotX = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else if (st->phase >= 0xBC) {
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = 0xBC;
          StopSoundVoicesByOwner(actor, 1);
        } else if ((st->phase >> 1) == 0x10 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
        } else if ((st->phase >> 1) == 0x30 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
        } else if ((st->phase >> 1) == 0x50 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
        }

        at = &actor->posX;
        if (ApproxDist2D(at, &spy->posX) < 0x800) {
          SubtractVector(t384, at, &spy->posX);
          t384[2] = (t384[2] * 3) >> 2;

          if (VectorLength(t384, 1) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->facing;
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, facing);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;
              StubRewardHook(st->reward, 0);

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->unk18 == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            actor->mode = 2;
            actor->rotX = st->rotX;
            actor->rotY = st->rotY;
            actor->posZ = st->restZ;
            BeginGemPickupOverlay(actor);
            ZeroVector(&SPY.headLook);
            g_dwSpyroRequestMask = 0x80002147;

            if (g_nSpyroAirborneFrames != 0) {
              g_nSpyroRequestState = 6;
              g_anSpyroRequestVelocity.x = g_anSpyroMotionVec >> 8;
              g_nSpyroRequestVelocityY = g_nSpyroMotionY >> 8;
              if (g_nSpyroMotionZ > 0) {
              clear_c68:
                g_nSpyroRequestVelocityZ = 0;
              } else {
                g_nSpyroRequestVelocityZ = g_nSpyroMotionZ >> 6;
              }
            } else {
              int *look = &SPY.u208;

              g_nSpyroRequestState = 3;
              CopyVector(look, &SPY.u10C);
              RShiftVector3(look, 6);
              g_nSpyroRequestVelocityZ = 0;

              if (SPY.u208 != 0 || g_nSpyroRequestVelocityY != 0) {
                ScaleVector3ByRatio(look, VectorLength(look, 0), 0x60);
                goto clear_c68;
              }
            }
          }
        }

      } else {
        g_dwSpyroRequestMask = 0x80002000;
      }

      break;
    }

    /* Dragon fragment, updated out of line. */
    case 251:
      TickGemPickupBurstEntity(actor);
      break;

    /* Floating digits (DebrisState). */
    case 260:
    case 261:
    case 262:
    case 263:
    case 264:
    case 265:
    case 266:
    case 267:
    case 268:
    case 269: {
      DebrisState *st = actor->state;

      if (!(actor->cullRadius & 0x80) && (actor->cullRadius != 0)) {

        if (st->life >= 1) {

          actor->rotZ += 1;

          st->vel[2] -= 6;
          if (st->vel[2] < -0x80) {
            st->vel[2] = -0x80;
          }

          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          actor->posZ += st->vel[2];

          if (actor->posZ > 1023) {
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
              int *nrm = &g_anCollisionProbeVec;
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(nrm, 0x1000);

              dot = (st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionProbeVecY +
                     st->vel[2] * g_nCollisionProbeVecZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(nrm, 0x1000, -dot);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionProbeVecY);
                st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
              }
            }
            st->life--;
          } else {
            DespawnActorRecord(actor);
          }
        } else {
          g_pfnLevelEmitParticles(10, 0x47, &actor->posX, 0);
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Type 296: bobbing platform tied to a partner actor. */
    case 296: {
      L26Bob *st = actor->state;

      if ((actor->flags & 0x90000) && actor->mode == 0) {
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;
      }
      if (g_pActorListBase[st->partner].mode >= 0x80) {
        DespawnActorRecord(actor);
        continue;
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 3, 0, 0);
        if (st->big != 0) {
          actor->posZ = st->baseZ + ((g_anCosineLut[st->phase] + 0x1000) >> 1);
        } else {
          actor->posZ = st->baseZ + ((g_anCosineLut[st->phase] + 0x1000) >> 5);
        }
        if (st->phase >= 0xDD) {
          g_pActorListBase[st->partner].subMode = 1;
        } else {
          g_pActorListBase[st->partner].subMode = 0;
        }
        if (TickCountdownByFrameDelta(&st->phase, 4) != 0) {
          st->phase = 0xFF;
        }
        break;

      case 1:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 298: bouncer riding its partner. Thrown when its partner is
     * charged, and dropped when the partner goes. */
    case 298: {
      L26Rider *st = actor->state;

      TickActorFlameGlow(actor);
      if (actor->mode < 2) {
        if (actor->flags & 0x80000) {
          g_pActorListBase[st->partner].flags |= actor->flags;
          st->speed = (SPY.u118 >> 6) + 0x1C2;
          st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                                    actor->posY - SPY.posY, 0),
                                      g_bSpyroPersistentEulerZ, 0x20, 0x40);
          st->lift = 0x50;
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, 2);
        }
        if (g_pActorListBase[st->partner].mode != 0) {
          st->heading = actor->rotZ;
          st->speed = 0;
          st->lift = 0;
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, 2);
        }
        GetActorAttachPoint(&g_pActorListBase[st->partner], 0, &actor->posX);
        *(L26Rot *)&actor->rotX = *(L26Rot *)&g_pActorListBase[st->partner].rotX;
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0xC00 &&
            (unsigned int)(actor->posZ - SPY.posZ - 1) < 0xBFF &&
            actor->subMode != 0) {
          ENTER_POSE(actor, 1);
        }
        break;

      case 1:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 2:
        if (DriveActorTrackedMove(actor, &st->speed, st->heading, &st->lift, 0xC,
                          0x10) != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = 3;
          if (actor->nextAnim != 3) {
            actor->frameProgress = 0x10;
            actor->frameRate = 0x10;
            actor->nextAnim = 3;
            actor->nextFrame = 0;
            StubActorHook(actor);
          }
          continue;
        }
        break;

      case 3:
        if (DriveActorTrackedMove(actor, &st->speed, st->heading, &st->lift, 0xC,
                          0x10) != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->posZ < 0x4E20) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 5);
      break;
    }

    /* Camera trigger volume (TriggerState). */
    case 300: {
      TriggerState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (TriggerState *)actor->state;
      if (st->flags & 8) {
        if (g_pActorListBase[st->idx].type == 0xAD) {
          if (g_pActorListBase[st->idx].subMode == 0)
            break;
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1C01)
            break;
        } else if (g_pActorListBase[st->idx].type == 0xFA ||
                   g_pActorListBase[st->idx].type == 0x53) {
          if (g_pActorListBase[st->idx].mode < 0x80)
            break;
        } else {
          break;
        }
      }
      flags = st->flags;
      if (flags & 2) {
        if (st->timer != 0) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0)
            st->flags = (st->flags & ~2) | 4;
          break;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[54], actor, 8, &actor->sfxSlot);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, 6, (int *)actor, 0);
      SubtractVector(t384, &spy->posX, &actor->posX);
      distance = VectorLength(t384, 0);
      if (distance < st->radius && t384[2] >= -0x3FF && t384[2] < st->spanZ &&
          (SPY.state == 0x11 || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->controlFlags = 0x80000000;
        SPY.pScriptedActor = actor;
        SPY.damageFlags |= 0x800;
        SPY.u200 = actor->posZ + st->spanZ;
        CopyVector(&spy->contact, &actor->posX);
        if (st->flags & 1) {
          spyro->controlFlags |= 0x8000;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamMode2Front;
          SPY.pCamAnchor = &spy->posX;
          SPY.pCamTarget = &g_anCameraActiveTargetParams;
          spyro->controlFlags |= 0x200;
          g_anCameraActiveTargetParams.w[0] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
        }
        st->timer = 0x78;
        break;
      }
      if ((st->flags & ~1) == 4 && st->timer != 0 &&
          TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~4) | 2;
      }
      break;
    }

    /* Sparkling shard: 1 puff per frame. */
    case 309: {
      ShardState *st = actor->state;

      if (st->life != 0 && actor->drawn != 0) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];

        t400[0] = (GetRandomU32() & 0xFE) - 0x7F;
        t400[1] = (GetRandomU32() & 0xFE) - 0x7F;
        t400[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(t400, t400, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, t400, (int *)1);
        st->life -= 1;
      } else {
        DespawnActorRecord(actor);
        continue;
      }

      break;
    }

    /* Wood shard: bounces and throws three sparks a frame. */
    case 310:
    case 311: {
      ShardState *st = actor->state;
      register int i asm("$16");

      if (st->life > 0) {
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }

        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
          int *nrm = &g_anCollisionProbeVec;
          int along;

          SetVector3Magnitude(nrm, 0x1000);
          along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                  (st->vel[2] * g_nCollisionProbeVecZ);
          i = along >> 11;

          if (i < 0) {
            ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - i);
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        i = 0;
        {
          actor->rotY += st->spin[1];
          actor->rotZ += st->spin[2];

          do {
            i += 1;
            t416[0] = (GetRandomU32() & 0xFE) - 0x7F;
            t416[1] = (GetRandomU32() & 0xFE) - 0x7F;
            t416[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(t416, t416, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, t416, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Type 312: cracking boulder. Shakes and smokes when hit by a charge or
     * flame, then bursts into three rings of debris and knocks the camera
     * back. */
    case 312: {
      L16Boulder *st = actor->state;

      switch (actor->subMode) {
      case 0:
        *(int *)&actor->tint0 = D_8006E52C[actor->dropKind];
        actor->subMode = 1;
        break;

      case 1:
        if (actor->flags & 0x40000) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & 0x10000) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          st->timer = 0;
        } else {
          goto hit;
        }
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;

      hit:
        if (actor->flags & 0xA0000) {
          actor->subMode = 3;
          actor->cullRadius = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;

      case 2: {
        int t = st->timer + g_nFrameStep;
        int i;
        int n;

        st->timer = t;
        if (t < 0x78) {
        if ((actor->flags & 0x40000) && t < 0x5A) {
          st->timer = 0x5A;
        }
        for (i = 0; i < 6; i++) {
          ps[0] = g_aSmokeJetEmitters[i >> 1].pos[0];
          ps[1] = g_aSmokeJetEmitters[i >> 1].pos[1];
          ps[2] = g_aSmokeJetEmitters[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, ps, ps);
          AddVector(ps, ps, &actor->posX);
          ps[3] = g_aSmokeJetEmitters[i >> 1].vel[0];
          ps[4] = g_aSmokeJetEmitters[i >> 1].vel[1];
          ps[5] = g_aSmokeJetEmitters[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, ps + 3, ps + 3);
          ps[3] += (GetRandomU32() & 0xF) - 8;
          ps[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, 0x4A, ps, ps + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          ps[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          ps[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          ps[2] = (GetRandomU32() & 0x7F) + 0xC0;
          ps[3] = ps[0] >> 4;
          ps[4] = ps[4] >> 4;
          ps[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, ps, ps);
          AddVector(ps, ps, &actor->posX);
          RotateVectorByMatrix(actor->mtx, ps + 3, ps + 3);
          t456[0] = (GetRandomU32() & 7) + 0x10;
          t456[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, 0x10, ps, t456);
          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 0, 0);
          }
        }
        if (st->timer >= 0x39) {
          actor->rotX = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b)) *
                  6;
        }
        } else {
          actor->subMode = 3;
          actor->cullRadius = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;
      }

      case 3:
        if (st->timer == 0) {
          int i;
          Actor *c;
          DebrisFallState *cs;

          PlayActorMeshVoice(actor, 1, 0);
          SpawnActorDeathDrop(actor, 6, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            p = g_pfnLevelSpawnActor(0x97, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(0x99, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 1) + 0x12;
            CopyVector(&c->posX, &actor->posX);
            c->posX += (g_anCosineLut[i * 16] >> 4) + (GetRandomU32() & 0x1F) - 0xF;
            c->posY += (g_anSineLut[i * 16] >> 4) + (GetRandomU32() & 0x1F) - 0xF;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = g_anCosineLut[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = g_anSineLut[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[2] = (GetRandomU32() & 0x3F) + 0x30;
            c->rotX = GetRandomU32();
            c->rotY = GetRandomU32();
            c->rotZ = GetRandomU32();
            cs->spin[0] = GetRandomU32() & 0xF;
            cs->spin[1] = GetRandomU32() & 0xF;
            cs->spin[2] = GetRandomU32() & 0xF;
            cs->floor = actor->posZ;
          }
          for (i = 0; i < 12; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(0x98, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 3) + 0x19;
            CopyVector(&c->posX, &actor->posX);
            c->posX += (g_anCosineLut[(i << 8) / 12] >> 4) + (GetRandomU32() & 0x3F) - 0x1F;
            c->posY += (g_anSineLut[(i << 8) / 12] >> 4) + (GetRandomU32() & 0x3F) - 0x1F;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = (short)(g_anCosineLut[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = (short)(g_anSineLut[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[2] = (GetRandomU32() & 0x7F) + 0x60;
            c->rotX = GetRandomU32();
            c->rotY = GetRandomU32();
            c->rotZ = GetRandomU32();
            cs->spin[0] = GetRandomU32() & 0xF;
            cs->spin[1] = GetRandomU32() & 0xF;
            cs->spin[2] = GetRandomU32() & 0xF;
            cs->floor = actor->posZ;
          }
        }
        if (st->pushed == 0 &&
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, 0x86) != 0) {
          st->pushed = 1;
          g_anSpyroRequestVelocity.x = SPY.posX - actor->posX;
          g_nSpyroRequestVelocityZ = 0;
          g_nSpyroRequestVelocityY = SPY.posY - actor->posY;
          if (g_anSpyroRequestVelocity.x != 0 || g_nSpyroRequestVelocityY != 0) {
            int len = VectorLength(&g_anSpyroRequestVelocity.x, 1);

            ScaleVector3ByRatio(&g_anSpyroRequestVelocity.x, len,
                          (0x1000 - ApproxDist2D(&actor->posX, &spy->posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = 0x46;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, 0x50000, actor, 0);
        if (++st->timer >= 8) {
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }

    /* Respawn warden (WardenState): puts its group back once Spyro has left. */
    case 323: {
      WardenState *st = actor->state;

      CopyVector(&actor->posX, &SPY.posX);

      if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        Actor *scan;

        st->timer = st->period;

        for (scan = g_pActorListBase; scan < g_pActorPoolDynBase; scan++) {
          if (scan->type == st->group && scan->mode >= 0x80 &&
              ApproxDist2D(&SPY.posX, scan->state) >= 0x6001) {
            CopyVector(&scan->posX, scan->state);
            InsertActorIntoSpatialGrid(scan);
            scan->flags = 0;
            scan->mode = 0;
            scan->killGroup &= 0x7F;
            ANIM_RESET(scan, 0);
          }
        }
      }

      break;
    }

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case 329: {
      L16Pod *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = D_8006E52C[actor->dropKind];
          actor->subMode = 1;
          actor->updateDist = 0x10;
        }
        if (actor->flags & 0xB0000) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = 0x40;
          st->prize = SpawnActorDeathDrop(actor, 5, 0, 0);
          st->prize->subMode = 4;
          st->prize->cullRadius = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, 0x40, &actor->posX, (void *)(st->prize->type - 0x53));
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          SpawnActorRotBillboard(actor, g_anRotBillboardOffsetsRand[GetRandomU32() & 3]);
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
        break;

      case 1: {
        int i;

        st->prize->cullRadius = 0x18;
        CopyVector(&st->prize->posX, &actor->posX);
        st->prize->posZ += LookupSine((st->timer << 11) / 90) / 3 + 0x1A4;
        st->prize->rotZ += g_nFrameStep * 2;
        if (st->timer < 0x40) {
          int dx;
          int dy;

          actor->rotX = g_aShakeRotOffsets[st->timer >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[st->timer >> 1].b + st->rotY;
          dx = (signed char)g_aShakeRotOffsets[st->timer >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[st->timer >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        }
        if (st->timer < 0x20 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          ZeroVector(t456);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, t456);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == 0x2C || g_nSpyroState == 0x18 ||
            (g_nSpyroState == 0x14 && (g_nSpyroStateFlags & 0x40))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & 0x80000) {
          st->prize->subMode = 2;
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        st->timer += g_nFrameStep;
        if (st->timer >= 0x56) {
          int j;

          for (j = 0; j < 6; j++) {
            CopyVector(t472, &st->prize->posX);
            t472[0] += g_anCosineLut[j * 43] >> 6;
            t472[1] += g_anSineLut[j * 43] >> 6;
            t472[2] += 0x28;
            col2[0] = g_anCosineLut[j * 43] >> 7;
            col2[1] = g_anSineLut[j * 43] >> 7;
            col2[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, t472, col2);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= 0x7F;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, 1, 0);
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Whirlwind pad: a short spin-up before the ride, then a cooldown. */
    case 331:
      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1: {
        int height;
        int drop;

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x380) {
          break;
        }

        height = actor->posZ - actor->floorOffset;
        drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = 2;
          actor->anim = 1;
          actor->subMode = 0;
          continue;
        }
        break;
      }

      case 2: {
        int d = g_nFrameStep;
        int tick = actor->subMode + d;

        actor->subMode = tick;
        if ((unsigned char)tick < 0x30) {
          break;
        }
        actor->mode = 0;
        actor->anim = 0;
        continue;
      }
      }
      break;

    /* Water bubbles and splash. */
    case 405:
    case 477:
      /* Removed as soon as the animation finishes. */
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case 390: {
      L26Spinner *st = actor->state;
      int pitch = *(unsigned short *)(g_pLevelSampleBankEntries +
                                      g_apActorMeshTable[actor->type]->m_Sounds[1] *
                                          20 +
                                      0xA);
      int cur = g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0];
      int i;

      if ((actor->flags & 0x90000) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, 8,
                      &actor->sfxSlot);
        if (actor->sfxSlot != 0x7F) {
          g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
        StopSoundVoicesByOwner(actor, 1);
      }
      if (actor->subMode == 0) {
        st->lid = g_pfnLevelSpawnActor(0x188, actor);
        st->lid->cullRadius = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = 1;
        continue;
      }
      if (st->timer == 0 || --st->timer == 0) {
        if (actor->flags & 0x10000) {
          st->timer = 0x10;
          st->speed += 0x64;
        }
      }
      st->handle = SelectActorLodBand(actor, st->handle);
      st->speed -= 1;
      if (pitch < cur && actor->sfxSlot != 0x7F) {
        g_anSpuVoiceRowPitchStep[actor->sfxSlot & 0x7F][0] = -4;
      }
      if (st->speed < 0x18) {
        st->speed = 0x18;
        actor->updateDist = 0x10;
      } else {
        actor->updateDist = 0x40;
      }
      st->angle += (st->speed * g_nFrameStep) >> 1;
      st->lid->rotZ = st->angle >> 4;
      if (st->speed > 0xE0 || (actor->flags & 0x80000)) {
        st->lid->subMode = 1;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      }
      actor->flags = 0;
      break;
    }

    /* Type 392: homing spark. Launched at the nearest enemy in front of
     * Spyro (or away from him when there is none), and bursts on what it
     * hits. */
    case 392: {
      L26Spark *st = actor->state;
      int i;

      if (actor->subMode == 1) {
        Actor *best = 0;
        int bestAng = 0x180;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= 0x80 || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(t456, &a->posX, &actor->posX);
          d = ABS2(t456[0]) + ABS2(t456[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(t456, 0) - 0x1000) <= 0x2000 &&
              (unsigned int)(t456[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(t456[0], t456[1], 1));
            if (d < bestAng) {
              bestAng = d;
              best = a;
            }
          }
        }
        if (best != 0) {
          SubtractVector(st->vel, &best->posX, &actor->posX);
          st->vel[2] += 0x200;
          RShiftVector3(st->vel, 7);
        } else {
          SubtractVector(st->vel, &actor->posX, &SPY.posX);
          st->vel[2] = 0;
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, 0), 0x50);
        }
        st->vel[2] += 0xA0;
        st->timer = 0;
        actor->subMode = 2;
        actor->cullRadius = 0x20;
        actor->updateDist = 0xFF;
        actor->rotX = 0xA;
        actor->rotY = 0;
        continue;
      }
      if (actor->subMode != 2) {
        continue;
      }
      st->timer += g_nFrameStep;
      actor->rotZ += g_nFrameStep * 8;
      if (st->vel[2] >= -0x9F) {
        st->vel[2] -= (g_nFrameStep * 5) >> 1;
      }
      for (i = 0; i < g_nFrameStep; i++) {
        AddVector(&actor->posX, &actor->posX, st->vel);
        if (actor->posZ < 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += 0xB4;
        if (st->timer >= 5 &&
            (CollideSphereWithWorldAndActors(&actor->posX, 0xB4, 0xB4) != 0 ||
             DispatchActorContactAtSphere(&actor->posX, 0x100, 0, 0x30000, actor, 0) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, 0x400, 0, 0x30000, actor, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
          PlayActorMeshVoice(actor, 0, 0);
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0xB4;
      }
      continue;
    }

    /* Life chest (LiftState). */
    case 421: {
      LiftState *st = actor->state;
      Actor *rider;
      int i;
      int a;
      int b;

      if (st->rideMode == 1) {
        SnapActorToGround(actor);
        EncodeCachedVecToActorDirCode(actor);
      }

      if (actor->mode == 0) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x1E1) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->mode = 1;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = 1;

          st->passenger = g_pfnLevelSpawnActor(0x1A6, actor);
          st->passenger->rotX = actor->rotX;
          st->passenger->rotY = actor->rotY;
          st->passenger->rotZ = actor->rotZ;
          st->passenger->frameRate = 0x10;
          st->passenger->updateDist = 0;
          st->passenger->cullRadius = 0;
          st->passenger->drawn = 0;
          st->passenger->unk47 = 5;

          PlayActorMeshVoice(actor, 0, 0);
        } else if (st->phase >= 0x1A0) {
          actor->rotX = g_aShakeRotOffsets[LIFT_STEP(st)].a + st->savedRotX;
          actor->rotY = g_aShakeRotOffsets[LIFT_STEP(st)].b + st->savedRotY;
          a = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].a;
          b = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->savedPosZ + (a + b) * 6;

          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 1, 0);
          }
        } else if (st->phase >= 0x19C) {
          /* Just before the shake: remember the rest pose. */
          st->savedRotX = actor->rotX;
          st->savedRotY = actor->rotY;
          st->savedPosZ = actor->posZ;
          if (st->rideMode == 1) {
            st->phase = 0;
          }
        }
      } else {
        if (actor->anim == 1) {
          st->phase += g_nFrameStep;
          st->passenger->cullRadius = 0x10;
          st->passenger->updateDist = 0xFF;
        }

        if (st->phase >= 0xE6) {
          DespawnActorRecord(st->passenger);
          st->phase = GetRandomU32() & 0xFF;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->mode = 0;
          actor->nextAnim = 0;
          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 3, 0);
          }
        }
      }

      if ((actor->flags & 0xB0000) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, 5, 0, 0);
      if (st->rideMode == 1) {
        ((unsigned char *)rider->state)[0xE] = st->rideMode;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 4, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A7, actor);
        g_pfnLevelSpawnActor(0x1A8, actor);
      }

      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A9, actor);
      }

      for (i = 0; i < 8; i++) {
        t472[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        t472[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        t472[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, t472);
      }

      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x18);

      if (actor->mode == 1) {
        DespawnActorRecord(st->passenger);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[2], actor, 8,
                    (void *)(long)actor->sfxSlot);
      DespawnActorRecord(actor);
      continue;
    }

    /* Orbiting satellite (SatState): rides a spinning parent at a fixed
     * radius, or just spins in place behind a non-spinner. */
    case 76:
    case 426:
    case 427:
    case 428:
    case 429:
    case 430:
    case 431:
    case 432:
    case 433:
    case 434:
    case 435:
    case 436:
    case 437:
    case 438:
    case 439:
    case 440:
    case 441:
    case 442:
    case 443:
    case 444:
    case 445:
    case 446:
    case 447:
    case 448:
    case 449:
    case 450:
    case 451: {
      SatState *st = actor->state;

      if (st->parent->mode == 0 || st->parent->subMode != actor->mode) {
        DespawnActorRecord(actor);
        continue;
      }

      if (st->parent->type == 9) {
        if (ApproxDist2D(&st->parent->posX, &spy->posX) < 0x400) {
          unsigned char was = actor->subMode;
          int prev = g_anCosineLut[was];
          unsigned char now = was + 8;

          actor->subMode = now;
          actor->rotZ =
              actor->rotZ - ((prev * 3) >> 9) + ((g_anCosineLut[now] * 3) >> 9);
        } else {
          int bearing;
          int arm;
          int idx;
          short *swing;

          actor->subMode += 8;
          bearing = ArcTan2(spy->posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          t472[0] = (g_anCosineLut[bearing] * 3) >> 4;
          t472[1] = (g_anSineLut[bearing] * 3) >> 4;
          t472[2] = 0;
          t504[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          t504[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          t504[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(t520, t504, st->radius - 1);
          LShiftVector3(t504, 1);
          arm = st->radius - 1;
          t520[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          t536[0] = t472[0] * g_anCosineLut[arm * 2];
          t536[1] = t472[1] * g_anCosineLut[arm * 2];
          t536[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(t504, t504, st->phase);
          SubtractVector(t520, t520, t504);
          swing = &g_anCosineLut[bearing];
          actor->posX = t472[0] * *swing;
          actor->posY = t472[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, t536);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, t472);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, t520);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }
    
}
  }
}
