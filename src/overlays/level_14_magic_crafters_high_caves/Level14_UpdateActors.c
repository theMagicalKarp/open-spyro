/* Level14_UpdateActors -- per-frame actor update for level 14 (High
 * Caves). 0x8007B64C, 53,744 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 4,
 * 9, 12, 13, 15, 16, 25 and 26); the level-14 classes are 109 (a flying
 * carrier that follows Spyro and can be ridden), 227 (the superflame giver: it
 * flies in front of Spyro under a scripted camera and grants the superflame),
 * 301 (a bolt caster), 302 (a world-chunk operator), 303 and 304 (a spawner
 * and its runner) and 305 (a path-walking critter that flees).
 *
 * Load-bearing source forms:
 *   - No base/spy carrier: `spy` is `&SPY`, so Spyro reads are absolute and
 *     the preheader hoists only the small constants.
 *   - The frame is laid out in first-use order with shared slots (svF for
 *     cases 14 and 109, s496 for 312/329/392, s512 for 329/76).
 *   - Struct-versus-scalar spellings decide memory dependences (cookbook
 *     A302): SPY.state, SPY.bodyYaw/frameMotionLength, SPY.bodyRotZ, g_anCollisionContactPoint[2].
 *   - Case 109's turn-and-walk idiom reuses `h` and `d`, and its node
 *     searches use `&p->nodes[i].x` (loop.c makes the 8 + 16*i giv) with
 *     `st->cur->cur = i` (one fewer st ref decides st vs which, s5/s4).
 *   - Case 109's camera hold block takes `int *w1 = &g_anCameraActiveTargetParams.w[1]` so
 *     cse derives &g_anCameraActiveTargetParams as w1 - 4; cases 227/109 read the yaw as
 *     SPY.bodyRotZ (a struct MEM, ordered against the st->cam stores).
 *
 *   - Case 227 reads the camera's second sine through a pointer local
 *     (`*(nv = &g_anSineLut[..])`); with the plain array read sched2 puts the
 *     split 0x80000200 constant's lui ahead of the g_anCosineLut reload.
 */

#include "actor.h"
#include "model.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"
#include "level_actor_states.h"
#include "level_update_actors_macros.h"
#include "gamestate.h"
#include "match_asm.h"
#include "pad_buttons.h"
#include "spyro_state.h"
#include "camera.h"
#include "level_sfx.h"
#include "emit_record.h"

/* ==== Per-class state blocks ====
 *
 * Every actor class keeps its private state behind actor->state. The blocks
 * below sit next to the globals and callees their case needs.
 */


/* Callees used by the collectible and fairy behaviours. */

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */


/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (next);                                                         \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = 0;                                                              \
  OnActorAnimQueued(a);

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
    OnActorAnimQueued(a);                                                          \
  }                                                                            \
  continue

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */


/* ==== Globals shared with level 4 ==== */


/* Case 328 -- floor-snapped marker. */


/* ==== Camera and Spyro globals used by the camera arms ==== */

#define sixteen 0x10


/* ==== Level 14 classes ==== */
extern int g_anCollisionContactPoint[2]; /* wall probe hit x, y */

/* Type 303: runner spawner. Every so often (`timer`, 0xB4) spawns one type
 * 304 runner and hands it `value`; killable. */
typedef struct RunnerSpawnerState {
  unsigned char *link; /* 0x00 byte +1 cleared when the spawn fires */
  int flags;           /* 0x04 bit 0: spawned */
  int wait;            /* 0x08 */
  int timer;           /* 0x0C */
  int value;           /* 0x10 copied to the spawn's state +0x14 */
} RunnerSpawnerState;

/* Type 304: waypoint runner. Walks `path` until it reaches Spyro, the path
 * end, or its `target` actor (which it flags), then clears its spawner's
 * flags bit 0 and leaves. */
typedef struct WaypointRunnerState {
  void *path;   /* 0x00 */
  Actor *link;  /* 0x04 its state's flags bit 0 is cleared on release */
  int hold;     /* 0x08 */
  int pad0C[2]; /* 0x0C */
  short target; /* 0x14 pool index, < 0 = none */
} WaypointRunnerState;

/* Type 302: chunk operator. Starts its world chunk's animation when Spyro
 * comes near (0x2000) or goes far (0x3000); with `riding` set it follows the
 * moving floor triangle under it. */
typedef struct ChunkOperatorState {
  int chunkAnim;     /* 0x00 world chunk it animates (GetWorldChunkAnimState) */
  int riding;    /* 0x04 */
  char *anchor;  /* 0x08 +0x18: distance reference */
  int spark;     /* 0x0C pool index of the spark target */
  int node;      /* 0x10 path node, -1 = none */
  int pts[3][3]; /* 0x14 node triangle */
} ChunkOperatorState;

/* Type 301: bolt caster. Wakes when Spyro is in range and fires type 295
 * bolts at him. */
typedef struct BoltCasterState {
  int target; /* 0x00 pool index of the actor it fires at, -1 = none */
  int flags;  /* 0x04 bit 0: woken, bit 1: Spyro in range */
  int timer;  /* 0x08 */
  int count;  /* 0x0C bolts fired */
} BoltCasterState;


/* Type 305: a critter that walks one of three paths and flees Spyro. */
typedef struct FleeingWalkerState {
  PathData *cur;   /* 0x00 path being walked */
  PathData *pathA; /* 0x04 */
  PathData *pathB; /* 0x08 */
  PathData *pathC; /* 0x0C */
  int mode;        /* 0x10 bits 0-1 path, 2 fleeing, 3 Spyro close, 4 hold,
                      5 restart, 6 pause, 7 reverse */
  int vel[3];      /* 0x14 */
  int timer;       /* 0x20 */
  int heading;     /* 0x24 */
  int pad28;       /* 0x28 */
  int speed;       /* 0x2C */
  int range;       /* 0x30 */
} FleeingWalkerState;

#define PATH_END(p) (&(p)->nodes[(p)->count - 1].x)
#define PATH_CUR(p) (&(p)->nodes[(p)->cur].x)
#define NODE_PT(p) ((int *)((char *)(p) + ((p)->cur << 4) + 8))

typedef struct Cam5 {
  int w[5];
} Cam5;

/* Type 109: flying carrier. Follows Spyro and can be ridden. */
typedef struct CarrierState {
  int mode;        /* 0x00 bits 0-1 riding (1/2 = side), 2 to dest, 3 path0,
                      4 follow Spyro, 5 return along cur, 6 back down cur,
                      7 camera hold; 0x2C = carried */
  PathData *pathA; /* 0x04 */
  PathData *pathB; /* 0x08 */
  PathData *pathC; /* 0x0C */
  PathData *path0; /* 0x10 */
  PathData *cur;   /* 0x14 */
  int dest[3];     /* 0x18 */
  int rider;       /* 0x24 pool index */
  int bob;         /* 0x28 */
  int vz;          /* 0x2C */
} CarrierState;

/* Turn toward `h`; walk at `d`, slowed by the remaining turn. */
#define STEER(a, h, d, k1, k2, sh)                                             \
  if (TurnActorYawTowardAngle(a, h, k1, k2, 1) != 0) {                                   \
    MoveActorAlongHeading(a, d, 0, 0x200, 0x20);                                       \
  } else {                                                                     \
    h = ((h) - (a)->rotZ) & 0xFF;                                             \
    if (h > 0x80) {                                                            \
      h -= 0x100;                                                              \
    }                                                                          \
    d -= ABS2(h) << (sh);                                                      \
    if (d > 0) {                                                               \
      MoveActorAlongHeading(a, d, 0, 0x200, 0x20);                                     \
    }                                                                          \
  }

