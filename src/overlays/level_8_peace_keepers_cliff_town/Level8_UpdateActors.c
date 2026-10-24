/* Level8_UpdateActors -- per-frame actor update for level 8 (Cliff
 * Town).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 9,
 * 15, 16, 25, 26 and 31); the level-8 classes are the bell prop (206/239/
 * 242), the catapult (218), the two-path runner (226), the lookout (229, a
 * variant of level 10's), the swooping guard (230), the wandering critter
 * (238) and the bouncing debris (288/289).
 *
 * Load-bearing source forms (matched 2026-09-30):
 *   - Level 15's loop top and frame scheme; fourteen shared scratch slots
 *     after svJ (y256..y464) in the original frame order.
 *   - Case 416 reads g_nFrameStep directly (level 0's form), so no `dt`
 *     carrier is kept live.
 *   - Case 288 reverses its sway through a temp (`sw`); re-reading the field
 *     in the `||` test costs a reload phantom slot and grows the frame by 8.
 *   - Case 230's default weave step is assigned after the heading call: a
 *     callee-saved-only insn is scheduled across the call to its slot.
 *   - Case 218 keeps both velocity factors in temps so fold does not move
 *     the constant onto the distance term.
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


/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)



#define AS_I32(sym) (((I32 *)&(sym))->v)

#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */

#define LIFT_STEP(st) (((st)->phase - 0x1A0) >> 1)

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */

#define CAM g_anWorldToCameraRotMtx



/* Glow node owned by Sparx; follows its anchor position. */

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
  int pad10;          /* 0x10 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;


#define g_nSpyroRequestVelocityY g_anSpyroRequestVelocity.y


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

/* Case 328 -- floor-snapped marker. */


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
/* ENTER_POSE with an empty loop at the head of the change: cse starts a new
 * block there, so the `nextAnim = n` store takes its own copy of the constant
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


/* ==== Camera and Spyro globals used by the camera arms ==== */
#define CAM_LOOK (*(CamLook *)&g_anSpyroRequestVelocity)

#define AS_PTRV(sym) (((PtrV *)&(sym))->v)

/* Set the state byte and queue animation `n` without promoting the current
 * one, then move on to the next actor. */
#define ANIM_QUEUE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->nextAnim = (n);                                                          \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

/* TurnActorYawTowardAngle called with its first four arguments only. */
#define FACE4(a, h, s, t)                                                      \
  ((int (*)(Actor *, int, int, int))TurnActorYawTowardAngle)(a, h, s, t)

/* Back to the idle state, restarting animation 0. */
#define RESET_IDLE(a)                                                          \
  (a)->mode = 0;                                                              \
  ANIM_RESET(a, 0);                                                            \
  continue

#define sixteen 0x10

/* Case 402: cannon ball path header { count, cur }, points at +8. */
#define PATH_PT(p, i) ((int *)((p) + (((i) << 4) + 0x8)))

/* Types 288/289: bouncing debris. Flies on its velocity until its life runs
 * out; type 288 tumbles, 289 sways as it falls. */

/* Type 218: catapult. Charging it winds it up; when it swings it flings the
 * linked actor along an arc. */

extern int g_dwActiveCameraOptions;