/* ==== The function ==== */
#define one 1
#define spy (&SPY)
#define oneq 1
void Level14_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  int dt;

  /* Scratch vectors, in first-use order. */
  int svA[3];
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int svF[3];
  int acc[3][3];
  int svG[3];
  int svH[3];
  int svI[3];
  int svJ1[3];
  int svJ[3];
  int x272[3];
  int x288[3];
  int fx[6];
  int col[3];
  int svK[3];
  int svL[3];
  int v376[3];
  int t392[3][3];
  int svM[3];
  int svT[3];
  int ps[6];
  int s496[3];
  int mat2[3][3];
  int s512[3];
  int s528[3];
  int vecB[3];
  int vecC[3];
  int vecD[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, 0);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apActiveActorList;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & ACTOR_ANIM_DONE;
    g_nActorAnimNewFrame = actor->animFlags & ACTOR_ANIM_NEW_FRAME;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Level-exit whirlwind (ExitWhirlState): shows its name as the camera nears,
     * lifts Spyro, and once he is high above it starts the return to the hub. */
    case ACTOR_TYPE_LEVEL_EXIT: {
      ExitWhirlState *st = actor->state;
      int dist = ApproxDist2D(&actor->posX, &CAM.posX);
      int i;

      if (actor->mode != LEVELEXIT_FAR ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == LEVELEXIT_FAR) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = oneq;
      } else {
        actor->mode = LEVELEXIT_FAR;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL_SMEAR, &actor->posX, (void *)tick);
        }
      }

      if (dist < 0x400) {
        char *base = (char *)&g_nSpyroWorldPosZ;
        int top = *(int *)base;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == SPYRO_ST_LIFT || top + 0xC00 < z + depth) {
            if (g_abHudCounterDisplayState == 2) {
              RefreshHudLevelGemCounter();
            }
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_TARGET_CAMERA);
            g_pScriptedCameraActor = actor;
            g_dwSpyroTriggerEventFlags |= SPYRO_EV_LIFT;
            g_nSpyroRequestTargetZ = ACTOR_RAW(actor, int, posZ) + 0x4000;
            CopyVector(SPY_HELD_FIELD(base, int, firstContactPoint), &actor->posX);

            if (depth < drop) {
              int yaw = g_nCameraEulerPitch & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -0x200) {
                g_abLevelExited[g_nLevelIndex] = 1;
                g_nCdStreamState = 0;
                g_nEnteredPortalIdx = -1;
                g_nPortalTransitionActive = one;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = GS_LEVEL_LOAD;
                g_nGamePaused = 1;
                g_nReturnPortalLevelId = g_nActiveLevelId;
                g_nCurrentLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(0);
                g_nSpyroFlightMode = 0;
                g_nSpyroSuperflameTimer = 0;
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
    case ACTOR_TYPE_FOLLOWER: {
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
            (parent->mode >= ACTOR_MODE_DORMANT || (parent->dropKind & 0x80))) {
          Actor *scan;
          int self;

          parent->dropKind = actor->dropKind;
          DespawnActorRecord(actor);

          if (parent->mode < ACTOR_MODE_DORMANT) {
            DespawnActorRecord(parent);
          }

          /* Take the parent's whole record over (gcc's block_move_loop). */
          *actor = *parent;
          InsertActorIntoSpatialGrid(actor);

          actor->drawn = 1;
          self = actor - g_pActorListBase;
          parent = g_pActorListBase;

          for (; parent < g_pActorPoolDynBase; parent++) {
            if (parent != actor && parent->mode < ACTOR_MODE_DORMANT && parent->type == ACTOR_TYPE_FOLLOWER) {
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
              SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              SpawnActorDeathDrop(actor, DROP_AT_POINT, 0, st->pos);
            } else {
              SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
            }
            MarkActorCommittedKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->voiceRow);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= ACTOR_MODE_DORMANT) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < ACTOR_MODE_DORMANT && parent->type == ACTOR_TYPE_FOLLOWER &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->voiceRow);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case ACTOR_TYPE_BUTTERFLY: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.sparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pSparxActor == 0) {
            if (SPY.sparxHealth >= SPARX_GONE) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, 0, 0);

              SPY.sparxHealth = SPARX_GREEN;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            ChaserState *cs = (ChaserState *)g_pSparxActor->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(SPY.bodyRotZ,
                                ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0)) < 0) {
                st->dir = 1;
              } else {
                st->dir = -1;
              }

              g_pSparxActor->subMode = SPARX_SUB_CHASE;
              cs->owner = actor;
              cs->timer = RandomIntInRange(150, 210);

              actor->mode = BUTTERFLY_CIRCLE;
            }
          }
        }
      }

      if (actor->cullRadius & 0x80)
        break;

      switch (actor->mode) {
      case BUTTERFLY_FLUTTER: {
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
      case BUTTERFLY_CIRCLE: {
        SteerActorAroundPoint(actor, &SPY.posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pSparxActor == 0 || g_pSparxActor->subMode == SPARX_SUB_PARKED) {
          actor->mode = BUTTERFLY_FLUTTER;
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

    /* Egg thief (ThiefState). */
    case ACTOR_TYPE_EGG_THIEF: {
      ThiefState *st;

      EBB_BREAK();
      st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != 10) {
        st->knock = 0x96;
        if ((actor->flags & ACTOR_HIT_CHARGE) != 0) {
          st->knock = 0xE1;
        }
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        if (actor->dropKind == 0xFF) {
          actor->dropKind = 0xF;
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorCommittedKill(actor);
        }
        actor->mode = 10;
        continue;
      }

      if (actor->dropKind != 0xFF && st->egg == 0 && actor->mode != 10) {
        st->egg = g_pfnLevelSpawnActor(ACTOR_TYPE_DRAGON_EGG, actor);
        st->egg->otBias = 0;
        *(Actor **)st->egg->state = actor;
      }

      if (TickCountdownByFrameDelta(&st->voice, 4) != 0 &&
          (actor->mode == 1 || actor->mode == 0x28)) {
        int k;

        st->voice = (int)GetRandomU32() % 3 + 4;
        k = (int)GetRandomU32() % 5;
        while (k == st->lastVoice) {
          k = (int)GetRandomU32() % 5;
        }
        PlayActorMeshVoice(actor, k, 0);
        st->lastVoice = k;
      }

      if (actor->nextAnim == 4) {
        if (actor->nextFrame == 2 && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 0xC && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case 0:
        actor->otBias = 2;
        if (st->mode == 0) {
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
            st->landed = 0;
            ANIM_GO(actor, 1);
            actor->mode = 1;
            continue;
          }
        } else if (st->mode == 1) {
          int spd = 0x154;

          spd -= ApproxDist2D(&actor->posX, &spy->posX) >> 5;
          if (spd < 0x8C) {
            spd = 0x8C;
          }
          if (spd > 0xF0) {
            spd = 0xF0;
          }
          if (actor->subMode == 0) {
            int home = ArcTan2(st->path->nodes[0].x - actor->posX,
                                     st->path->nodes[0].y - actor->posY, 0);

            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) < 0x1000) {
              TurnActorYawTowardAngle(actor,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - SPY.posY, 0),
                            8, 0, 0);
              MoveActorAlongHeading(actor, spd, 0, 0, 5);
            } else {
              int lim = 10;
              int r;
              int a = ArcTan2(
                  st->path->nodes[st->path->cur].x - spy->posX,
                  st->path->nodes[st->path->cur].y - SPY.posY, 0);
              int b = ArcTan2(actor->posX - st->path->nodes[0].x,
                                    actor->posY - st->path->nodes[0].y, 0);

              if (actor->nextAnim == 4) {
                lim = 0x28;
              }
              if (AbsAngleDelta8(a, b) > lim &&
                  ApproxDist2D(&actor->posX, &spy->posX) < 0x3000) {
                r = SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0x1800, a, 5,
                                  spd, 0xE, 0x14, 0xFF, 0xFF, 0, 0, 4);
              }
              if (r > 0x5A) {
                st->stuck++;
              } else {
                st->stuck = 0;
              }
              if (r < 0x1E) {
                TrackEntityYawTowardSpyro(actor, 6, 0, 0);
                ANIM_GO(actor, 4);
              } else {
                ANIM_GO(actor, 1);
              }
              if (st->stuck >= 0x1E &&
                  AbsAngleDelta8(home, ArcTan2(spy->posX - actor->posX,
                                                    spy->posY - actor->posY,
                                                    0)) > 0x20) {
                actor->subMode = 1;
              }
            }
          } else {
            int a = ArcTan2(st->path->nodes[0].x - spy->posX,
                                  st->path->nodes[0].y - SPY.posY, 0);
            int b = ArcTan2(st->path->nodes[0].x - actor->posX,
                                  st->path->nodes[0].y - actor->posY, 0);

            TurnActorYawTowardAngle(actor, a, 8, 0, 0);
            MoveActorAlongHeading(actor, spd, 0, 0, 5);
            if (AbsAngleDelta8(a, b) > 0x50) {
              actor->subMode = 0;
            }
            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x1800) {
              actor->subMode = 0;
            }
          }
        } else if (st->mode == 2) {
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
            actor->mode = 0x19;
            continue;
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1C00) {
            actor->mode = 0x14;
            continue;
          }
        } else if (st->mode == 3) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
              ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x708 &&
              (st->needLos == 0 || CheckSpyroLineOfSightTo(&actor->posX) != 0)) {
            st->node = FindFarthestPathPointFromSpyro(st->path);
            st->speed = 0;
            if (st->node - st->path->reversed != st->path->cur) {
              st->turned = 0;
              if (ApproxDist2D(&actor->posX,
                                &st->path->nodes[st->path->cur].x) < 0x100) {
                st->path->cur = (st->path->cur + st->path->reversed +
                                 st->path->count) %
                                st->path->count;
              }
              actor->mode = 0x1E;
              continue;
            }
            ANIM_GO(actor, 4);
            TrackEntityYawTowardSpyro(actor, 6, 0, 0);
            break;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == 4) {
          if (IsPointInRotatedBox(&SPY.posX, st->area) != 0) {
            actor->mode = 0x28;
            continue;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == 5) {
          if (st->dist == 0) {
            st->dist = ApproxDist2D(&actor->posX,
                                     &st->path->nodes[st->path->cur].x);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int d = ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                  SPY.posY - st->path->nodes[st->path->cur].y, 0);
            int b = ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                  actor->posY - st->path->nodes[st->path->cur].y,
                                  0) -
                    0x80;

            d = (d - b) & 0xFF;

            if (d > 0x80) {
              d -= 0x100;
            }
            if (ABS(d) > 0x30) {
              actor->mode = 0x32;
              continue;
            }
          }
        } else if (st->mode == 6) {
          actor->mode = 0x3C;
          continue;
        }
        break;

      case 1: {
        int spd = 0x154;

        spd -= ApproxDist2D(&actor->posX, &SPY.posX) >> 5;
        if (spd < 0x46) {
          spd = 0x46;
        }
        if (spd > 0xDC) {
          spd = 0xDC;
        }
        if (PivotActorAlongPath(actor, st->path, 0x73A, spd, 0xC8, 0xA, 0x80,
                          actor->group, 0) == 0x104) {
          st->vz = 0x12C;
          actor->mode = 2;
          continue;
        }
        break;
      }

      case 2:
        actor->otBias = 0;
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x) <
            0x100) {
          actor->mode = 3;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        break;

      case 3: {
        int g = ProbeActorGroundHeight(actor);

        ANIM_GO(actor, 4);
        if (st->landed == 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->landed = 1;
            break;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        } else {
          int k;
          int h;

          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[4].x) <
              ApproxDist2D(&spy->posX, &st->path->nodes[5].x)) {
            k = 4;
          } else {
            k = 5;
          }
          h = ArcTan2(st->path->nodes[k].x - actor->posX,
                            st->path->nodes[k].y - actor->posY, 0);
          if (ApproxDist2D(&actor->posX, &st->path->nodes[k].x) > 0x100) {
            if (TurnActorYawTowardAngle(actor, h, 8, 0x14, 1) != 0) {
              MoveActorAlongHeading(actor, 0x96, 0, 0, 5);
            }
          } else if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x800 ||
                     ApproxDist2D(&spy->posX, &st->path->nodes[7].x) < 0x800) {
            st->vz = 0x96;
            actor->mode = 4;
            continue;
          }
        }
        break;
      }

      case 4: {
        int g;

        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[0].x - actor->posX,
                                    st->path->nodes[0].y - actor->posY, 0),
                      6, 0, 0);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x100) {
          MoveActorAlongHeading(actor, 0x82, 0, 0, 1);
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        g = ProbeActorGroundHeight(actor);
        if (st->vz < 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->path->cur = 0;
            ANIM_GO(actor, 0);
            actor->mode = 0;
            continue;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        }
        break;
      }

      case 10:
        if (st->egg != 0) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          if (actor->nextAnim == 3 && actor->nextFrame >= 5) {
            st->egg->mode = EGG_RELEASED;
            st->egg->otBias = 4;
            st->egg = 0;
          }
        }
        if (st->knock != 0) {
          MoveActorAlongAngle(actor, st->heading, st->knock, 0, 0x12C, 5);
          st->knock -= 0x10;
          if (st->knock < 0) {
            st->knock = 0;
          }
        }
        ANIM_GO(actor, 3);
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        BurstActorWithSound(actor, 0x18);
        DespawnActorRecord(actor);
        continue;

      case 0x14:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xF0, 0, 8, sixteen, 0xFF,
                          5) == 0x103) {
          actor->mode = 0x15;
          continue;
        }
        break;

      case 0x15:
        if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
          actor->mode = 0x19;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          actor->mode = 0x16;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        ANIM_GO(actor, 4);
        break;

      case 0x16:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xDA, 0, 0xA, 0x30, 0xFF,
                          5) == 0x101) {
          actor->mode = 0x17;
          continue;
        }
        break;

      case 0x17: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (d < 0x2000 && SPY.posZ - actor->posZ >= -0xC7) {
          st->vz = 0x96;
          st->landed = 0;
          actor->mode = 0x18;
          continue;
        }
        break;
      }

      case 0x18: {
        int sp = 1;

        if (st->landed != 0) {
          sp = 5;
        }
        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[2].x - actor->posX,
                                        st->path->nodes[2].y - actor->posY, 0),
                          8, 0x1E, 1) == 0) {
          break;
        }
        MoveActorAlongHeading(actor, 0xFA, 0, 0, sp);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[2].x) < 0x100) {
          st->path->cur = 3;
          actor->mode = 0x15;
          continue;
        }
        if (st->landed == 0) {
          int g = ProbeActorGroundHeightRaised(actor, 0x1800);

          actor->posZ += st->vz;
          st->vz -= 0x14;
          if (st->vz < 0 && actor->posZ - g < ABS(st->vz)) {
            st->landed = 1;
          }
        }
        break;
      }

      case 0x19:
        CopyVector(&actor->posX, &st->path->nodes[0].x);
        st->path->cur = 1;
        SnapActorToGround(actor);
        actor->mode = 0x17;
        continue;

      case 0x1E: {
        int top = 0xCD;
        int next;
        int dCur;
        int dNext;
        int r;

        ANIM_GO(actor, 1);
        if (st->path->nodes[st->path->cur].speed > 0) {
          top = st->path->nodes[st->path->cur].speed;
        }
        if (st->speed < top) {
          st->speed += 0x1E;
          if (top < st->speed) {
            st->speed = top;
          }
        } else if (top < st->speed) {
          st->speed -= 0x1E;
          if (st->speed < top) {
            st->speed = top;
          }
        }
        next = (st->path->cur + st->path->reversed + st->path->count) %
               st->path->count;
        dCur = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);
        dNext = ApproxDist2D(&actor->posX, &st->path->nodes[next].x);
        if (dCur > 0x190 && dNext > 0x190) {
          int aCur = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                   st->path->nodes[st->path->cur].y - actor->posY,
                                   0);
          int aNext = ArcTan2(st->path->nodes[next].x - actor->posX,
                                    st->path->nodes[next].y - actor->posY, 0);
          int aSpy =
              ArcTan2(spy->posX - actor->posX, spy->posY - actor->posY, 0);
          int dA = AbsAngleDelta8(aCur, aSpy);
          int dB = AbsAngleDelta8(aNext, aSpy);

          if (st->turned == 0 && ABS2(dA - dB) < 0x28) {
            st->turned = 1;
            st->path->reversed = -st->path->reversed;
          } else if (dA < dB && ABS2(dA - dB) >= 0x1D && dB > 0x3C &&
                     AbsAngleDelta8(actor->rotZ,
                                   ArcTan2(spy->posX - actor->posX,
                                                 spy->posY - actor->posY,
                                                 0)) < 0x3C) {
            st->path->reversed = -st->path->reversed;
            st->path->cur = next;
          }
        }
        r = PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x1E, 0xFF, 5);
        if (r & 0x100) {
          if ((r & ~0x100) ==
              (st->node - st->path->reversed) % st->path->count) {
            actor->mode = 0;
            continue;
          }
          st->node = FindFarthestPathPointFromSpyro(st->path);
        }
        break;
      }

      case 0x28:
        ANIM_GO(actor, 1);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        st->speed = 0xB4;
        if (st->path->nodes[st->path->cur].speed > 0) {
          st->speed = st->path->nodes[st->path->cur].speed;
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          5) == 0x10B) {
          st->vz = 0x96;
          actor->mode = 0x29;
          continue;
        }
        break;

      case 0x29: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        }
        if (d < 0x100 && actor->posZ == g) {
          if (++st->path->cur == 0xD) {
            actor->mode = 0x2A;
            continue;
          }
          st->vz = 0x96;
          break;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case 0x2A:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        {
          char *base = (char *)&g_nSpyroWorldPosZ;

          if (*(int *)base - st->path->nodes[10].z < -0x5DC &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1800) {
            st->vz = 0x1A4;
            st->path->cur = 9;
            actor->mode = 0x2B;
            continue;
          }
        }
        break;

      case 0x2B: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        ANIM_GO(actor, 1);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    0),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, 1);
        }
        if (d < 0x100 && actor->posZ == g) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 8;
          actor->mode = 0x2C;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case 0x2C:
        ANIM_GO(actor, 1);
        st->speed = 0xC8;
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          5) == st->path->count + 0xFF) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 1;
          actor->mode = 0;
          continue;
        }
        break;

      case 0x32:
        ANIM_GO(actor, 1);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          int a = (ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                 SPY.posY - st->path->nodes[st->path->cur].y,
                                 0) +
                   0x80) &
                  0xFF;

          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist, a, 8,
                        st->speed, 0xF, 0x40, 0xFF, 0xFF, 0, 0, 0);
          a = (a - ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                 actor->posY - st->path->nodes[st->path->cur].y,
                                 0)) &
              0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS(a) < 0x20) {
            ENTER_POSE(actor, 0);
          }
          break;
        }
        ENTER_POSE(actor, 0);

      case 0x3C:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        break;
      }

      if (st->egg != 0) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, &st->egg->posX);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Recovered dragon egg (EggRiseState): spirals up, flies to Spyro, then rolls the HUD egg counter. */
    case ACTOR_TYPE_DRAGON_EGG: {
      EggRiseState *st = actor->state;
      unsigned int phase = actor->mode;

      if (phase != 0 && phase < 5) {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (g_nSpyroState != SPYRO_ST_IDLE && g_nSpyroState != SPYRO_ST_CHARGE_BONK) {
          actor->mode = EGG_COLLECTED;
        }
      }

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case EGG_RELEASED:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode += 1;
        break;

      case EGG_SPIRAL_UP: {
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

          CopyVector(svA, &SPY.posX);
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
        if (g_dwPadPressed & PAD_CROSS) {
          actor->mode = EGG_COLLECTED;
        }
        break;
      }

      case EGG_TO_SPYRO: {
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
          CopyVector(svD, &SPY.posX);
          svD[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[2] += 0x300;
          VectorToSVectorShr2(st->centre, svD);
          st->radius = 0x200;
          st->angle = g_bSpyroPersistentEulerZ - 0x40;
          st->phase = 0;
          actor->mode += 1;
        }
        if (g_dwPadPressed & PAD_CROSS) {
          actor->mode = EGG_COLLECTED;
        }
        break;
      }

      case EGG_SPIRAL_DOWN: {
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
        if (g_dwPadPressed & PAD_CROSS) {
          actor->mode = EGG_COLLECTED;
        }
        break;
      }

      case EGG_COLLECTED: {
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->idx);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Path walker (PathState): waits for Spyro, then paces its path, turning
     * back at either end and choosing the node that leads away from him. */
    case ACTOR_TYPE_PATH_WALKER: {
      PathState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 2) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;
      }

      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1800) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
            ENTER_POSE4(actor, 1);
          }
        }
        break;

      case 1:
        if (ApproxDist2D(&actor->posX,
                          &st->path->nodes[st->path->cur].x) < 0x400) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1801) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop >= 0x801 : (SPY.posZ - height) >= 0x801) {
              goto pose0;
            }
            goto walk;
          }
        pose0:
          ENTER_POSE4(actor, 0);
        walk:
          if (st->path->cur == 0 && st->loops == 0) {
            st->path->cur = 1;
          } else if (st->path->cur == st->path->count - 1 && st->loops == 0) {
            st->path->cur--;
          } else {
            int spyroAngle;
            int nextNode;
            int previousNode;
            int nextAngle;
            int previousAngle;

            spyroAngle = ArcTan2(spy->posX - actor->posX,
                                       spy->posY - actor->posY, 0);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;
            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, 0);
            previousAngle =
                ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                              st->path->nodes[previousNode].y - actor->posY, 0);
            if (AbsAngleDelta8(spyroAngle, nextAngle) >
                AbsAngleDelta8(spyroAngle, previousAngle)) {
              st->path->cur = nextNode;
            } else {
              st->path->cur = previousNode;
            }
          }
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[st->path->cur].x -
                                            actor->posX,
                                        st->path->nodes[st->path->cur].y -
                                            actor->posY,
                                        0),
                          8, 0x20, 1) != 0) {
          MoveActorAlongHeading(actor, 0x60, 0x200, 0x200, 0x27);
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1: {
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
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svD);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (int *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Falling chest fragments (TumbleState). */
    case ACTOR_TYPE_SEED_POD_SHARD + 2:
    case ACTOR_TYPE_BOULDER_DEBRIS:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 2: {
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

    /* Collectibles: life statue, life orb, gems (PickupState). */
    case ACTOR_TYPE_EXTRA_LIFE:
    case ACTOR_TYPE_SPARX_FOOD:
    case ACTOR_TYPE_GEM_1:
    case ACTOR_TYPE_GEM_2:
    case ACTOR_TYPE_GEM_5:
    case ACTOR_TYPE_GEM_10:
    case ACTOR_TYPE_GEM_25: {
      PickupState *st = (PickupState *)actor->state;

      int dist = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

        if (st->anim != 255) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset5, g_aRotBillboards[st->anim].pos);
          } else {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset3, g_aRotBillboards[st->anim].pos);
          }

          AddVector(g_aRotBillboards[st->anim].pos, g_aRotBillboards[st->anim].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->anim].life < 5) {
            st->anim = 255;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);
            if (handle >= 0 && dist < 0x4000) {
              st->anim = handle;
            }

            st->timer = (GetRandomU32() & 0x1F) + 33;
          }
        }
      }

      if (!actor->drawn && dist > 0x2000 && actor->subMode == COLLECT_REST && st->sub == 0) {
        break;
      }

      switch (actor->subMode) {
      case COLLECT_SETTLE: {
        int *nrm = &g_anCollisionProbeVec;

        CopyVector(svA, &actor->posX);
        svA[2] += 0x400;
        FindGroundHeightBelow(svA, 0x10000);
        st->pitch = -ArcTan2_8bit(
            IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
            g_nCollisionProbeVecY);
        st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);

        if (st->pitch != 0 || st->yaw != 0) {
          actor->rotZ = 0;
        }

        actor->subMode = COLLECT_REST;
        break;
      }
      case COLLECT_REST: {
        if (st->sub != 0) {

          if (st->sub == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              int *nrm = &g_anCollisionProbeVec;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                  g_nCollisionProbeVecY);
              st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
            }

          } else if (st->sub == 2) {

            UnpackWorldCollisionTri(st->groundTri, acc[0]);

            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);

            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              colIndex = g_nLastCollisionHitChunkId;
              st->sub = 2;
              st->groundTri = colIndex;
            }
          }
        }

        if (dist < 1434) {
          int zDist = (SPY.posZ - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0) {

                int res = CheckLineOfSightSegment(&g_pSparxActor->posX, &actor->posX);

                if (res) {
                  ChaserState *cs = (ChaserState *)g_pSparxActor->state;

                  if (cs->owner == 0) {
                    if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                       g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0])) {
                      g_nSfxOverrideFlags = 1;
                      g_nSfxOverrideVolR = 0x2000;
                      g_nSfxOverrideVolL = 0x2000;
                      PlayActorMeshVoice(g_pSparxActor, 0, 0);
                    }

                    g_pSparxActor->subMode = SPARX_SUB_FETCH;
                    cs->owner = actor;
                  }
                }
              }
            }
          }
        }

        break;
      }
      case COLLECT_BOUNCE: {
        int speed;

        ScaleVector3Sat(svG, st->vel, g_nFrameStep);
        RShiftVector3(svG, 1);

        speed = VectorLength(svG, VEC_LEN_XYZ);

        if (220 < speed) {
          ScaleVector3ByRatio(svG, speed, 220);
        }

        AddVector(svG, &actor->posX, svG);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (svG[2] < 0) {
          if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          svG[2] += 240;

          if (CollideSphereWithWorldAndActors(svG, 240, 240)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(svH, &g_anCollisionProbeVec);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = FindGroundHeightBelow(svG, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, VEC_LEN_XY));

              if ((svG[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = COLLECT_REST;
                CopyVector(&actor->posX, svG);
                actor->posZ = groundHeight;

                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->yaw = -ArcTan2_8bit(svH[2], svH[0]);

                  if (st->pitch != 0 || st->yaw != 0) {
                    actor->rotZ = 0;
                  }
                }

                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->count++;
              }
            }

            if (st->count != 0) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolL = 0x3CCC >> (3 - st->count);
              g_nSfxOverrideVolR = 0x3CCC >> (3 - st->count);

              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);

              if (ReflectVectorOffNormal(st->vel, svH, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            svG[2] -= 240;
            CopyVector(&actor->posX, svG);
            FindGroundHeightBelow(svG, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }
        }

        if (dist < 1434) {
          int zDist = (((int *)&SPY.posX)[2] - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0 && st->count < 3) {

                ChaserState *cs = (ChaserState *)g_pSparxActor->state;

                if (cs->owner == 0) {
                  if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                     g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0])) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pSparxActor, 0, 0);
                  }

                  g_pSparxActor->subMode = SPARX_SUB_FETCH;
                  cs->owner = actor;
                }
              }
            }
          }
        }

        break;
      }
      case COLLECT_HOMING: {
        if (st->timer >= 32) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(svI, &SPY.posX, st->vel);
          RShiftVector3(svI, 5);

          if (VectorLength(svI, VEC_LEN_XYZ) > 480) {
            CopyVector(&actor->posX, &SPY.posX);
            st->timer = 32;
          } else {
            ScaleVector3Sat(svI, svI, st->timer);
            AddVector(&actor->posX, st->vel, svI);
            actor->posZ += g_anSineLut[st->timer * 4] / 12;
          }
        }

        actor->rotX = actor->rotX - 7 + st->pitch;
        actor->rotY = actor->rotY - 7 + st->yaw;
        actor->rotZ = actor->rotZ - 7 + st->roll;
        break;
      }

      case COLLECT_INERT: {
        continue;
      }
      }
      if (actor->subMode < COLLECT_HOMING) {
        if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
          actor->rotX = (g_anCosineLut[st->roll] >> 9) + st->pitch;
          actor->rotY = (g_anSineLut[st->roll] >> 9) + st->yaw;
          st->roll += g_nFrameStep << 1;
        } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
          actor->rotZ += 8;
          actor->rotY -= 6;
        } else {
          actor->rotX = (g_anCosineLut[st->roll] >> 7) + st->pitch;
          actor->rotY = (g_anSineLut[st->roll] >> 7) + st->yaw;
          st->roll += g_nFrameStep << 1;
        }
      }
      if ((st->timer > 32 && dist < 512 && (-384 < (SPY.posZ - actor->posZ)) &&
           ((SPY.posZ - actor->posZ) < 512)) ||
          (actor->subMode == COLLECT_HOMING && st->timer > 64)) {

        if (g_pSparxActor != 0) {
          ChaserState *cs = (ChaserState *)g_pSparxActor->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, (void *)8);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);

          if (g_nExtraLives < 99) {
            g_nExtraLives++;
          }

          CommitActorKillGems(0, actor);
          DespawnActorRecord(actor);
          continue;
        } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          g_nSparxFoodCount++;

          DespawnActorRecord(actor);
        } else {
          CollectPickupActor(actor);
          DespawnActorRecord(actor);
          continue;
        }
      }

      break;
    }

    case ACTOR_TYPE_CARRIER: {
      CarrierState *st = actor->state;
      int d;
      int h;

      switch (st->mode & 3) {
      case 0:
        if (st->mode & 0x80) {
          if (g_nSpyroAirborneFrames != 0) {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
            if (g_nSpyroState == SPYRO_ST_08) {
              g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroMoveTargetYaw) & 0xFFF;
            } else {
              g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
            }
            {
              int *w1 = &g_anCameraActiveTargetParams.w[1];

              g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
              *w1 = g_anCameraParamMode2Front.w[1];
            }
            g_nCameraActiveTargetDistance = g_anCameraParamMode2Front.w[2];
            g_nCameraActiveTargetRoll = g_anCameraParamMode2Front.w[3];
            g_nCameraActiveTargetPitchBias = g_anCameraParamMode2Front.w[4];
            g_nCameraActiveTargetYawBias = g_anCameraParamMode2Front.w[5];
          } else {
            st->mode &= ~0x80;
          }
        }
        if (st->mode & 0x2C) {
          int *cf = &g_dwSpyroRequestMask;

          *cf = 0x80000327;
          SubtractVector(svF, &actor->posX, &SPY.posX);
          svF[2] -= 0x80;
          RShiftVector3(svF, 2);
          d = VectorLength(svF, VEC_LEN_XYZ);
          if (d > 0x200) {
            ScaleVector3ByRatio(svF, d, 0x200);
          }
          AddVector(cf + 1, svF, &SPY.posX);
          h = (actor->rotZ - SPY.bodyRotZ) & 0xFF;
          if (h > 0x80) {
            h -= 0x100;
          }
          if (h < -4) {
            h = -4;
          }
          if (h > 4) {
            h = 4;
          }
          g_bSpyroRequestYaw = SPY.bodyRotZ + h;
          g_nSpyroRequestState = SPYRO_ST_LIFT;
          g_pSpyroRequestCameraAnchor = &SPY.posX;
          g_pScriptedCameraActor = 0;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
          g_anCameraActiveTargetParams.w[1] = g_anCameraParamMode2Front.w[1];
          g_anCameraActiveTargetParams.w[2] = g_anCameraParamMode2Front.w[2];
          g_anCameraActiveTargetParams.w[3] = g_anCameraParamMode2Front.w[3];
          g_anCameraActiveTargetParams.w[4] = g_anCameraParamMode2Front.w[4];
          g_anCameraActiveTargetParams.w[5] = g_anCameraParamMode2Front.w[5];
        } else if (st->mode & 0x10) {
          int *cf = &g_dwSpyroRequestMask;

          *cf = 0x80000327;
          CopyVector(cf + 1, &SPY.posX);
          h = (actor->rotZ - SPY.bodyRotZ) & 0xFF;
          if (h > 0x80) {
            h -= 0x100;
          }
          if (h < -3) {
            h = -3;
          }
          if (h > 3) {
            h = 3;
          }
          g_bSpyroRequestYaw = SPY.bodyRotZ + h;
          g_nSpyroRequestState = SPYRO_ST_LIFT;
          g_pSpyroRequestCameraAnchor = &SPY.posX;
          g_pScriptedCameraActor = 0;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_anCameraActiveTargetParams.w[0] = (g_anCameraParamMode2Front.w[0] - g_nSpyroBodyYaw) & 0xFFF;
          g_anCameraActiveTargetParams.w[1] = g_anCameraParamMode2Front.w[1];
          g_anCameraActiveTargetParams.w[2] = g_anCameraParamMode2Front.w[2];
          g_anCameraActiveTargetParams.w[3] = g_anCameraParamMode2Front.w[3];
          g_anCameraActiveTargetParams.w[4] = g_anCameraParamMode2Front.w[4];
          g_anCameraActiveTargetParams.w[5] = g_anCameraParamMode2Front.w[5];
        }

        if (st->mode & 4) {
          d = ApproxDist2D(&actor->posX, st->dest);
          if (d < 0x200) {
            st->mode = (st->mode & ~4) | 8;
            (st->cur = st->path0)->cur = 0;
            st->vz = 0;
            continue;
          }
          h = ArcTan2(st->dest[0] - actor->posX,
                            st->dest[1] - actor->posY, 0);
          d >>= 1;
          if (d > 0x140) {
            d = 0x140;
          }
          STEER(actor, h, d, 4, 0x20, 4);
          d = st->dest[2] - actor->posZ;
          if (d != 0) {
            d >>= 1;
            if (d < -0x40) {
              d = -0x40;
            }
            if (d > 0x40) {
              d = 0x40;
            }
            actor->posZ += d;
          }
          continue;
        }

        if (st->mode & 8) {
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 2;
          if (d > 0x60) {
            d = 0x60;
          }
          if (d < -0x60) {
            d = -0x60;
          }
          d = (d - st->vz) >> 1;
          if (d < -0x10) {
            d = -0x10;
          }
          if (d > 0x10) {
            d = 0x10;
          }
          st->vz += d;
          actor->posZ += st->vz;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200 &&
              ++st->cur->cur >= st->cur->count) {
            st->cur->cur = st->cur->count - 2;
            st->mode = (st->mode & ~8) | 0xC0;
            EBB_BREAK();
            g_nSpyroRequestState = SPYRO_ST_08;
            g_nSpyroMoveTargetYaw = 0x290;
          }
          continue;
        }

        if (st->mode & 0x10) {
          int done = 1;

          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, SFX_MODE_3D, &actor->voiceRow);
          if (actor->drawn == 0) {
            CopyVector(&actor->posX, &SPY.posX);
            actor->posZ += 0x400;
          } else {
            d = ApproxDist2D(&actor->posX, &SPY.posX);
            if (d > 0x80) {
              d >>= 1;
              if (d > 0x140) {
                d = 0x140;
              }
              h = ArcTan2(SPY.posX - actor->posX,
                                SPY.posY - actor->posY, 0);
              if (TurnActorYawTowardAngle(actor, h, 0x10, 0x30, 1) != 0) {
                MoveActorAlongHeading(actor, d, 0, 0, 0);
              } else {
                h = (h - actor->rotZ) & 0xFF;
                if (h > 0x80) {
                  h -= 0x100;
                }
                d -= ABS2(h) << 4;
                if (d > 0) {
                  MoveActorAlongHeading(actor, d, 0, 0, 0);
                }
              }
              done = 0;
            } else {
              d = SPY.posZ - actor->posZ + 0x100;

              if (ABS2(d) > 0x80) {
                if (d < -0x80) {
                  d = -0x80;
                }
                if (d > 0x80) {
                  d = 0x80;
                }
                actor->posZ += d;
                done = 0;
              }
            }
          }
          if (done) {
            int i;
            int which;

            st->mode &= ~0x10;
            d = ApproxDist2D(&actor->posX, st->dest);
            which = 0;
            for (i = 0; i < st->pathA->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathA->nodes[i].x);

              if (e < d) {
                d = e;
                which = 1;
                st->pathA->cur = i;
              }
            }
            for (i = 0; i < st->pathB->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathB->nodes[i].x);

              if (e < d) {
                d = e;
                which = 2;
                st->pathB->cur = i;
              }
            }
            for (i = 0; i < st->pathC->count; i++) {
              int e = ApproxDist2D(&actor->posX, &st->pathC->nodes[i].x);

              if (e < d) {
                d = e;
                which = 3;
                st->pathC->cur = i;
              }
            }
            switch (which) {
            case 0:
              st->mode |= 4;
              break;
            case 1:
              st->mode |= 0x20;
              st->cur = st->pathA;
              break;
            case 2:
              st->mode |= 0x20;
              st->cur = st->pathB;
              break;
            case 3:
              st->mode |= 0x20;
              st->cur = st->pathC;
              break;
            }
          }
          continue;
        }

        if (st->mode & 0x20) {
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 1;
          if (d > 0x40) {
            d = 0x40;
          }
          if (d < -0x40) {
            d = -0x40;
          }
          actor->posZ += d;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200 &&
              ++st->cur->cur >= st->cur->count) {
            st->cur->cur = 0;
            st->mode = (st->mode & ~0x20) | 4;
          }
          continue;
        }

        if (st->mode & 0x40) {
          if (st->cur->cur > st->cur->count) {
            d = ApproxDist2D(&actor->posX, st->dest);
            if (d > 0x140) {
              d = 0x140;
            }
            h = ArcTan2(st->dest[0] - actor->posX,
                              st->dest[1] - actor->posY, 0);
            STEER(actor, h, d, 8, 0x30, 4);
            d = (st->dest[2] - actor->posZ) >> 1;
            if (ABS2(d) < 0x80) {
              st->cur->cur = 0;
              st->mode &= ~0x40;
            }
            if (d > 0x80) {
              d = 0x80;
            }
            if (d < -0x80) {
              d = -0x80;
            }
            actor->posZ += d;
            continue;
          }
          d = 0x140;
          h = ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                            NODE_PT(st->cur)[1] - actor->posY, 0);
          if (TurnActorYawTowardAngle(actor, h, 8, 0x30, 1) != 0) {
            MoveActorAlongHeading(actor, 0x140, 0, 0x200, 0x20);
          } else {
            h = (h - actor->rotZ) & 0xFF;
            if (h > 0x80) {
              h -= 0x100;
            }
            d -= ABS2(h) << 4;
            if (d > 0) {
              MoveActorAlongHeading(actor, d, 0, 0x200, 0x20);
            }
          }
          d = (NODE_PT(st->cur)[2] - actor->posZ) >> 1;
          if (d > 0x40) {
            d = 0x40;
          }
          if (d < -0x40) {
            d = -0x40;
          }
          actor->posZ += d;
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x200) {
            st->cur->cur--;
          }
          continue;
        }

        {
          int i;

          d = ApproxDist2D(&SPY.posX, st->dest);
          st->cur = 0;
          for (i = 0; i < st->pathA->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathA->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathA;
              st->cur->cur = i;
            }
          }
          for (i = 0; i < st->pathB->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathB->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathB;
              st->cur->cur = i;
            }
          }
          for (i = 0; i < st->pathC->count; i++) {
            int e = ApproxDist2D(&SPY.posX, &st->pathC->nodes[i].x);

            if (e < d) {
              d = e;
              st->cur = st->pathC;
              st->cur->cur = i;
            }
          }
          if (d >= 0x3000) {
            continue;
          }
          if (st->cur != 0) {
            CopyVector(svF, PATH_CUR(st->cur));
          } else {
            CopyVector(svF, st->dest);
          }
          d = ApproxDist2D(&actor->posX, svF);
          if (d > 0x100) {
            d >>= 1;
            if (d > 0x140) {
              d = 0x140;
            }
            h = ArcTan2(svF[0] - actor->posX, svF[1] - actor->posY,
                              0);
            STEER(actor, h, d, 0x20, 0x30, 4);
          } else {
            StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
            TrackEntityYawTowardSpyro(actor, 0x10, 0x18, 1);
          }
          d = (SPY.posZ - actor->posZ - 0x400) >> 1;
          if (d < 0) {
            if (d < -0x80) {
              d = -0x80;
            }
            actor->posZ += d;
          } else {
            d = st->dest[2] - actor->posZ;
            if (d > 0x40) {
              d = 0x40;
            }
            actor->posZ += d;
          }
          if (SPY.posZ < 0x9358) {
            st->mode |= 0x10;
          }
          continue;
        }

      case 1:
      case 2: {
        if (*(int *)g_pActorListBase[st->rider].state & 0x2C) {
          int side = -0x40;

          if ((st->mode & 3) == 1) {
            side = 0x40;
          }
          CopyVector(svF, &SPY.posX);
          if (actor->mode != st->mode) {
            actor->mode = st->mode & 3;
            if (actor->nextAnim != st->mode) {
              ANIM_ADVANCE(actor, st->mode & 3);
            }
            continue;
          }
          h = (side << 4) + g_nSpyroBodyYaw;
          svF[0] += LookupCosine(h) >> 4;
          svF[1] += LookupSine(h) >> 4;
          CopyVector(&actor->posX, svF);
          TurnActorYawTowardAngle(actor, g_nSpyroBodyYaw >> 4, 8, 0x20, 1);
          st->bob = (st->bob + g_nFrameStep * 2) & 0xFF;
          EncodeCachedVecToActorDirCode(actor);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
          continue;
        } else {
          int side = -0x50;

          if ((st->mode & 3) == 1) {
            side = 0x50;
          }
          CopyVector(svF, &g_pActorListBase[st->rider].posX);
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          h = ((side + g_pActorListBase[st->rider].rotZ) & 0xFF) << 4;
          svF[2] -= 0x180;
          svF[0] += LookupCosine(h) >> 2;
          svF[1] += LookupSine(h) >> 2;
          d = ApproxDist2D(svF, &actor->posX);
          SubtractVector(svF, svF, &actor->posX);
          if (d > 0x20) {
            h = ArcTan2(svF[0], svF[1], 0);
            d >>= 1;
            if (d > 0x190) {
              d = 0x190;
            }
            STEER(actor, h, d, 8, 0x20, 5);
            st->bob = g_pActorListBase[st->rider].rotZ;
          } else {
            TurnActorYawTowardAngle(actor, g_pActorListBase[st->rider].rotZ, 8, 0x20, 1);
          }
          if (svF[2] < -0x80) {
            svF[2] = -0x80;
          }
          if (svF[2] > 0x80) {
            svF[2] = 0x80;
          }
          actor->posZ += svF[2];
          continue;
        }
      }
      }
      break;
    }

    /* Save-prompt host (SaveHostState). */
    case ACTOR_TYPE_SAVE_HOST: {
      SaveHostState *st = (SaveHostState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->armer];
      Actor *tgtB = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY: {
        actor->collision = 0;
        if (ApproxDist2D(&tgtB->posX, &SPY.posX) > 2560) {
          actor->mode = SAVEHOST_WAIT_ARMED;
        }
        break;
      }

      case SAVEHOST_WAIT_ARMED: {
        if (tgtA->mode >= 0x80u) {
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 16;
          st->baseZ = actor->posZ;
        }
        break;
      }

      case SAVEHOST_HIDDEN: {
        if (st->timer != 0) {
          st->timer--;
        } else {
          actor->collision = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 2048) {
              if (st->prompted == 0 || tgtB->mode == 1) {
                st->prompted = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->cullRadius = 0x20;
                actor->mode = SAVEHOST_POP_IN;
                actor->scale = 0x60;

                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (void *)0xC);

                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(&actor->posX, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= ACTOR_GROUND_VALID;
              }
            }
          }
        }
        break;
      }

      case SAVEHOST_POP_IN: {
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->mode = SAVEHOST_HOVER;
          st->timer = 0;
          st->wanderTimer = 30;
        }
        break;
      }

      case SAVEHOST_HOVER: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 1024 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > 0xC00)) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          st->wanderTimer--;
          if (st->wanderTimer == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + 0x140;
            char finalAngle = (ArcTan2(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, 0) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->wanderTo, &tgtB->posX);

            actor->mode = SAVEHOST_WANDER;

            st->wanderTo[0] -= (LookupCosine(finalAngle * 0x10) * baseDistance) >> 12;
            st->wanderTo[1] -= (LookupSine(finalAngle * 0x10) * baseDistance) >> 12;

            st->faceYaw = ArcTan2(SPY.posX - st->wanderTo[0],
                                      SPY.posY - st->wanderTo[1], 0);
            st->wanderTimer = 0;
          }
        }
        break;
      }
      case SAVEHOST_WANDER: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 0x400 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2800 ||
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) >= 0xC01) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->wanderTimer++;

          SubtractVector(svA, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (svA[0] * cosDelta) / totalWeight;
          actor->posY += ((svA[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->wanderTimer == 0x10) {
            actor->mode = SAVEHOST_HOVER;
            st->wanderTimer = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }

        break;
      }

      case SAVEHOST_POP_OUT: {
        if (actor->scale < 96) {
          actor->scale += 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 16;

          g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (void *)0xC);

          actor->groundCode &= ~ACTOR_GROUND_VALID;
        }
        break;
      }
      }

      break;
    }

    /* Sparx (SparxState). */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = (SparxState *)actor->state;
      char pitchAngle;

      if (SPY.sparxHealth >= SPARX_BLUE) {

        svA[0] = 0;
        svA[1] = 0x64;
        svA[2] = 0;
        RotateVectorByMatrix(actor->mtx, svA, svA);
        AddVector(svA, svA, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svA, 0);

        svA[0] = 0;
        svA[1] = -0x64;
        svA[2] = 0;
        ApplyActiveGteRotation(svA, svA);
        AddVector(svA, svA, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svA, 0);
      }

      if (SPY.sparxHealth >= SPARX_GOLD) {
        if (st->glow == 0) {
          FxNode *g = AllocWorldSparkle();
          st->glow = g;
          if (g != 0) {
            g->anchor = &actor->posX;
            st->glow->size = 0x40;
            st->glow->offX = 0;
            st->glow->offY = 0;
            st->glow->offZ = 0;
            st->glow->r = 0xC0;
            st->glow->g = 0xC0;
            st->glow->b = 0x60;
            st->glow->kind = 9;
            st->glow->desc = &g_anSparkleGlowDesc;
          }
        } else {
          st->glow->size += 0x20;
          if (st->glow->size > 0x400) {
            st->glow->size = 0x400;
          }
        }
      } else {
        if (st->glow != 0) {
          ClearWord(st->glow);
          st->glow = 0;
        }
      }

      if (SPY.sparxHealth <= SPARX_GONE) {
        Actor *target = st->target;
        if (target != 0 && target->type == ACTOR_TYPE_BUTTERFLY) {
          target->mode = 0;
        }
        g_pSparxActor = 0;
        DespawnActorRecord(actor);
        break;
      }

      if (st->target != 0 && ApproxDist2D(&actor->posX, &SPY.posX) > 8192) {
        st->timer = 0;
      }

      if (actor->subMode == SPARX_SUB_PARKED)
        break;

      if (st->target != 0) {
        switch (actor->subMode) {
        case SPARX_SUB_CHASE: {
          Actor *target = st->target;
          ChaseState *bp = target->state;
          int isPickupAnimPhase = actor->nextAnim & 1;
          int dist;
          int targetSpeed;

          CopyVector(svJ1, &target->posX);
          if (isPickupAnimPhase == 0) {
            svJ1[0] -= g_anCosineLut[st->target->rotZ] >> 4;
            svJ1[1] -= g_anSineLut[st->target->rotZ] >> 4;
          } else {
            svJ1[0] -= g_anCosineLut[st->target->rotZ] >> 6;
            svJ1[1] -= g_anSineLut[st->target->rotZ] >> 6;
          }

          if (isPickupAnimPhase) {
            int delta;
            if (actor->nextFrame >= 7) {
              actor->subMode++;
            }

            if (2 < actor->rotY && actor->rotY < 254) {
              delta = -actor->rotY;
              if (delta > 2) {
                delta = 2;
              }
              if (delta < -2) {
                delta = -2;
              }
              actor->rotY = actor->rotY + delta;
            }
          }

          SubtractVector(svJ1, svJ1, &actor->posX);
          dist = VectorLength(svJ1, VEC_LEN_XYZ);

          if (isPickupAnimPhase == 0) {
            if (TickCountdownByFrameDelta(st, 4) != 0 ||
                (st->timer < 80 &&
                 AbsAngleDelta8((actor->rotZ + (bp->dir * 110)) & 0xFF,
                               ((CAM.rotZ >> 4) + 0x80) & 0xFF) < 8)) {
              if (actor->nextAnim != actor->nextAnim + 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim + 1);
              }
            }
          }
          targetSpeed = isPickupAnimPhase ? 150 : 130;
          if (targetSpeed + 5 < dist) {
            ScaleVector3ByRatio(svJ1, dist, targetSpeed + 5);
          } else {
            ScaleVector3ByRatio(svJ1, dist, targetSpeed - 5);
          }
          AddVector(&actor->posX, &actor->posX, svJ1);
          TurnActorYawTowardAngle(actor, ArcTan2(svJ1[0], svJ1[1], 0), 0xA, 0, 0);
          pitchAngle = ArcTan2(VectorLength(svJ1, VEC_LEN_XY), svJ1[2], 0);
          actor->rotY = pitchAngle;
          actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          break;
        }
        case SPARX_SUB_EAT: {
          SPY.sparxHealth++;
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          actor->subMode++;
          DespawnActorRecord(st->target);
          break;
        }
        case SPARX_SUB_EAT_DONE: {
          if (g_nActorAnimDone != 0) {
            if (actor->nextAnim != actor->nextAnim - 1) {
              g_nActorAnimDone = 0;
              ANIM_ADVANCE(actor, actor->nextAnim - 1);
            }
            st->target = 0;
            actor->subMode = SPARX_SUB_CHASE;
          }
          break;
        }
        case SPARX_SUB_FETCH: {
          PickupState *cp = st->target->state;
          int dist;

          SubtractVector(svJ, &st->target->posX, &actor->posX);
          dist = VectorLength(svJ, VEC_LEN_XYZ);

          if (dist < 512) {
            cp->timer = 0;
            cp->pitch = GetRandomU32() & 0xE;
            cp->yaw = GetRandomU32() & 0xE;
            cp->roll = GetRandomU32() & 0xE;

            CopyVector(cp->vel, &st->target->posX);
            st->target->subMode = COLLECT_HOMING;

            if (st->target->type == ACTOR_TYPE_EXTRA_LIFE)
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, g_dwBurstColorType14);
            else if (st->target->type == ACTOR_TYPE_SPARX_FOOD)
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, g_dwBurstColorType15);
            else
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, g_adwActorBurstColors[st->target->type]);

            st->target = 0;
            if (!GetSoundVoiceStatusByOwner(actor, AS_PTRU8(g_pLevelSampleBankHeader)[SFX_GEM_BOUNCE])) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolR = 0x3600;
              g_nSfxOverrideVolL = 0x3600;
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);
            }
          } else {
            int dcheck = 310;

            if (dcheck < dist)
              ScaleVector3ByRatio(svJ, dist, 310);
            else
              ScaleVector3ByRatio(svJ, dist, 290);

            AddVector(&actor->posX, &actor->posX, svJ);
            TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
            pitchAngle = ArcTan2(VectorLength(svJ, VEC_LEN_XY), svJ[2], 0);
            actor->rotY = pitchAngle;
            actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          }
          break;
        }
        }
      } else {

        actor->cullRadius = 0x10;
        if (st->timer <= 0) {

          svA[0] = -0x258;
          svA[1] = (GetRandomU32() & 0x310) - 0x188;
          if (CAM.state == 3) {
            svA[2] = (GetRandomU32() & 0x7F) + 0x64;
          } else {
            svA[2] = (GetRandomU32() & 0x1FF) - 0xC8;
          }
          st->drift[0] = svA[0];
          st->drift[1] = svA[1];
          st->drift[2] = svA[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == 3) {
            svA[0] = st->drift[0] + (SPY.motionSpeed >> 2);
            svA[1] = st->drift[1];
            svA[2] = st->drift[2];
            if (svA[2] < 0x64)
              svA[2] = 0x64;
          } else {
            if (CAM.state == 0x80000009)
              svA[0] = st->drift[0] - 512;
            else {
              svA[0] = st->drift[0];
            }
            svA[1] = st->drift[1];
            svA[2] = st->drift[2];
          }
          RotateVectorByMatrix(&SPY.bodyMtx, svA, svA);
          AddVector(svA, svA, &SPY.posX);
          SubtractVector(svA, svA, &actor->posX);
          RShiftVector3(svA, 2);
          AddVector(&actor->posX, &actor->posX, svA);

          if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
          }

          if (svA[2] > 32)
            svA[2] = 32;
          if (svA[2] < -32)
            svA[2] = -32;
          actor->rotX = 0;
          actor->rotY = svA[2];
          TurnActorYawTowardAngle(actor, SPY.bodyRotZ, 4, 0, 0);
        }

        switch (SPY.sparxHealth) {
        case SPARX_GREEN:
          if (actor->nextAnim != 0) {
            if (actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
          }
          break;
        case SPARX_BLUE:
          if (actor->nextAnim != 2) {
            if (actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
          }

          break;
        case SPARX_GOLD:
          if (actor->nextAnim != 4) {
            if (actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
          }
          break;
        }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      break;
    }

    case ACTOR_TYPE_SUPERFLAME_GIVER: {
      SuperflameGiverState *st = actor->state;

      if (st->heading == -1) {
        st->heading = actor->rotZ;
      }
      switch (actor->mode) {
      case 0: {
        int range = 0xA8C;

        if (st->kind == 3) {
          range = 0xD48;
        }
        if (st->alert != 0) {
          actor->mode = 4;
          actor->posZ -= 0x1F4;
          continue;
        }
        if (actor->subMode != 0) {
          actor->subMode = 0;
          st->cool = 0;
          actor->mode = 3;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->link == -1 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) &&
            st->cool == 0 && g_nSpyroSuperflameTimer == 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < range &&
            ABS2(SPY_DZ(actor)) < 0x7D0 &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, 0)) < 0x1E) {
          st->timer = 0x4B;
          actor->subMode = 0;
          actor->mode = 1;
          continue;
        }
        break;
      }

      case 1: {
        int dz;
        short *nv;

        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        CopyVector(x288, &SPY.posX);
        g_nGameplayBlocked = 1;
        x288[0] += (g_anCosineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        x288[1] += (g_anSineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        x288[2] -= 0x17C;
        dz = x288[2] - actor->posZ;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        CopyVector(st->cam, &SPY.posX);
        st->cam[0] += (g_anCosineLut[SPY.bodyRotZ] * 0x4B) >> 10;
        st->cam[1] += (*(nv = &g_anSineLut[SPY.bodyRotZ]) * 0x4B) >> 10;
        st->cam[2] += 0x64;
        g_pSpyroRequestCameraAnchor = st->cam;
        g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
        g_anCameraActiveTargetParams.w[2] = 0x400;
        g_anCameraActiveTargetParams.w[1] = 0;
        g_anCameraActiveTargetParams.w[3] = 0;
        g_anCameraActiveTargetParams.w[4] = 0;
        g_anCameraActiveTargetParams.w[5] = 0;
        SPY.requestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        g_anCameraActiveTargetParams.w[0] = -(SPY.bodyRotZ + 0x40) << 4;
        switch (actor->subMode) {
        case 0:
          if (ApproxDist2D(&actor->posX, x288) > 0x50) {
            MoveActorAlongAngle(actor,
                          ArcTan2(x288[0] - actor->posX,
                                        x288[1] - actor->posY, 0),
                          0x64, 0, 0, 0);
          } else {
            actor->posX = x288[0];
            actor->posY = x288[1];
          }
          if (ABS2(dz) > 0x28) {
            if (dz < 0) {
              if (dz < -0x32) {
                dz = -0x32;
              }
            } else if (dz > 0 && dz > 0x32) {
              dz = 0x32;
            }
            actor->posZ += dz;
          } else {
            actor->posZ = x288[2];
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ANIM_GO(actor, 1);
            st->count = 0;
            actor->subMode = 1;
          }
          break;

        case 1: {
          int f;
          int *p;
          int *q;

          if (actor->nextFrame >= 6 &&
              (++st->count == 2 || actor->type == ACTOR_TYPE_SUPERFLAME_GIVER_ALT)) {
            int r = 0x140;

            if (actor->type == ACTOR_TYPE_SUPERFLAME_GIVER_ALT) {
              r = 0x190;
            }
            CopyVector(x288, &SPY.posX);
            x288[0] += (r * g_anCosineLut[SPY.bodyRotZ]) >> 12;
            x288[1] += (r * g_anSineLut[SPY.bodyRotZ]) >> 12;
            g_pfnLevelEmitParticles(1, EMIT_SPREAD_MOTE, x288, 0);
            st->count = 0;
          }
          if (g_nActorAnimDone != 0) {
            ANIM_GO(actor, 0);
            st->timer = 0xA;
            actor->subMode = 2;
            PlayActorMeshVoice(actor, 0, 0);
          }
          f = 7;
          if (actor->type == ACTOR_TYPE_SUPERFLAME_GIVER_ALT) {
            f = 0xE;
          }
          if (actor->nextFrame >= f && *(p = &g_nSpyroSuperflameTimer) == 0) {
            if (st->kind == 1 || st->kind == 3) {
              *p = 0x9450C00;
            } else if (st->kind == 2) {
              *p = 0x30C;
            } else {
              *p = 0x384;
            }
            *(q = &g_dwSpyroRequestMask) |= 0x80020000;
            g_bSpyroRequestYaw = st->heading;
          }
          break;
        }

        case 2:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            actor->mode = 2;
            continue;
          }
          break;
        }
        break;
      }

      case 2: {
        int dz;

        g_nGameplayBlocked = 0;
        dz = st->home[2] - actor->posZ;
        if (st->link != -1 && g_pActorListBase[st->link].mode < ACTOR_MODE_DORMANT) {
          TurnActorYawTowardAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        6, 0, 0);
        } else {
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        }
        if (ApproxDist2D(&actor->posX, st->home) > 0x50) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        0x64, 0, 0, 0);
        } else {
          actor->posX = st->home[0];
          actor->posY = st->home[1];
        }
        if (ABS2(dz) > 0x28) {
          if (dz < 0) {
            if (dz < -0x32) {
              dz = -0x32;
            }
          } else if (dz > 0 && dz > 0x32) {
            dz = 0x32;
          }
          actor->posZ += dz;
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x51 && ABS2(dz) < 0x29) {
          actor->subMode = 0;
          actor->mode = 0;
          continue;
        }
        break;
      }

      case 3:
        if (WalkActorAlongWaypoints(actor, st->path, 0x200, 0x96, 0, 6, 0x10, 0) ==
            0x100) {
          actor->subMode = 0;
          CopyVector(st->home, &actor->posX);
          actor->mode = 0;
          continue;
        }
        break;

      case 4: {
        int h = (ArcTan2(actor->posX - st->path->nodes[0].x,
                               actor->posY - st->path->nodes[0].y, 0) +
                 8) &
                0xFF;

        ANIM_GO(actor, 2);
        if (actor->subMode != 0 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) {
          actor->subMode = 0;
          st->alert = 0;
          ANIM_GO(actor, 0);
          actor->mode = 2;
          continue;
        }
        SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0xBB8, h, 8, 0x78, 0xE,
                      0x80, 0xFF, 0xFF, 0, 0, 0);
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      GetActorAttachPoint(actor, 0, x272);
      g_pfnLevelEmitParticles(2, EMIT_TWINKLE, x272, (void *)2);
      break;
    }

    /* Types 152/153: sparking debris. Falls under gravity, spinning, and
     * pops when it runs out of life or reaches the floor. */
    case ACTOR_TYPE_BOULDER_DEBRIS + 1:
    case ACTOR_TYPE_BOULDER_DEBRIS + 2: {
      DebrisFallState *st = actor->state;

      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (--actor->subMode == 0 || actor->posZ < st->floor) {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);
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
        g_pfnLevelEmitParticles(1, EMIT_SPARK_RGB, fx, col);
      }
      break;
    }

    /* Key (KeyState). */
    case ACTOR_TYPE_KEY: {
      KeyState *st;
      int dz;
      int ang;
      int i;

      st = (KeyState *)actor->state;
      if (actor->subMode != 0) {
        continue;
      }

      actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
      actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
      st->angle = st->angle + g_nFrameStep * 2;

      if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < 0x200
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, (void *)(0x8080 + 0x1000000 * i));
          }

          g_nLevelKeyState = 1;
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
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->attach].pos);
        AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                      &actor->posX);
        if (g_aRotBillboards[st->attach].life < 5) {
          st->attach = 0xFF;
        }
      } else if (st->timer >= 0xF8) {
        ang = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);
        if (ang >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
          st->attach = ang;
        }
        st->timer = (GetRandomU32() & 0x3F) + 0x18;
      }

      st->timer = st->timer + g_nFrameStep;
      continue;
    }

    /* Wooden chest (BreakState). */
    case ACTOR_TYPE_WOOD_CHEST: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->snapToGround != 0) {
        SnapActorToGround(actor);
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) == 0) {
        continue;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 1, actor);
      }
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 2, actor);
      }

      g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
      SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
      MarkActorCommittedKill(actor);
      DespawnActorRecord(actor);
      continue;
    }

    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case ACTOR_TYPE_WOBBLE_CHEST: {
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

      if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
        }

        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);

        if ((actor->flags & ACTOR_HIT_FLAME) && st->phase == 0) {
          st->phase = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      actor->flags = 0;
      break;
    }

    /* Crystal dragon (RideState). */
    case ACTOR_TYPE_DRAGON_STATUE: {
      RideState *st = (RideState *)actor->state;

      if (actor->mode == STATUE_INIT) {
        int padIdx = st->partner;
        if (padIdx != -1) {
          g_pActorListBase[padIdx].mode = 3;
        }
        actor->mode = STATUE_IDLE;
        st->savedRotX = actor->rotX;
        st->savedRotY = actor->rotY;
        st->savedPosZ = actor->posZ;
      } else if (actor->mode == STATUE_IDLE) {
        st->phase += g_nFrameStep;
        if (st->phase > 256) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if (st->phase >= 192) {

          if (!GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0])) {
            PlayActorMeshVoice(actor, 0, 0);
          }
          actor->rotX =
              (signed char)g_aShakeRotOffsets[(st->phase - 192) >> 1].a + st->savedRotX;
          actor->rotY =
              (signed char)g_aShakeRotOffsets[(st->phase - 192) >> 1].b + st->savedRotY;
          actor->posZ =
              st->savedPosZ +
              (ABS2((signed char)g_aShakeRotOffsets[(st->phase - 192) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(st->phase - 192) >> 1].b)) *
                  6;
        } else if (st->phase >= 188) {
          st->savedRotX = actor->rotX;
          st->savedRotY = actor->rotY;
          st->savedPosZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = 188;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else {
          if ((st->phase >> 1) == 16 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
          } else if ((st->phase >> 1) == 48 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
          } else if ((st->phase >> 1) == 80 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) < 2048) {

          SubtractVector(svK, &actor->posX, &SPY.posX);
          svK[2] = (svK[2] * 3) >> 2;
          if (VectorLength(svK, VEC_LEN_XYZ) < 1088) {
            int rotZ;
            int cutsceneId;
            if (st->partner != -1) {
              rotZ = g_pActorListBase[st->partner].rotZ;
            } else {
              rotZ = st->fallbackHeading;
            }
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, rotZ);

            if (st->rewardScript != -1) {
              g_anLevelDragonsRescued[g_nLevelIndex]++;
              g_nWorldDragonsRescued++;
              StubRewardHook(st->rewardScript, 0);
              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            } else if (cutsceneId = st->cutsceneId, cutsceneId == st->rewardScript) {
              g_anLevelDragonsRescued[g_nLevelIndex]++;
              g_nWorldDragonsRescued++;
              if (st->partner != cutsceneId) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            } else {
              actor->mode = STATUE_RESCUING;
              actor->rotX = st->savedRotX;
              actor->rotY = st->savedRotY;
              actor->posZ = st->savedPosZ;
              BeginDragonRescue(actor);
              ZeroVector(&SPY.headLook);
              SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT);
              if (SPY.airborneFrames != 0) {
                SPY.requestState = SPYRO_ST_FALL;
                SPY.requestVelX = SPY.motionX >> 8;
                SPY.requestVelY = SPY.motionY >> 8;
                if (SPY.motionZ > 0) {
                  SPY.requestVelZ = 0;
                } else {
                  SPY.requestVelZ = SPY.motionZ >> 6;
                }
              } else {
                SPY.requestState = SPYRO_ST_STOP;
                CopyVector(&SPY.requestVelX, &SPY.motionX);
                RShiftVector3(&SPY.requestVelX, 6);
                SPY.requestVelZ = 0;
                if (SPY.requestVelX != 0 || SPY.requestVelY != 0) {
                  ScaleVector3ByRatio(&SPY.requestVelX, VectorLength(&SPY.requestVelX, VEC_LEN_XY), 0x60);
                }
                SPY.requestVelZ = 0;
              }
            }
          }
        }
      } else {
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
      }
      break;
    }

    /* Dragon fragment, updated out of line. */
    case ACTOR_TYPE_STATUE_SHARD:
      TickStatueBurstShard(actor);
      break;

    /* Floating digits (GemDigitState). */
    case ACTOR_TYPE_GEM_DIGIT_0:
    case ACTOR_TYPE_GEM_DIGIT_0 + 1:
    case ACTOR_TYPE_GEM_DIGIT_0 + 2:
    case ACTOR_TYPE_GEM_DIGIT_0 + 3:
    case ACTOR_TYPE_GEM_DIGIT_0 + 4:
    case ACTOR_TYPE_GEM_DIGIT_0 + 5:
    case ACTOR_TYPE_GEM_DIGIT_0 + 6:
    case ACTOR_TYPE_GEM_DIGIT_0 + 7:
    case ACTOR_TYPE_GEM_DIGIT_0 + 8:
    case ACTOR_TYPE_GEM_DIGIT_0 + 9: {
      GemDigitState *st = actor->state;

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
          g_pfnLevelEmitParticles(10, EMIT_STREAK_BURST_WHITE, &actor->posX, 0);
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Ambient sound source (AmbientSoundState): plays random ambient sounds;
     * mode 2 also steps it along a waypoint path. */
    case ACTOR_TYPE_AMBIENT_SOUND: {
      AmbientSoundState *st = actor->state;
      int voice = -1;

      actor->soundRange = st->soundRange;

      switch (st->mode) {
      case 0:
        voice = 6;
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->sub) {
        case 0:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->timer = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->sub = 1;
          }
          break;

        case 1: {
          int left = st->left;

          st->left = left - 1;

          if (left > 0) {
            actor->posX += st->step[0];
            actor->posY += st->step[1];
            actor->posZ += st->step[2];
          } else {
            PathData *path = st->path;
            int next = (path->cur + 1) % path->count;

            actor->posX = path->nodes[next].x;
            actor->posY =
                st->path->nodes[(st->path->cur + 1) % st->path->count].y;
            actor->posZ =
                st->path->nodes[(st->path->cur + 1) % st->path->count].z;
            st->sub = 0;
            st->path->cur = (st->path->cur + 1) % st->path->count;
            st->left = ComputeStepTowardPoint(
                &st->path->nodes[st->path->cur].x,
                &st->path->nodes[(st->path->cur + 1) % st->path->count].x,
                st->step, st->speed);
          }

          if (actor->voiceRow == VOICE_ROW_NONE && st->left == (st->left / 48) * 48) {
            voice = 0xE;
          }
          break;
        }
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          int r;

          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          r = GetRandomU32();
          voice = (r % 4) + 0x15;
        }
        break;

      case 5:
        voice = 0x18;
        break;

      case 6:
        voice = 0x19;
        break;

      case 7:
        voice = 0x12;
        break;

      case 8:
        voice = 0x1A;
        break;

      case 9:
        voice = 0x36;
        break;
      }

      if (voice >= 0) {
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, SFX_MODE_3D, &actor->voiceRow);
      }

      break;
    }

    /* Homing wisp (WispState). */
    case ACTOR_TYPE_HOMING_WISP: {
      WispState *st = actor->state;
      int i;

      if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st->target != 0) {
        CopyVector(svK, &st->target->posX);
        svK[2] += 0x2BC;
      } else {
        CopyVector(svK, &SPY.posX);
      }
      {
        int d = (ArcTan2(svK[0] - actor->posX, svK[1] - actor->posY, 0) -
                 actor->rotZ) &
                0xFF;

        if (d > 0x80) {
          d -= 0x100;
        }
        if (d > 2) {
          d = 2;
        }
        if (d < -2) {
          d = -2;
        }
        actor->rotZ += d;
      }
      CopyVector(svL, &actor->posX);
      if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 0x86) != 0) {
        for (i = 0; i < 3; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_FALLING_DEBRIS, actor);
        }
        g_anSpyroRequestVelocity.x = g_anCosineLut[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityY = g_anSineLut[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityZ = 0;
        DespawnActorRecord(actor);
        continue;
      }
      if (TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
          TryMoveActorForward(actor, 0x12C, 0, 0x100, 0) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st->target != 0) {
        if (ApproxDist2D(&actor->posX, svK) < 0x200) {
          int dz = actor->posZ - svK[2];

          if (ABS(dz) < 0x200) {
            for (i = 0; i < 3; i++) {
              if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
                break;
              }
              g_pfnLevelSpawnActor(ACTOR_TYPE_FALLING_DEBRIS, actor);
            }
            st->target->flags |= 1;
            DespawnActorRecord(actor);
            continue;
          }
        }
      } else {
        g_pfnLevelEmitParticles(1, EMIT_JITTER_DRIFT, &actor->posX, 0);
      }
      if (CastRayWorldAndActors(svL, &actor->posX) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    case ACTOR_TYPE_BOLT_CASTER: {
      BoltCasterState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != 4) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 4;
        ANIM_RESET(actor, 4);
        continue;
      }
      if ((actor->flags & 1) && actor->mode < 4) {
        actor->mode = 5;
        ANIM_RESET(actor, 5);
        continue;
      }
      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2(SPY_DZ(actor)) < 0xC00) {
          if (st->flags & 1) {
            st->flags |= 2;
            ENTER_POSE(actor, 2);
            continue;
          }
          ENTER_POSE(actor, 1);
          continue;
        }
        if (st->target >= 0) {
          if (g_pActorListBase[st->target].mode >= ACTOR_MODE_DORMANT) {
            st->target = -1;
          } else if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ENTER_POSE(actor, 2);
            continue;
          } else {
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                        g_pActorListBase[st->target].posY - actor->posY,
                                        0),
                          8, 0x10, 1);
          }
        }
        break;

      case 1:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        if (g_nActorAnimDone != 0) {
          st->flags |= 1;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;

      case 2:
        if (st->flags & 2) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800 &&
              ABS2(SPY_DZ(actor)) < 0xC00) {
            TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
          } else {
            st->flags &= ~2;
            ENTER_POSE(actor, 0);
            continue;
          }
        } else {
          TurnActorYawTowardAngle(actor,
                        ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                      g_pActorListBase[st->target].posY - actor->posY,
                                      0),
                        8, 0x10, 1);
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0;
          st->count = 0;
          actor->mode = 3;
          ANIM_RESET(actor, 3);
          continue;
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->timer, 4) && actor->nextFrame >= 4 &&
            st->count < 3) {
          Actor *b = g_pfnLevelSpawnActor(ACTOR_TYPE_HOMING_WISP, actor);

          if (st->flags & 2) {
            if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA00 &&
                ABS2(SPY_DZ(actor)) < 0x400) {
              CopyVector(&b->posX, &SPY.posX);
            }
          } else {
            Actor **bs = b->state;

            CopyVector(v376, &g_pActorListBase[st->target].posX);
            v376[2] += 0x2BC;
            b->rotY = ArcTan2(ApproxDist2D(&actor->posX, v376),
                                     v376[2] - actor->posZ, 0);
            b->rotZ = ArcTan2(v376[0] - actor->posX,
                                     v376[1] - actor->posY, 0);
            b->cullRadius = 0x24;
            *bs = &g_pActorListBase[st->target];
          }
          st->timer = 4;
          st->count++;
        }
        if (g_nActorAnimDone != 0) {
          if ((st->flags & 2) &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
              ABS2(SPY_DZ(actor)) < 0xC00) {
            actor->mode = 2;
            ANIM_RESET(actor, 2);
            continue;
          }
          st->flags &= ~2;
          st->timer = (GetRandomU32() & 0x1F) + 0x20;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 5:
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    case ACTOR_TYPE_CHUNK_OPERATOR: {
      ChunkOperatorState *st = actor->state;
      int i;

      if (st->riding != 0) {
        if (st->node >= 0) {
          UnpackWorldCollisionTri(st->node, t392[0]);
          ZeroVector(v376);
          for (i = 0; i < 3; i++) {
            SubtractVector(t392[i], t392[i], st->pts[i]);
            AddVector(v376, v376, t392[i]);
          }
          v376[0] /= 3;
          v376[1] /= 3;
          v376[2] /= 3;
          AddVector(&actor->posX, &actor->posX, v376);
        }
        actor->posZ += 0x400;
        RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
        i = FindGroundHeightBelow(&actor->posX, 0x1000);
        if (i > 0) {
          actor->posZ = i;
          st->node = g_nLastCollisionHitChunkId;
          UnpackWorldCollisionTri(g_nLastCollisionHitChunkId, st->pts[0]);
        } else {
          st->node = -1;
        }
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 3) {
        Actor *hit;

        StopSoundVoiceByIndex(actor->voiceRow, 4);
        hit = SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
        if (hit != 0) {
          ((unsigned char *)hit->state)[0xE] = 3;
        }
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 3);
        continue;
      }
      if ((actor->flags & 1) && actor->mode < 3) {
        ENTER_POSE(actor, 4);
        continue;
      }
      switch (actor->mode) {
      case 0: {
        int d;
        int r;

        if (st->riding != 0) {
          ENTER_POSE(actor, 1);
          continue;
        }
        d = ApproxDist2D(&SPY.posX, (int *)(st->anchor + 0x18));
        if (d < 0x2000 && st->spark != 0 && (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, &g_pActorListBase[st->spark]);
        }
        r = GetWorldChunkAnimState(st->chunkAnim);
        if (r & CHUNK_ANIM_HALTED) {
          r = CHUNK_ANIM_STEP(r);
          if (r == 0 && d < 0x2000) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            ENTER_POSE(actor, 2);
            continue;
          }
          if (r == 0xE && d > 0x3000) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            ENTER_POSE(actor, 2);
            continue;
          }
        }
        break;
      }

      case 1:
        if (st->spark >= 0 && (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, &g_pActorListBase[st->spark]);
        }
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        if (st->spark != 0 && (GetRandomU32() & 1) &&
            ApproxDist2D(&SPY.posX, (int *)(st->anchor + 0x18)) < 0x2000) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, &g_pActorListBase[st->spark]);
        }
        break;

      case 3:
        if (st->riding == 0) {
          int r = GetWorldChunkAnimState(st->chunkAnim);

          if (r & CHUNK_ANIM_HALTED) {
            r = CHUNK_ANIM_STEP(r);
            if (r == 0xE) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
        break;

      case 4:
        if (st->spark >= 0) {
          if (GetRandomU32() & 1) {
            g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, &g_pActorListBase[st->spark]);
          }
        }
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        break;
      }
      break;
    }

    case ACTOR_TYPE_RUNNER_SPAWNER: {
      RunnerSpawnerState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 4) {
        StopSoundVoiceByIndex(actor->voiceRow, 4);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 4);
        continue;
      }
      if ((actor->flags & 1) && actor->mode != 2 && actor->mode != 4) {
        ENTER_POSE(actor, 2);
        continue;
      }
      TickCountdownByFrameDelta(&st->timer, 4);
      switch (actor->mode) {
      case 0:
        if (st->timer == 0 && !(st->flags & 1)) {
          st->timer = 0xB4;
          ENTER_POSE(actor, 3);
          continue;
        }
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          ENTER_POSE(actor, 1);
          continue;
        }
        break;

      case 1:
        if (g_nActorAnimDone != 0) {
          st->wait = (GetRandomU32() & 0x1F) + 0x30;
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 2:
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 3:
        if (actor->nextFrame >= 5 && !(st->flags & 1)) {
          st->link[1] = 0;
          ((short *)g_pfnLevelSpawnActor(ACTOR_TYPE_WAYPOINT_RUNNER, actor)->state)[10] = st->value;
          st->flags |= 1;
        }
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, 0);
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          StopSoundVoicesByOwner(actor, VOICE_STOP_LOOPING);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }

    case ACTOR_TYPE_WAYPOINT_RUNNER: {
      WaypointRunnerState *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (g_nActorAnimDone != 0) {
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->link->mode >= 4) {
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 1: {
        int *pos = &actor->posX;

        if (RaiseSpyroTriggerInRadius(pos, 0x280, 0x46) != 0) {
          ((int *)st->link->state)[1] &= ~1;
          ENTER_POSE(actor, 2);
          continue;
        }
        if (WalkActorAlongWaypoints(actor, st->path, 0x100, 0x40, 0, 0x20, 0xFF,
                          &st->hold) == 0x100) {
          ((int *)st->link->state)[1] &= ~1;
          ENTER_POSE(actor, 2);
          continue;
        }
        if (st->target >= 0 && g_pActorListBase[st->target].mode < ACTOR_MODE_DORMANT &&
            ApproxDist2D(pos, &g_pActorListBase[st->target].posX) < 0x280) {
          ((int *)st->link->state)[1] &= ~1;
          g_pActorListBase[st->target].flags |= 1;
          ENTER_POSE(actor, 2);
          continue;
        }
        break;
      }

      case 2:
        if (g_nActorAnimDone != 0) {
          StopSoundVoicesByOwner(actor, VOICE_STOP_LOOPING);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      FindGroundHeightBelow(&actor->posX, 0x10000);
      EncodeCachedVecToActorDirCode(actor);
      break;
    }

    case ACTOR_TYPE_FLEEING_WALKER: {
      FleeingWalkerState *st = actor->state;

      TickActorFlameGlow(actor);
      if (actor->mode == 2) {
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        if (st->speed < 0x40) {
          st->speed = 0x40;
        }
        actor->posX += (st->speed * LookupCosine(st->heading)) >> 12;
        actor->posY += (st->speed * LookupSine(st->heading)) >> 12;
        continue;
      }
      if (actor->flags & ACTOR_HIT_SUPER) {
        st->heading = SPY.bodyYaw;
        st->speed = SPY.frameMotionLength >> 5;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
        continue;
      }
      actor->flags = 0;
      if (SPY.state == SPYRO_ST_HURT) {
        st->timer = 0x78;
        st->mode |= 0x10;
      }
      if (g_nSpyroAirborneFrames == 0) {
        if (ABS2(actor->posZ - SPY.posZ + 0x164) < 0x140 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < st->range) {
          st->mode |= 8;
          ZeroVector(st->vel);
        } else {
          st->mode &= ~8;
        }
        switch (st->mode & 3) {
        case 0:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode |= 4;
          }
          break;

        case 1:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          }
          break;

        case 2:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathB)) < 0xC00) {
            st->cur = st->pathA;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 1;
          }
          break;

        case 3:
          if (ApproxDist2D(&SPY.posX, PATH_END(st->pathA)) < 0xC00) {
            st->pathA->cur = st->pathA->count - 1;
            st->cur = st->pathA;
            st->mode = (st->mode | 4) & ~3;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathB)) < 0xC00) {
            st->cur = st->pathA;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 1;
          } else if (ApproxDist2D(&SPY.posX, PATH_END(st->pathC)) < 0xC00) {
            st->cur = st->pathB;
            st->cur->cur = 0;
            st->mode = (st->mode & ~3) | 2;
          }
          break;
        }
      }
      if (st->mode & 0x10) {
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->mode &= ~0x10;
        }
        if (actor->mode != 0) {
          ENTER_POSE(actor, 0);
          continue;
        }
      } else if (st->mode & 8) {
        RaiseSpyroTriggerInRadius(&actor->posX, 0x540, 7);
        if (actor->mode != 3) {
          ENTER_POSE(actor, 3);
          continue;
        }
        actor->frameRate = 0x10;
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          if (TrackEntityYawTowardSpyro(actor, 8, 0x30, 1) != 0 &&
              MoveActorAlongHeading(actor, st->speed, 0, 0x400, 0x15) == 2) {
            st->timer = 0x12;
            st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                        g_anCollisionContactPoint[1] - actor->posY, 0);
            if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
              st->heading = (st->heading - 0x40) & 0xFF;
            } else {
              st->heading = (st->heading + 0x40) & 0xFF;
            }
          }
        } else {
          TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
          MoveActorAlongHeading(actor, st->speed, 0, 0x400, 0x15);
        }
      } else if (st->mode & 4) {
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          if (st->mode & 0x20) {
            st->cur->cur = 0;
          }
          if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) > 0x100) {
            if (actor->mode != 0) {
              ENTER_POSE(actor, 0);
              continue;
            }
            actor->frameRate = 8;
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            if (actor->mode != 1) {
              ENTER_POSE(actor, 1);
              continue;
            }
            actor->frameRate = 0x10;
          }
        } else {
          TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
          MoveActorAlongHeading(actor, 0x80, 0, 0x400, 0x15);
        }
      } else if (st->mode & 3) {
        if (st->mode & 0x40) {
          if (actor->mode != 1) {
            ENTER_POSE(actor, 1);
            continue;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->mode &= ~0x40;
          }
        } else {
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          actor->frameRate = 8;
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) < 0x100) {
              if (st->mode & 0x80) {
                if (st->cur->cur != 0) {
                  st->cur->cur--;
                } else {
                  st->mode = (st->mode & ~0x80) | 0x40;
                  st->timer = 0x3C;
                }
              } else if (st->cur->cur >= st->cur->count - 2) {
                st->mode |= 0xC0;
                st->timer = 0x3C;
              } else {
                st->cur->cur++;
              }
            }
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
            MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15);
          }
        }
      } else {
        st->cur->cur = 0;
        if (ApproxDist2D(&actor->posX, PATH_CUR(st->cur)) > 0x100) {
          if (actor->mode != 0) {
            ENTER_POSE(actor, 0);
            continue;
          }
          actor->frameRate = 8;
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(NODE_PT(st->cur)[0] - actor->posX,
                                            NODE_PT(st->cur)[1] - actor->posY,
                                            0),
                              8, 0x30, 1) != 0 &&
                MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15) == 2) {
              st->timer = 0x12;
              st->heading = ArcTan2(g_anCollisionContactPoint[0] - actor->posX,
                                          g_anCollisionContactPoint[1] - actor->posY, 0);
              if (((st->heading - actor->rotZ) & 0xFF) < 0x80) {
                st->heading = (st->heading - 0x40) & 0xFF;
              } else {
                st->heading = (st->heading + 0x40) & 0xFF;
              }
            }
          } else {
            TurnActorYawTowardAngle(actor, st->heading, 8, 0x30, 1);
            MoveActorAlongHeading(actor, 0x60, 0, 0x400, 0x15);
          }
        } else {
          if (actor->mode != 1) {
            ENTER_POSE(actor, 1);
            continue;
          }
          actor->frameRate = 0x10;
        }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Sparkling shard: 1 puff per frame. */
    case ACTOR_TYPE_SPARKLE_SHARD: {
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

        svM[0] = (GetRandomU32() & 0xFE) - 0x7F;
        svM[1] = (GetRandomU32() & 0xFE) - 0x7F;
        svM[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(svM, svM, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, svM, (int *)1);
        st->life -= 1;
      } else {
        DespawnActorRecord(actor);
        continue;
      }

      break;
    }

    /* Wood shard: bounces and throws three sparks a frame. */
    case ACTOR_TYPE_WOOD_SHARD:
    case ACTOR_TYPE_WOOD_SHARD + 1: {
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
            svT[0] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[1] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(svT, svT, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, svT, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (int *)0x10);
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Type 312: cracking boulder. Shakes and smokes when hit by a charge or
     * flame, then bursts into three rings of debris and knocks the camera
     * back. */
    case ACTOR_TYPE_CRACKING_BOULDER: {
      CrackingBoulderState *st = actor->state;

      switch (actor->subMode) {
      case 0:
        *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
        actor->subMode = 1;
        break;

      case 1:
        if (actor->flags & ACTOR_HIT_BLAST) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & ACTOR_HIT_FLAME) {
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
        if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) {
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
        if ((actor->flags & ACTOR_HIT_BLAST) && t < 0x5A) {
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
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, ps, ps + 3);
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
          s496[0] = (GetRandomU32() & 7) + 0x10;
          s496[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, ps, s496);
          if (actor->voiceRow == VOICE_ROW_NONE) {
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
          SpawnActorDeathDrop(actor, DROP_HIGH_HOP, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            p = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 2, actor);
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
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 1, actor);
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
            int len = VectorLength(&g_anSpyroRequestVelocity.x, VEC_LEN_XYZ);

            ScaleVector3ByRatio(&g_anSpyroRequestVelocity.x, len,
                          (0x1000 - ApproxDist2D(&actor->posX, &SPY.posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = 0x46;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_BLAST), actor, 0);
        if (++st->timer >= 8) {
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }

    /* Falling debris: tumbles along its velocity while on screen and is
     * removed once its timer runs out or it goes off screen. */
    case ACTOR_TYPE_FALLING_DEBRIS: {
      int *st = actor->state;

      if (TickCountdownByFrameDelta(&st[3], 4) != 0 || actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      st[2] -= 6;
      if (st[2] < -0x80) {
        st[2] = -0x80;
      }
      AddVector(&actor->posX, &actor->posX, st);
      actor->rotX += 0xF;
      actor->rotZ += 9;
      break;
    }

    /* Respawner (RespawnState). */
    case ACTOR_TYPE_RESPAWNER: {
      RespawnState *st = actor->state;
      Actor *p;

      CopyVector(&actor->posX, &SPY.posX);
      if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
        break;
      }
      st->timer = st->period;

      for (p = g_pActorListBase; p < g_pActorPoolDynBase; p++) {
        if (p->type != st->type) {
          continue;
        }
        if (p->mode < ACTOR_MODE_DORMANT) {
          continue;
        }
        if (ApproxDist2D(&SPY.posX, p->state) < 0x6001) {
          continue;
        }

        CopyVector(&p->posX, p->state);
        InsertActorIntoSpatialGrid(p);
        p->killGroup &= ACTOR_KILL_GROUP_MASK;
        p->flags = 0;
        p->mode = 0;
        p->frameProgress = 0;
        p->frameRate = TYPE_DESC(p, 0);
        p->anim = 0;
        p->nextAnim = 0;
        p->frame = 0;
        p->nextFrame = 1;
      }
      break;
    }

    /* Type 328: floor-snapped marker. Drops onto the floor once, then sits at
     * the centroid of the triangle it landed on. */
    case ACTOR_TYPE_FLOOR_MARKER: {
      int *tri = actor->state;

      if (*tri < 0) {
        int floor;
        int d;

        actor->posZ += 0x200;
        floor = FindGroundHeightBelow(&actor->posX, 0x400);
        d = actor->posZ - floor;
        if (ABS(d) < 0x400) {
          actor->posZ = floor;
          *tri = g_nLastCollisionHitChunkId;
        } else {
          actor->posZ -= 0x200;
        }
        if (*tri < 0) {
          break;
        }
      }
      UnpackWorldCollisionTri(*tri, mat2[0]);
      CopyVector(&actor->posX, mat2[0]);
      AddVector(&actor->posX, &actor->posX, mat2[1]);
      AddVector(&actor->posX, &actor->posX, mat2[2]);
      actor->posX /= 3;
      actor->posY /= 3;
      actor->posZ /= 3;
      break;
    }

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case ACTOR_TYPE_SEED_POD: {
      SeedPodState *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case SEEDPOD_CLOSED:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
          actor->subMode = 1;
          actor->updateDist = 0x10;
        }
        if (actor->flags & ACTOR_HITS_SPYRO) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = 0x40;
          st->prize = SpawnActorDeathDrop(actor, DROP_IN_PLACE, 0, 0);
          st->prize->subMode = COLLECT_INERT;
          st->prize->cullRadius = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, EMIT_GEM_COLOR_BURST, &actor->posX, (void *)(st->prize->type - 0x53));
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = SEEDPOD_OPEN;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          SpawnActorRotBillboard(actor, g_anRotBillboardOffsetsRand[GetRandomU32() & 3]);
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
        break;

      case SEEDPOD_OPEN: {
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
          ZeroVector(s496);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, s496);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == SPYRO_ST_SUPERCHARGE || g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
            (g_nSpyroState == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & ACTOR_HIT_SUPER) {
          st->prize->subMode = COLLECT_BOUNCE;
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        st->timer += g_nFrameStep;
        if (st->timer >= 0x56) {
          int j;

          for (j = 0; j < 6; j++) {
            CopyVector(s512, &st->prize->posX);
            s512[0] += g_anCosineLut[j * 43] >> 6;
            s512[1] += g_anSineLut[j * 43] >> 6;
            s512[2] += 0x28;
            s528[0] = g_anCosineLut[j * 43] >> 7;
            s528[1] = g_anSineLut[j * 43] >> 7;
            s528[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, s512, s528);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= ACTOR_KILL_GROUP_MASK;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, 1, 0);
          actor->mode = SEEDPOD_CLOSED;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Checkpoint: Spyro reaching it sets the respawn anchor
     * (SetSpawnAnchorFromActor); it then plays its anim for 0x30 frames. */
    case ACTOR_TYPE_CHECKPOINT + 2: {
      switch (actor->mode) {
      case 0: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) > 2560) {
          actor->mode = 1;
        }
        break;
      }

      case 1: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 896 &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 512) {

          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = 2;
          actor->anim = 1;
          actor->subMode = 0;
        }
        break;
      }

      case 2: {
        actor->subMode += g_nFrameStep;

        if (actor->subMode >= 0x30) {
          actor->mode = 0;
          actor->anim = 0;
        }
        break;
      }
      }

      break;
    }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case ACTOR_TYPE_SPIN_CHEST: {
      SpinChestState *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
      int cur = g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0];
      int i;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, SFX_MODE_3D,
                      &actor->voiceRow);
        if (actor->voiceRow != VOICE_ROW_NONE) {
          g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
      }
      if (actor->subMode == 0) {
        st->lid = g_pfnLevelSpawnActor(ACTOR_TYPE_HOMING_SPARK, actor);
        st->lid->cullRadius = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = 1;
        continue;
      }
      if (st->timer == 0 || --st->timer == 0) {
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->timer = 0x10;
          st->speed += 0x64;
        }
      }
      st->handle = SelectActorLodBand(actor, st->handle);
      st->speed -= 1;
      if (pitch < cur && actor->voiceRow != VOICE_ROW_NONE) {
        g_anSpuVoiceRowPitchStep[actor->voiceRow & 0x7F][0] = -4;
      }
      if (st->speed < 0x18) {
        st->speed = 0x18;
        actor->updateDist = 0x10;
      } else {
        actor->updateDist = 0x40;
      }
      st->angle += (st->speed * g_nFrameStep) >> 1;
      st->lid->rotZ = st->angle >> 4;
      if (st->speed > 0xE0 || (actor->flags & ACTOR_HIT_SUPER)) {
        st->lid->subMode = 1;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
        }
        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      }
      actor->flags = 0;
      break;
    }

    /* Type 392: homing spark. Launched at the nearest enemy in front of
     * Spyro (or away from him when there is none), and bursts on what it
     * hits. */
    case ACTOR_TYPE_HOMING_SPARK: {
      HomingSparkState *st = actor->state;
      int i;

      if (actor->subMode == 1) {
        Actor *best = 0;
        int bestAng = 0x180;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= ACTOR_MODE_DORMANT || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(s496, &a->posX, &actor->posX);
          d = ABS2(s496[0]) + ABS2(s496[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(s496, VEC_LEN_XY) - 0x1000) <= 0x2000 &&
              (unsigned int)(s496[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(s496[0], s496[1], 1));
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
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, VEC_LEN_XY), 0x50);
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
             DispatchActorContactAtSphere(&actor->posX, 0x100, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, 0) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, 0x400, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          }
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
          PlayActorMeshVoice(actor, 0, 0);
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0xB4;
      }
      continue;
    }

    /* Floating-word letters (OrbitState). */
    case ACTOR_TYPE_GLYPH_COMMA:
    case ACTOR_TYPE_LETTER_A:
    case ACTOR_TYPE_LETTER_A + 1:
    case ACTOR_TYPE_LETTER_A + 2:
    case ACTOR_TYPE_LETTER_A + 3:
    case ACTOR_TYPE_LETTER_A + 4:
    case ACTOR_TYPE_LETTER_A + 5:
    case ACTOR_TYPE_LETTER_A + 6:
    case ACTOR_TYPE_LETTER_A + 7:
    case ACTOR_TYPE_LETTER_A + 8:
    case ACTOR_TYPE_LETTER_A + 9:
    case ACTOR_TYPE_LETTER_A + 10:
    case ACTOR_TYPE_LETTER_A + 11:
    case ACTOR_TYPE_LETTER_A + 12:
    case ACTOR_TYPE_LETTER_A + 13:
    case ACTOR_TYPE_LETTER_A + 14:
    case ACTOR_TYPE_LETTER_A + 15:
    case ACTOR_TYPE_LETTER_A + 16:
    case ACTOR_TYPE_LETTER_A + 17:
    case ACTOR_TYPE_LETTER_A + 18:
    case ACTOR_TYPE_LETTER_A + 19:
    case ACTOR_TYPE_LETTER_A + 20:
    case ACTOR_TYPE_LETTER_A + 21:
    case ACTOR_TYPE_LETTER_A + 22:
    case ACTOR_TYPE_LETTER_A + 23:
    case ACTOR_TYPE_LETTER_A + 24:
    case ACTOR_TYPE_LETTER_A + 25: {
      OrbitState *st = actor->state;

      if (st->owner->mode == 0 || st->owner->subMode != actor->mode) {
        DespawnActorRecord(actor);
      } else if (st->owner->type == ACTOR_TYPE_LEVEL_EXIT) {
        if (ApproxDist2D(&st->owner->posX, &SPY.posX) < 0x400) {
          int oldval;
          oldval = g_anCosineLut[actor->subMode];
          actor->subMode += 8;
          actor->rotZ = actor->rotZ - (oldval * 3 >> 9) +
                         (g_anCosineLut[actor->subMode] * 3 >> 9);
        } else {
          int anglespyro;

          actor->subMode += 8;
          anglespyro = ArcTan2(SPY.posX - st->owner->posX,
                                     SPY.posY - st->owner->posY, 0);
          s512[0] = g_anCosineLut[anglespyro] * 3 >> 4;
          s512[1] = g_anSineLut[anglespyro] * 3 >> 4;
          s512[2] = 0;
          vecB[0] = g_anCosineLut[anglespyro + 64 & 0xFF] >> 5;
          vecB[1] = g_anSineLut[(anglespyro + 64) & 0xFF] >> 5;
          vecB[2] = 0;
          anglespyro = (anglespyro + 0x80) & 0xFF;
          actor->rotZ = anglespyro + (g_anCosineLut[actor->subMode] * 3 >> 9);
          ScaleVector3Sat(vecC, vecB, st->ring - 1);
          LShiftVector3(vecB, 1);
          anglespyro = 2;
          anglespyro = (st->ring - 1) * anglespyro;
          vecC[2] += g_anCosineLut[anglespyro] * 3 >> 3;
          vecD[0] = s512[0] * g_anCosineLut[anglespyro];
          vecD[1] = s512[1] * g_anCosineLut[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->ringPhase * 4)) & 0xFF;
          ScaleVector3Sat(vecB, vecB, st->ringPhase);
          SubtractVector(vecC, vecC, vecB);
          actor->posX = s512[0] * g_anCosineLut[anglespyro];
          actor->posY = s512[1] * g_anCosineLut[anglespyro];
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, vecD);
          RShiftVector3(&actor->posX, 10);
          AddVector(&actor->posX, &actor->posX, s512);
          AddVector(&actor->posX, &actor->posX, &st->owner->posX);
          SubtractVector(&actor->posX, &actor->posX, vecC);
          actor->posZ = actor->posZ + (g_anCosineLut[anglespyro] * 3 >> 3) + 0x600;
        }
      } else {
        actor->subMode += 8;
        actor->rotZ = actor->floorOffset + (g_anCosineLut[actor->subMode] * 3 >> 9);
      }
    }
    break;

    default: /* 77..82, 88..109, 258..259 and everything unlisted */
      break;
    
}
  }
}