/* ==== The function ==== */
void Level8_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  short oneq;
  char *base;
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
  int svJ[3];
  int y256[3];
  int y272[3];
  int y288[3];
  int y304[3];
  int y320[3];
  int svT[3];
  int y352[3];
  int y368[3];
  int y384[3];
  int y400[3];
  int y416[3];
  int y432[3];
  int y448[3];
  int y464[3];

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
    oneq = 1;
    one = 1;
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
        actor->mode = oneq;
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
            g_nSpyroRequestTargetZ = ACTOR_RAW(actor, int, posZ) + 0x4000;
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
                g_nScriptedRespawnFlag = one;
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]) +
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case 16: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.readyFlag < 3 && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pLevelOverlayInitRecord == 0) {
            if (SPY.readyFlag >= 0) {
              g_pLevelOverlayInitRecord = g_pfnLevelSpawnActor(120, actor);

              PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);

              SPY.readyFlag = 1;

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

    /* Egg thief (ThiefState). */
    case 33: {
      ThiefState *st;

      do {
      } while (0);
      st = actor->state;

      if ((actor->flags & 0xB0000) != 0 && actor->mode != 10) {
        st->knock = 0x96;
        if ((actor->flags & 0x20000) != 0) {
          st->knock = 0xE1;
        }
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        if (actor->dropKind == 0xFF) {
          actor->dropKind = 0xF;
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
        }
        actor->mode = 10;
        continue;
      }

      if (actor->dropKind != 0xFF && st->egg == 0 && actor->mode != 10) {
        st->egg = g_pfnLevelSpawnActor(0x22, actor);
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
          g_dwSpyroRequestMask = 0x80002000;
          if (actor->nextAnim == 3 && actor->nextFrame >= 5) {
            st->egg->mode = 1;
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
        RefreshActorDerivedState(actor, 4);
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
        if (SPY.posZ - st->path->nodes[10].z < -0x5DC &&
            ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1800) {
          st->vz = 0x1A4;
          st->path->cur = 9;
          actor->mode = 0x2B;
          continue;
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
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, &st->egg->posX);
      }
      RefreshActorDerivedState(actor, 1);
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

    /* Explosion shard with a ground bounce (ShardState). */
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
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, svD);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Falling chest fragments (TumbleState). */
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
          CopyVector(svA, &actor->posX);
          svA[2] += 0x400;
          FindGroundHeightBelow(svA, 0x10000);
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
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
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

          ScaleVector3Sat(svG, (int *)st, g_nFrameStep);
          RShiftVector3(svG, 1);
          dist = VectorLength(svG, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(svG, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(svG, at, svG);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (svG[2] < 0) {
            if (actor->type != 14 && actor->type != 15) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          svG[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(svG, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != 14 && actor->type != 15) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(svH, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(svG, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

              if ((svG[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, svG);
                actor->posZ = floor;
                if (actor->type != 15) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->roll = -ArcTan2_8bit(svH[2], svH[0]);
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

            if (ReflectVectorOffNormal((int *)st, svH, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            svG[2] -= 0xF0;
            CopyVector(at, svG);
            FindGroundHeightBelow(svG, 0x10000);
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
            SubtractVector(svI, &spy->posX, (int *)st);
            RShiftVector3(svI, 5);

            if (VectorLength(svI, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(svI, svI, st->timer);
              AddVector(&actor->posX, (int *)st, svI);
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

    /* Lift plate (PlateState): rises once its trigger actor is gone, waits,
     * then drops back. */
    case 110: {
      PlateState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->trigger];
      Actor *rider = &g_pActorListBase[st->rider];

      switch (actor->mode) {
      case 0:
        actor->collision = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) > 0xA00) {
          actor->mode = 1;
        }
        break;

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
          SubtractVector(svF, st->from, here);
          turn = SignedAngleDelta8(st->yaw, actor->rotZ);
          w0 = LookupCosine((st->step - 1) << 7);
          wSpan = w0 - LookupCosine(st->step << 7);
          wDen = LookupCosine((st->step - 1) << 7) + 0x1000;
          actor->posX += (svF[0] * wSpan) / wDen;
          actor->posY += (svF[1] * wSpan) / wDen;
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

      if (g_anLevelReadyFlagBlock[0] >= 2) {
        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
      }

      if (g_anLevelReadyFlagBlock[0] >= 3) {
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

      if (g_anLevelReadyFlagBlock[0] <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          ApproxDist2D(&actor->posX, (int *)((char *)g_anLevelReadyFlagBlock - 0x164)) >=
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

            CopyVector(svA, &st->gnorc->posX);

            if (high == 0) {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 20;
              svA[1] -=
                  (unsigned short)g_anSineLut[st->gnorc->rotZ] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              svA[1] -=
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

            SubtractVector(svA, svA, &actor->posX);
            dist = VectorLength(svA, 1);

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
              ScaleVector3ByRatio(svA, dist, limit);
            } else {
              ScaleVector3ByRatio(svA, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, svA);
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(svA, 0), svA[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_anLevelReadyFlagBlock[0] += 1;
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

            SubtractVector(svJ, &st->gnorc->posX, &actor->posX);
            dist = VectorLength(svJ, 1);

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
                ScaleVector3ByRatio(svJ, dist, lim);
              } else {
                ScaleVector3ByRatio(svJ, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, svJ);
              TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(svJ, 0), svJ[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* Nobody in the basket: drift on the wind. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            svF[0] = -0x258;
            svF[1] = (GetRandomU32() & 0x310) - 0x188;
            svF[2] = g_nCameraCurrentMode == 3 ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            do {
            } while (0);
            st->drift[0] = svF[0];
            st->drift[1] = svF[1];
            st->drift[2] = svF[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            do {
            } while (0);

            if (g_nCameraCurrentMode == 3) {
              svF[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
              if (svF[2] < 0x64) {
                svF[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == 0x80000009) {
                svF[0] = st->drift[0] - 0x200;
              } else {
                svF[0] = st->drift[0];
              }
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, svF, svF);
            AddVector(svF, svF, &SPY.posX);
            at = &actor->posX;
            SubtractVector(svF, svF, at);
            RShiftVector3(svF, 2);
            AddVector(at, at, svF);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (svF[2] >= 0x21) {
              svF[2] = 0x20;
            }
            if (svF[2] < -0x20) {
              svF[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = svF[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, 0);
          }

          switch (SPY.readyFlag) {
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

    /* Bell prop: rings when charged, spawning a ripple, and settles. */
    case 206:
    case 239:
    case 242:
      if (actor->flags != 0) {
        if (actor->flags == 0x10000 && actor->subMode == 0) {
          actor->subMode = 0x50;
          ANIM_SET(actor, 2);
        } else if (actor->nextAnim == 0) {
          ANIM_SET(actor, 1);
        }
      }
      actor->flags = 0;
      if (g_nActorAnimDone != 0) {
        switch (actor->nextAnim) {
        case 2: {
          Actor *p = g_pfnLevelSpawnActor(0x1ED, actor);

          p->rotZ = ((unsigned short)g_nCameraEulerYaw << 16 >> 18) - 0x80;
          CopyVector(&p->posX, &actor->posX);
          ANIM_SET(actor, 3);
          break;
        }
        case 1:
        case 4:
          ANIM_SET(actor, 0);
          break;
        }
      }
      if (TickCountdownByFrameDelta(&actor->subMode, 1) == 2) {
        ANIM_SET(actor, 4);
      }
      break;

    /* Catapult (CatapultState). */
    case 218: {
      CatapultState *st = actor->state;

      if (actor->flags & 0x10000) {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int a = AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                              ArcTan2(actor->posX - spy->posX,
                                            actor->posY - SPY.posY, 0));

        actor->flags = 0;
        if (st->angle == 0) {
          int m = (0x1770 - d) >> 6;
          int k = (0x28 - a) * 0x46;

          st->timer = 0x5A;
          st->vel = (m * k) >> 12;
        }
        if (st->vel > 0x50) {
          st->vel = 0x50;
        }
      }
      st->angle += st->vel;
      if (st->angle > 0x120) {
        st->angle = 0x120;
      }
      if (st->angle < 0) {
        st->angle = 0;
      }
      st->vel -= 4;
      if (st->vel < -0x20) {
        st->vel = -0x20;
      }
      actor->frame = st->angle >> 5;
      actor->nextFrame = st->angle >> 5;
      if (st->link >= 0) {
        Actor *t = &g_pActorListBase[st->link];

        if (t->mode >= 0x80) {
          st->link = -1;
          break;
        }
        if (st->angle != 0) {
          EnemyState *fs;
          int n;
          int step;
          int z;

          PlayActorMeshVoice(actor, 0, 0);
          fs = t->state;
          CopyVector(y256, &t->posX);
          CopyVector(&t->posX, &actor->posX);
          n = 0;
          step = 0xB4;
          z = t->posZ;
          do {
            z += step;
            step -= 10;
            n++;
          } while (step > 0 || y256[2] < z);
          SubtractVector(y256, y256, &t->posX);
          DivideVector3(y256, y256, n);
          y256[2] = 0xB4;
          CopyVector(fs->vel, y256);
          fs->mode = 0;
          fs->timer = 0;
          fs->bounce = 3;
          t->subMode = 2;
          t->cullRadius = 0x18;
          t->updateDist = 0x40;
          st->flung = t;
          st->link = -1;
          break;
        }
        t->cullRadius = 0;
        t->drawn = 0;
        t->updateDist = 0;
        t->subMode = 4;
        break;
      }
      if (st->flung != 0) {
        if (st->flung->mode >= 0x80 || st->flung->subMode != 2) {
          st->flung = 0;
          break;
        }
        if (g_dwGamestateFrames & 1) {
          ZeroVector(y272);
          g_pfnLevelEmitParticles(1, 0, &st->flung->posX, y272);
        }
      }
      break;
    }

    /* Two-path patroller (RunnerState). */
    case 226: {
      RunnerState *st = actor->state;
      int r;

      TickActorFlameGlow(actor);
      if ((actor->flags & 0xA0000) && actor->mode != 5) {
        st->knockTimer = 0x190;
        st->knockSpeed = 0xAA;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        actor->flags = 0;
        actor->probeDist = 0x64;
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 5);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        if (st->mode == 1 || st->mode == 2) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 4: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        if (st->mode == 2) {
          if (ApproxDist2D(&spy->posX, &st->pathA->nodes[st->node].x) <
              st->range << 10) {
            st->which = 1;
            actor->mode = 0x14;
            continue;
          }
          if (ApproxDist2D(&spy->posX, &st->pathB->nodes[st->node].x) <
              st->range << 10) {
            st->which = 2;
            actor->mode = 0x14;
            continue;
          }
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0x44C &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x2BC) {
          st->timer = 0x78;
          ENTER_POSE(actor, 8);
        }
        break;
      }

      case 5:
        r = DriveActorTrackedMove(actor, &st->knockTimer, st->heading, &st->knockSpeed,
                          0xC, 0x10);
        if (r == 3 && st->cried == 0) {
          st->cried = 1;
          PlayActorMeshVoice(actor, 0, 0);
        }
        if (r == 2 && st->knockSpeed >= -0x63) {
          st->knockSpeed = -0x64;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 6:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 8:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 10:
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->pathA->nodes[st->pathA->cur].x -
                                        actor->posX,
                                    st->pathA->nodes[st->pathA->cur].y -
                                        actor->posY,
                                    0),
                      4, 0, 0);
        MoveActorAlongHeading(actor, 0x6E, 0, 0, 5);
        if (ApproxDist2D(&actor->posX,
                          &st->pathA->nodes[st->pathA->cur].x) < 0x80) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 0x14:
        ANIM_GO(actor, 7);
        if (st->which == 1) {
          if (PivotActorAlongPath(actor, st->pathA, 0x80,
                            actor->drawn == 0 ? 0x8C : 0x64, 0, 4, 0x14, 0xFF,
                            5) == 0) {
            break;
          }
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x5DC &&
              st->pathA->cur != st->node) {
            break;
          }
          st->mode = 1;
          ENTER_POSE(actor, 4);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x5DC &&
            PivotActorAlongPath(actor, st->pathB, 0x80, 0x50, 0, 4, 0x14, 0xFF, 5) ==
                0) {
          break;
        }
        if (st->pathB->cur != st->node) {
          break;
        }
        st->mode = 1;
        ENTER_POSE(actor, 4);
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Lookout (LookoutState): watches over the guard it is linked to, rouses
     * it when Spyro comes into range, and swipes at him up close. */
    case 229: {
      LookoutState *st = actor->state;
      Actor *link;
      int d;
      int h;
      int mode;

      link = 0;
      if (st->link != -1) {
        link = &g_pActorListBase[st->link];
      }
      if ((actor->flags & 0x90000) && actor->mode != 7) {
        st->speed = 0xF0;
        st->heading = SPY.bodyRotZ;
        actor->flags = 0;
        SpawnActorDeathDrop(actor, 1, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 7);
      }
      if (TickCountdownByFrameDelta(&st->camTimer, 4) == 0) {
        g_dwSpyroRequestMask = 0x80002047;
      }
      if (!(g_dwActiveCameraOptions & 0x10) && TickCountdownByFrameDelta(&st->camTimer2, 4) == 0) {
        int cd = ApproxDist2D(&actor->posX, &spy->posX) + 0xC00;

        if (st->camDist < cd) {
          st->camDist = cd;
        }
        if (st->camTimer != 0) {
          g_dwSpyroRequestMask |= 0x80000200;
        } else {
          g_dwSpyroRequestMask = 0x80000200;
        }
        SPY.pCamAnchor = &actor->posX;
        SPY.pCamTarget = &g_anCameraActiveTargetParams;
        g_anCameraActiveTargetParams.w[0] = -ArcTan2(SPY.posX - actor->posX,
                                         SPY.posY - actor->posY, 0)
                          << 4;
        g_nCameraActiveTargetPitch = 0;
        g_nCameraActiveTargetRoll = 0;
        g_nCameraActiveTargetPitchBias = 0;
        g_nCameraActiveTargetYawBias = 0;
        g_nCameraActiveTargetDistance = st->camDist;
      }

      mode = actor->mode;
      switch (mode) {
      case 0:
        d = ApproxDist2D(&actor->posX, &spy->posX);
        if (st->init == 0) {
          st->init = 1;
          st->home = actor->rotZ;
        }
        if (link != 0 && actor->subMode == 0 && link->mode == 0 &&
            d < st->range << 10 && actor->posZ < SPY.posZ &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x5DC) {
          actor->subMode = 1;
          ANIM_GO(actor, 4);
          actor->mode = 0xA;
          continue;
        }
        if (d > 0x2000) {
          TurnActorYawTowardAngle(actor, st->home, 4, 0, 0);
          if (actor->anim == 2 && g_nActorAnimNewFrame != 0 && actor->frame == 0xF &&
              --st->count > 0) {
            actor->frame = 5;
            actor->nextFrame = 6;
            actor->frameProgress = 0;
          }
          if (g_nActorAnimDone != 0) {
            if (st->count == 0) {
              st->count = RandomIntInRange(3, 5);
              if (RandomIntInRange(0, 0x64) > 0x50) {
                ANIM_GO(actor, 1);
              } else {
                ANIM_GO(actor, 4);
              }
            } else {
              ANIM_GO(actor, 2);
            }
          }
        } else {
          h = WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0),
                            5);
          if (actor->nextAnim == 4) {
            ANIM_GO(actor, 0);
          }
          if (actor->nextAnim == 2 && actor->nextFrame < 0xF) {
            actor->frame = 0xF;
            actor->nextFrame = 0x10;
            actor->frameProgress = 0;
          }
          if (actor->anim < 2 || actor->anim == 5) {
            TurnActorYawTowardAngle(actor, h, 4, 0, 0);
          }
          if (g_nActorAnimDone != 0) {
            if (actor->nextAnim == 2) {
              ANIM_SET(actor, 0);
            } else if (--st->count <= 0) {
              st->count = RandomIntInRange(3, 5);
              if (RandomIntInRange(0, 0x64) > 0x50) {
                if (actor->nextAnim != 1) {
                  ANIM_GO(actor, 1);
                } else {
                  ANIM_GO(actor, 0);
                }
              }
            } else {
              ANIM_GO(actor, 5);
            }
          }
        }
        if (d < 0xA8C && ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x3E8 &&
            actor->posZ - actor->floorOffset - SPY.posZ < 0) {
          ENTER_POSE(actor, 6);
        }
        break;

      case 6:
        TurnActorYawTowardAngle(actor,
                      WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                  spy->posY - actor->posY, 0),
                                    5),
                      6, 0, 0);
        if (g_nActorAnimNewFrame != 0 && actor->frame == 3 &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0xA8C) {
          SubtractVector(y288, &spy->posX, &actor->posX);
          ScaleVector3ByRatio(y288, VectorLength(y288, 0), 0x78);
          g_dwSpyroRequestMask = 0x80000047;
          g_anSpyroRequestVelocity.x = y288[0];
          g_nSpyroRequestVelocityZ = 0x28;
          g_nSpyroRequestState = mode;
          g_nSpyroRequestVelocityY = y288[1];
          st->camTimer = 0x28;
          st->camTimer2 = 0x28;
          st->camDist = 0xFA0;
          if (g_nHitRumbleTimer < 0xF) {
            g_nHitRumbleTimer = 0xF;
          }
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 7:
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0x1F4, 0x2BC, 1);
          st->speed -= 0x10;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 10:
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(link->posX - actor->posX,
                                        link->posY - actor->posY, 0),
                          8, 6, 1) != 0) {
          ANIM_GO(actor, 8);
        }
        if (actor->anim == 8 && g_nActorAnimNewFrame != 0 && actor->frame == 5) {
          if (link->nextAnim != 7) {
            ANIM_ADVANCE(link, 7);
          }
          link->mode = 0xA;
        }
        if (g_nActorAnimDone != 0 && actor->anim == 8) {
          ANIM_GO(actor, 4);
          actor->mode = 0;
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Swooping guard (SwoopState). */
    case 230: {
      SwoopState *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 4) {
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        st->speed = 0x12C;
        if (actor->mode != 0) {
          *st->alert = 0;
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        if (actor->flags & 0xB0000) {
          int i;

          for (i = 0; i < 7; i++) {
            Actor *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x120, actor);
            p = g_pfnLevelSpawnActor(0x121, actor);
            if (GetRandomU32() & 1) {
              p->mode = 1;
              ANIM_RESET(p, 1);
            }
          }
        }
        actor->mode = 4;
        ANIM_RESET(actor, 4);
        continue;
      }

      switch (actor->mode) {
      case 0: {
        int d = ApproxDist2D(&actor->posX, &SPY.posX);
        int dz = st->home[2] - actor->posZ;

        if (dz != 0) {
          if (dz >= -0x63) {
            actor->posZ = st->home[2];
          } else {
            actor->posZ -= 0x64;
          }
        }
        if (st->heading == -1) {
          st->heading = actor->rotZ;
        }
        TurnActorYawTowardAngle(actor, st->heading, 4, 0, 0);
        if (*st->alert == 0 && d < st->range << 10 && d >= 0x7D1 &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0xBB8) {
          CopyVector(y288, &actor->posX);
          y288[0] += g_anCosineLut[actor->rotZ] >> 2;
          y288[1] += g_anSineLut[actor->rotZ] >> 2;
          if (actor->drawn != 0 &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, 0)) < 0x19 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0)) < 0x30 &&
              CheckSpyroLineOfSightTo(y288) != 0) {
            if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
              break;
            }
            st->swayDir = 1;
            st->sway = 0;
            st->speed = 0;
            st->weaving = 0;
            if (st->kind == -1) {
              if (((int)GetRandomU32() & 0xFF) >= 0x80) {
                st->side = 1;
              } else {
                st->side = 0;
              }
            } else {
              st->side = st->kind;
            }
            *st->alert = 1;
            st->count = 0;
            if (actor->anim != 1) {
              actor->frameProgress = 8;
              actor->frameRate = 8;
              actor->anim = actor->nextAnim;
              actor->nextAnim = 1;
              actor->frame = actor->nextFrame;
              actor->nextFrame = 0;
              StubActorHook(actor);
            }
            actor->mode = 1;
            continue;
          }
          st->timer = 0x28;
        }
        break;
      }

      case 1: {
        int d;
        int step;
        int a;
        int target;
        int dz;
        int g1;
        int g2;

        d = ApproxDist2D(&actor->posX, &spy->posX);
        a = ArcTan2(spy->posX - actor->posX, SPY.posY - actor->posY, 0);
        step = 0xC;

        if (g_nActorAnimDone == 0 && actor->nextAnim == 1 && actor->nextFrame < 0xB) {
          break;
        }
        st->count += g_nFrameStep;
        actor->posZ += 0x5DC;
        target = FindGroundHeightBelow(&actor->posX, 0x1000) + 0x226;
        actor->posZ -= 0x5DC;
        if (st->kind == 2) {
          target = SPY.posZ + 0x12C;
        }
        if (d < 0xFA0) {
          step = 0x20;
        } else if (d < 0x1770) {
          step = 0x18;
        }
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 2);
        }
        if (st->speed < 0xAA) {
          st->speed += 0xA;
        }
        if (d < 0x4000 && st->side != 2) {
          st->weaving = 1;
        }
        if (st->weaving != 0) {
          if (st->swayDir == 1) {
            st->sway += step;
            if (st->sway >> 4 >= 0x23) {
              st->swayDir = -st->swayDir;
            }
          } else if (st->swayDir == -1) {
            st->sway -= step;
            if (st->sway >> 4 <= 0) {
              st->swayDir = 0;
            }
          }
        }
        dz = target - actor->posZ;
        if (ABS(dz) >= 0x47) {
          if (dz < 0) {
            actor->posZ -= 0x78;
          } else {
            actor->posZ += 0x78;
          }
        } else {
          actor->posZ = target;
        }
        if (d < 0x640 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) >= 0x3D) {
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, 0)) >= 0x51 &&
            actor->drawn == 0) {
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        if (st->side != 0) {
          actor->rotZ = WrapAddAngle8(a, st->sway >> 4);
        } else {
          actor->rotZ = WrapAddAngle8(a, -(st->sway >> 4));
        }
        if (MoveActorAlongHeading(actor, st->speed, 0, 0x15E, 1) != 0) {
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        if (st->count >= 0x3D && d < 0x2710 &&
            CheckSpyroLineOfSightTo(&actor->posX) == 0) {
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        if (d < 0x578 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x28 &&
            ABS2(actor->posZ - actor->floorOffset - spy->posZ) < 0x258) {
          SPY.damageFlags |= 1;
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        if (ApproxDist2D(&actor->posX, st->home) > (st->range << 10) + 0x800) {
          *st->alert = 0;
          actor->mode = 2;
          continue;
        }
        CopyVector(y304, &actor->posX);
        y304[0] += g_anCosineLut[actor->rotZ] >> 3;
        y304[1] += g_anSineLut[actor->rotZ] >> 3;
        y304[2] += 0x400;
        g1 = FindGroundHeightBelow(y304, 0x800);
        CopyVector(y304, &actor->posX);
        y304[0] += (g_anCosineLut[WrapAddAngle8(actor->rotZ, 0x80)] * 175) >> 10;
        y304[1] += (g_anSineLut[WrapAddAngle8(actor->rotZ, 0x80)] * 175) >> 10;
        y304[2] += 0x400;
        g2 = FindGroundHeightBelow(y304, 0x800);
        if (ABS2(g1 - g2) < 0x200) {
          actor->rotY = ArcTan2(0x578, g1 - g2, 0);
          actor->rotY = StepAngle8Toward(actor->rotY, 0, 0x20);
        }
        break;
      }

      case 2: {
        int a = ArcTan2(st->home[0] - actor->posX,
                              st->home[1] - actor->posY, 0);
        int d = ApproxDist2D(&actor->posX, st->home);
        int dz = st->home[2] - actor->posZ + 0x200;

        if (d > 0x1800) {
          dz = SPY.posZ - actor->posZ + 0x800;
        }
        if (ABS(dz) >= 0x3D) {
          if (dz >= 0) {
            actor->posZ += 0x64;
          } else {
            actor->posZ -= 0x64;
          }
        }
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 2);
        }
        if (st->kind == 2) {
          TurnActorYawTowardAngle(actor, a, 7, 0, 0);
        } else {
          TurnActorYawTowardAngle(actor, a, 2, 0, 0);
        }
        MoveActorAlongHeading(actor, st->speed, 0, 0, 0);
        if (d < 0xBB8 && st->speed >= 0x33) {
          st->speed -= 5;
        }
        if (d < 0x80) {
          ENTER_POSE(actor, 0);
        }
        break;
      }

      case 4:
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0, 0);
          st->speed -= 0x10;
        }
        CopyVector(y320, &actor->posX);
        y320[2] += 0x3E8;
        if (FindGroundHeightBelow(y320, 0x800) < actor->posZ) {
          actor->posZ -= 0x32;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 3);
      FindGroundHeightBelow(&actor->posX, 0x10000);
      EncodeCachedVecToActorDirCode(actor);
      break;
    }

    /* Wandering critter (L31Critter): roams its patch, and is knocked away
     * (or stunned, when charged) when struck. */
    case 238: {
      L31Critter *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 5 && actor->mode != 6) {
        st->knock = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        if (!(actor->flags & 0x10000)) {
          st->speed = 0x190;
        } else {
          st->speed = 0;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        if (st->speed != 0) {
          ENTER_POSE(actor, 5);
        }
        actor->mode = 6;
        ANIM_RESET(actor, 6);
        continue;
      }

      switch (actor->mode) {
      case 0:
        UpdateActorWander(actor, st);
        break;

      case 5:
        if (st->speed >= 0x10) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x2BC, 5);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }

      case 6:
        if (actor->anim == 6 && actor->frame < 8) {
          UpdateActorWander(actor, st);
        }
        if (g_nActorAnimDone != 0) {
          if (actor->anim != 6) {
            DespawnActorRecord(actor);
            continue;
          }
          ANIM_SET(actor, 7);
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
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
          SubtractVector(y320, at, &spy->posX);
          y320[2] = (y320[2] * 3) >> 2;

          if (VectorLength(y320, 1) < 0x440) {
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

            if (st->cutsceneId == st->reward) {
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
              int *look = &SPY.requestVelX;

              g_nSpyroRequestState = 3;
              CopyVector(look, &SPY.motionX);
              RShiftVector3(look, 6);
              g_nSpyroRequestVelocityZ = 0;

              if (SPY.requestVelX != 0 || g_nSpyroRequestVelocityY != 0) {
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

    /* Waypoint rider (RideState30): steps a closed path and calls out a voice
     * line per leg. */
    case 286: {
      RideState30 *st = actor->state;
      int voice = -1;

      actor->soundRange = st->volume;

      switch (st->pose) {
      case 0:
        voice = 6;
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->leg) {
        case 0:
          if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
            st->wait = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->leg = 1;
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
            st->leg = 0;
            st->path->cur = (st->path->cur + 1) % st->path->count;
            st->left = ComputeStepTowardPoint(
                &st->path->nodes[st->path->cur].x,
                &st->path->nodes[(st->path->cur + 1) % st->path->count].x,
                st->step, st->speed);
          }

          if (actor->sfxSlot == 0x7F && st->left == (st->left / 48) * 48) {
            voice = 0xE;
          }
          break;
        }
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          int r;

          st->wait = 0x258 - (GetRandomU32() & 0x7F);
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
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, 8, &actor->sfxSlot);
      }

      break;
    }

    /* Bouncing debris (BounceState). */
    case 288:
    case 289: {
      BounceState *st = actor->state;

      if (st->life == 0 || actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      actor->posX += st->vx;
      actor->posY += st->vy;
      st->vz -= 5;
      if (--st->slow == 0) {
        st->vx = 0;
        st->vy = 0;
        st->vz = 0;
      }
      if (actor->type == 0x120) {
        actor->rotX += st->rx;
        actor->rotY += st->ry;
        actor->rotZ += st->rz;
        if (st->vz < -0x80) {
          st->vz = -0x80;
        }
      } else {
        if (st->vz < -0x18) {
          st->vz = -0x18;
        }
        if (st->mode == 0) {
          actor->rotZ += 0xC;
        } else if (st->vz < 0) {
          {
            int u = actor->rotY;
            int sw = st->sway;

            if (u + sw == 0x20 || u + sw == 0xE0) {
              st->sway = -sw;
            }
          }
          actor->rotY += st->sway;
          actor->posY += st->sway;
          if (GetRandomU32() & 1) {
            actor->rotZ += 1;
          }
        }
      }
      actor->posZ += st->vz;
      st->life--;
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
      SubtractVector(y320, &spy->posX, &actor->posX);
      distance = VectorLength(y320, 0);
      if (distance < st->radius && y320[2] >= -0x3FF && y320[2] < st->spanZ &&
          (SPY.state == 0x11 || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->controlFlags = 0x80000000;
        SPY.pScriptedActor = actor;
        SPY.damageFlags |= 0x800;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
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

        y288[0] = (GetRandomU32() & 0xFE) - 0x7F;
        y288[1] = (GetRandomU32() & 0xFE) - 0x7F;
        y288[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(y288, y288, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, y288, (int *)1);
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
            svT[0] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[1] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(svT, svT, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, svT, (int *)1);
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

    /* Type 332: ambush plant. Rears up and bites when Spyro walks past. */
    case 332:      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0xA00) {
          actor->mode = 1;
        }
        break;

      case 1:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x380) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0x200
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
            SetSpawnAnchorFromActor(actor, actor->rotZ);
            actor->mode = 2;
            actor->anim = 1;
            actor->subMode = 0;
          }
        }
        break;

      case 2:
        actor->subMode += g_nFrameStep;
        if (actor->subMode >= 0x30) {
          actor->mode = 0;
          actor->anim = 0;
        }
        break;
      }
      break;

    /* Type 401: breakable rock. Shakes when struck; a charge shatters it
     * into rubble and dust and removes it. */
    case 401: {
      L31Rock *st = actor->state;

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
      }
      if (actor->flags & 0x80000) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 4;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          i++;
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 0xA;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
          i++;
        }
        g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);
        if ((actor->flags & 0x30000) && st->shake == 0) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }
      actor->flags = 0;
      break;
    }

    /* Type 402: cannon. Smokes while it builds up, fires, then flies its
     * ball along a path to the target, waking the linked actor. */
    case 402: {
      L25Shot *st = actor->state;

      if (st->init == 1) {
        int pitch;

        SubtractVector(y352, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(y352, 0), y352[2], 0);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < 0x60) {
          actor->rotY = 0xE0;
        }
        actor->rotZ = ArcTan2(y352[0], y352[1], 0);
        RefreshActorDerivedState(actor, 4);
        st->init = 2;
      }

      switch (actor->subMode) {
      case 0:
        if (st->link >= 0 && g_pActorListBase[st->link].mode >= 0x80) {
          DespawnActorRecord(actor);
          continue;
        }
        if (!(actor->flags & 0x10000)) {
          continue;
        }
        actor->subMode = 1;
        actor->updateDist = 0xFF;
        actor->cullRadius = 0x7F;
        continue;

      case 1:
        st->timer += g_nFrameStep;
        if (st->timer < 0x5A) {
          y368[0] = 0;
          y368[1] = 0;
          y368[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, y368, y368);
          AddVector(y368, y368, &actor->posX);
          y384[0] = 0;
          y384[1] = 6;
          y384[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, y384, y384);
          y384[0] += (GetRandomU32() & 0xF) - 8;
          y384[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, 0x4A, y368, y384);
          if (actor->sfxSlot != 0x7F) {
            continue;
          }
          PlayActorMeshVoice(actor, 0, 0);
          continue;
        }
        if (st->timer < 0xB4 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          y368[0] = 0;
          y368[1] = 0;
          y368[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, y368, y368);
          AddVector(y368, y368, &actor->posX);
          y384[0] = 0;
          y384[1] = ((st->timer - 0x5A) >> 3) + 9;
          y384[2] = -0x12 - ((st->timer - 0x5A) >> 2);
          RotateVectorByMatrix(actor->mtx, y384, y384);
          g_pfnLevelEmitParticles(1, 0, y368, y384);
          if (actor->rotX < 0x80) {
            actor->rotX = -((st->timer - 0x5A) >> 5);
          } else {
            actor->rotX = (st->timer - 0x5A) >> 5;
          }
          continue;
        }
        if (st->timer <= 0xB4) {
          continue;
        }
        actor->subMode = 2;
        actor->cullRadius = 0x40;
        actor->updateDist = 0x40;
        st->path[1] = 1;
        st->timer = 0;
        ZeroVector(st->vel);
        PlayActorMeshVoice(actor, 1, 0);
        continue;

      case 2:
        if (st->timer == 4) {
          SubtractVector(y400, PATH_PT(st->path, st->path[1]), &actor->posX);
          CopyVector(st->vel, y400);
          st->path[1]++;
          if (st->path[1] == st->path[0]) {
            Actor *target = &g_pActorListBase[st->link];

            if (st->link > 0) {
              target->updateDist = 0xFF;
              target->flags |= 0x80000;
            }
            DespawnActorRecord(actor);
          }
          st->timer = 0;
        } else {
          if (st->timer == 3) {
            SubtractVector(y400, PATH_PT(st->path, st->path[1]), &actor->posX);
            RShiftVector3(y400, 1);
          } else {
            SubtractVector(y400, PATH_PT(st->path, st->path[1]),
                          (int *)(st->path + ((st->path[1] << 4) - 8)));
            ScaleVector3Sat(y400, y400, g_anSineLut[st->timer * 2 - 6]);
            ScaleVector3Sat(y416, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(y400, y400, y416);
            RShiftVector3(y400, 0xA);
          }
          st->timer++;
        }
        VectorLength(y400, 1);
        actor->rotZ = ArcTan2_8bit(y400[0], y400[1]);
        actor->rotY = -ArcTan2_8bit(y400[2], VectorLength(y400, 0));
        AddVector(&actor->posX, &actor->posX, y400);
        ZeroVector(y400);
        g_pfnLevelEmitParticles(1, 0, &actor->posX, y400);
        continue;
      }
      continue;
    }

    /* Balloon: bobs about its start height. */
    case 416: {
      int *base = actor->state;

      if (actor->mode == 0) {
        *base = actor->posZ;
        actor->mode = 1;
        break;
      }

      actor->posZ = *base + (g_anSineLut[actor->subMode] >> 4);
      actor->subMode += g_nFrameStep;
      break;
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
          st->passenger->otBias = 5;

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
        ((unsigned char *)rider->state)[0xE] = one;
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
        y416[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        y416[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        y416[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, y416);
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
          y416[0] = (g_anCosineLut[bearing] * 3) >> 4;
          y416[1] = (g_anSineLut[bearing] * 3) >> 4;
          y416[2] = 0;
          y432[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          y432[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          y432[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(y448, y432, st->radius - 1);
          LShiftVector3(y432, 1);
          arm = st->radius - 1;
          y448[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          y464[0] = y416[0] * g_anCosineLut[arm * 2];
          y464[1] = y416[1] * g_anCosineLut[arm * 2];
          y464[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(y432, y432, st->phase);
          SubtractVector(y448, y448, y432);
          swing = &g_anCosineLut[bearing];
          actor->posX = y416[0] * *swing;
          actor->posY = y416[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, y464);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, y416);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, y448);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }
    

    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 234:
    case 363:
    case 364:
    case 493:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;
}
  }
}
