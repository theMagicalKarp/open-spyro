/* Level17_UpdateActors (0x8007CFB4, 20,312 B) -- matching notes.
 *
 * Sibling of Level11_UpdateActors (the flight actor-update family head);
 * templated from it arm for arm. See the level_11 file for the load-bearing
 * source forms shared by the whole family. Differences from the head:
 *   - new arms 0x11 (drift shard), 0x134 (roller) and 0x1E1..0x1E3 (its
 *     embers); 0x190 joins the 405/477 arm; 0x189 replaces 0x1EB in the 0x183
 *     arm; the 0x18D/0x18F trigger arm is gone.
 *   - `svX` is the first scratch vector (0x11's), the embers use a
 *     block-local `trail[3]`.
 *   - 0x134: the waypoint scan addresses `st->path + ((i << 4) + 8)` so the
 *     node offset is its own giv; `d` holds the old heading and then the
 *     delta, one variable.
 *   - 0x159 case 0 needs an empty `do { } while (0);` ahead of the
 *     TurnActorYawTowardAngle call: without it cse folds `spy->posX` to the absolute
 *     address (the extra code in this overlay changes what cse can see).
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"

/* Level17_UpdateActors -- per-frame actor update for level 11 (Night Flight).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Layout of this file:
 *   Core records      Actor, the per-type Model table, waypoint paths
 *   Per-class blocks  one state struct per actor class, each followed by the
 *                     globals and callees its case uses, in roughly case order
 *   Shared objects    the camera (CAM) and Spyro (SPY), each declared as one
 *                     aggregate over its whole address range
 *   Helper macros     absolute value and the three animation idioms
 *   The function      update-list preamble, then the per-class dispatch
 */

/* Node accessors for paths held as byte pointers (type 286). */
#define PATH_X(p, i) (*(int *)((p) + ((i) << 4) + 0x8))
#define PATH_Y(p, i) (*(int *)((p) + ((i) << 4) + 0xC))
#define PATH_Z(p, i) (*(int *)((p) + ((i) << 4) + 0x10))

/* Type 286: ambient sound source; mode 2 also coasts along a path. */


/* Progress-per-tick byte of animation `anim` in the actor's model. */
#define TYPE_DESC(a, id)                                                     \
  (g_apActorMeshTable[(a)->type]->m_Animations[(id)]->m_ProgressPerTick)

/* Step a countdown timer; non-zero when it fires. */

/* Mark an item collected; the second argument is the actor or its id. */

extern void SteerActorAroundPoint(Actor *actor, int *target, int a, int heading, int b,
                          int c, int d, int e, int f, int g, int h, int i,
                          int j);


#define AS_I32(sym) (((I32 *)&(sym))->v)

#define AS_PTRU8(sym) (((PtrU8 *)&(sym))->v)

/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */


/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */

#define CAM g_anWorldToCameraRotMtx

extern int FindGroundHeightBelow(int *v, int k);
extern int GetGroundTriggerRegionType(int *pos);

/* Glow node owned by Sparx; follows its anchor position. */


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
  (a)->frameProgress = 0x10;                                                           \
  (a)->frameRate = 0x10;                                                           \
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
    (a)->frameProgress = 0x10;                                                         \
    (a)->frameRate = 0x10;                                                         \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    StubActorHook(a);                                                          \
  }                                                                            \
  continue

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks (splice leftovers) ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */

/* Case 34 — the vortex/whirlwind pad that lifts Spyro out of the level. */
typedef struct VortexState {
  int idx;               /* 0x00 */
  int centre[3];         /* 0x04 pad centre */
  int apex[3];           /* 0x0A (short-aligned) rise target */
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
  signed char bounce;   /* 0x10 remaining knock-back bounces */
  signed char lean;     /* 0x11 body pitch */
  signed char roll;     /* 0x12 */
  unsigned char spin;   /* 0x13 */
  unsigned char attach; /* 0x14 attached smoke/spark handle, 0xFF = none */
} EnemyState;

/* Cases 255/256, 310/311, 257, 309 — debris shards thrown by a break. */
typedef struct ShardState {
  short vel[3];        /* 0x00 */
  signed char spin[3]; /* 0x06 pitch / yaw / roll per frame */
  char pad09[0x3];
  int life;  /* 0x0C */
  int floor; /* 0x10 */
} ShardState;

/* Case 398 — the level-exit vortex: drives the scripted camera and hands the
 * game over to the world map. */
typedef struct ExitState {
  PortalDesc *portal; /* 0x00 */
  int centre[3];      /* 0x04 */
  int dest;           /* 0x14 */
  int preset;         /* 0x18 */
} ExitState;

extern unsigned char g_bSpyroPersistentEulerZ; /* SPY.bodyRotZ, absolute form (A240) */
extern int g_nLevelReadyFlag; /* balloon sequence stage (== SPY.u164) */


/* ==== Flight-level classes ==== */

/* Flight targets (0x154 / 0x161): a helper actor spawned alongside, and the
 * sound to play when Spyro flies through. */
typedef struct TargetState {
  int pad00;
  int pad04;
  int pad08;
  int pad0C;
  Actor *helper;  /* 0x10 */
  int helperType; /* 0x14, spawned as helperType + 0x158 */
  int sound;      /* 0x18 */
} TargetState;

/* Particle burst shards spawned by the 0x161 target. */
typedef struct BurstState {
  int posX, posY, posZ; /* 0x00 velocity */
  signed char spinX;    /* 0x0C */
  signed char spinY;
  signed char spinZ;
  char pad0F;
  int life;             /* 0x10 */
} BurstState;

extern int g_nVblankTickCount;    /* frame counter */
extern void TallyFlightTarget(int sound);
extern void ProjectWorldPointGTEUnscaled(int *d, int *s);

/* HUD tally icon flight (0x183..0x185 / 0x1EB): pivot, radius, angle and the
 * swept phase. */
typedef struct TallyState {
  int x;        /* 0x00 pivot */
  int y;        /* 0x04 */
  int z;        /* 0x08 */
  short radius; /* 0x0C */
  short angle;  /* 0x0E */
  short phase;  /* 0x10 */
} TallyState;

/* Flight HUD controller (0x166): clock digits, the per-class tally icons,
 * icons being spent into a slot, and the four finish slots. */
typedef struct HudState {
  int time;         /* 0x00 starting clock, in ticks */
  Actor *digit[8];  /* 0x04 clock digits; 4..7 only in time attack */
  Actor *icon[8];   /* 0x24 tally row */
  Actor *spent[8];  /* 0x44 a completed row flying to its slot */
  Actor *slot[4];   /* 0x64 finish slots */
  int types[4];     /* 0x74 icon actor type per class */
  int pad84[4];     /* 0x84 */
  int timer;        /* 0x94 */
  int count;        /* 0x98 icons still to spend */
  int hurry;        /* 0x9C under a minute left */
} HudState;

extern void ChangeSpyroState(int a);

/* ==== The function ==== */
/* Type 0x11: a drifting shard. Moves by ->vel until its timer runs out or
 * it hits something. */
typedef struct DriftState {
  int vel[3]; /* 0x00 */
  int timer;  /* 0x0C */
} DriftState;

/* Type 0x134: a path walker that rolls with its turns and, when flamed,
 * spawns its reward and three embers and runs off. */
typedef struct RollerState {
  unsigned char *path; /* 0x00 [0]=node count, [1]=nearest node */
  int reward;          /* 0x04 reward actor type - 0x158 */
  int tally;           /* 0x08 */
  int roll;            /* 0x0C */
  int rollVel;         /* 0x10 */
  int zVel;           /* 0x14 passed by address to DriveActorTrackedMove */
  int heading;         /* 0x18 */
  int moveSpeed;           /* 0x1C passed by address to DriveActorTrackedMove */
  int legVec[3];        /* 0x20 */
  int legStep;           /* 0x2C */
  int ready;           /* 0x30 nearest node found */
} RollerState;

extern void TweenActorAlongPath(Actor *actor, unsigned char *path, int *a, int *b,
                          int c, int d);

/* Types 0x1E1..0x1E3: the walker's embers. */
typedef struct EmberState {
  short vel[3];  /* 0x00 */
  short spin[3]; /* 0x06 */
  int life;      /* 0x0C */
} EmberState;

void Level17_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  char *base;

  /* Scratch vectors shared by several cases. */
  int svX[3];
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
  int svT[3];
  int svU[3];
  int svV[3];
  int svW[3];
  int sh6;

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  sh6 = 6;
  base = (char *)&g_nLevelReadyFlag;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    one = 1;
    spy = (SpyroObj *)(base - 0x164);

    if (actor->mode >= 0x80) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    g_nActorFrameDelta = g_nFrameStep;

    switch (actor->type) {
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

              SPY.u164 = one;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pLevelOverlayInitRecord->subMode != 99) {
            ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) < 0) {
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

      if (actor->cullRadius & 0x80)
        break;

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
    case 0x11: {
      DriftState *st = actor->state;

      if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
        AddVector(svX, &actor->posX, st->vel);
        if (CastRayWorldAndActors(&actor->posX, svX) == 0 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0, 1) == 0) {
          CopyVector(&actor->posX, svX);
          break;
        }
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Dragon egg (ShotState). */
    case 34: {
      ShotState *st;

      st = (ShotState *)actor->state;

      if (actor->mode != 0 && actor->mode < 5) {
        SPY.controlFlags = 0x80002000;
        if (SPY.state != 0 && SPY.state != 0xC) {
          actor->mode = 5;
        }
      }
      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->from, &actor->posX);
        st->heading =
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
            0x40;
        st->speed = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode = actor->mode + 1;
        break;

      case 2:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed + step;
          st->heading = st->heading + step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 3);
          }
        } else {
          CopyVector(svA, &spy->posX);
          {
            int c = LookupCosine(g_bSpyroPersistentEulerZ << 4);
            int s = LookupSine(g_bSpyroPersistentEulerZ << 4);
            int cr = c * 724;
            int sr = s * 724;
            svA[0] = svA[0] + ((sr + cr) >> 12);
          }
          {
            int s = LookupSine(g_bSpyroPersistentEulerZ << 4);
            int c = LookupCosine(g_bSpyroPersistentEulerZ << 4);
            int sr = s * 724;
            int cr = c * 724;
            svA[1] = svA[1] + ((sr - cr) >> 12);
          }
          svA[2] = svA[2] + 0x300;
          VectorToSVectorShr2(st->to, svA);
          VectorToSVectorShr2(st->from, &actor->posX);
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 3:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x40) {
          SVectorToVectorShl2(svB, st->from);
          SVectorToVectorShl2(svC, st->to);
          SubtractVector(svC, svC, svB);
          ScaleVector3Sat(svC, svC, st->phase);
          RShiftVector3(svC, 6);
          AddVector(&actor->posX, svB, svC);
          actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
        } else {
          CopyVector(svD, &spy->posX);
          svD[0] = svD[0] + (LookupCosine(g_bSpyroPersistentEulerZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(g_bSpyroPersistentEulerZ << 4) >> 3);
          svD[2] = svD[2] + 0x300;
          VectorToSVectorShr2(st->from, svD);
          st->heading = g_bSpyroPersistentEulerZ - 0x40;
          st->speed = 0x200;
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 4:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed - step;
          st->heading = st->heading - step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 2);
          }
        } else {
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & 0x40) != 0) {
          actor->mode = 5;
        }
        break;

      case 5:
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, 0x4D, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&SPY.headLook);
        CommitActorKillGems(0, st->killActor);
        g_nWorldEggsRecovered = g_nWorldEggsRecovered + 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        break;

      default:
        break;
      }
      break;
    }

    /* Bouncing chest fragments (SparkState). */
    case 478:
    case 479: {
      SparkState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
          
          int dot;

          SetVector3Magnitude(&g_anCollisionProbeVec, 0x1000);
          dot = st->vel[0] * g_anCollisionProbeVec + st->vel[1] * g_nCollisionProbeVecY +
                    st->vel[2] * g_nCollisionProbeVecZ >>
                11;
          if (dot < 0) {
            ScaleVector3ByRatio(&g_anCollisionProbeVec, 0x1000, (dot >> 2) - (dot));
            st->vel[0] += g_anCollisionProbeVec;
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -128)
          st->vel[2] = -128;
        actor->posZ += st->vel[2];

        actor->rotX += st->spin0;
        actor->rotY += st->spin1;
        actor->rotZ += st->spin2;
        if ((st->life & 3) == 0) {

          svD[0] = GetRandomU32() & 3;
          svD[1] = GetRandomU32() & 3;
          svD[2] = 0x14;
          g_pfnLevelEmitParticles(1, 1, &actor->posX, svD);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Falling chest fragments (TumbleState). */
    case 480: {
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
    case 14:
    case 15:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87: {
      PickupState *st = (PickupState *)actor->state;

      int dist = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != 15) {

        if (st->anim != 255) {
          RefreshActorDerivedState(actor, 4);

          if (actor->type == 14) {
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

      if (!actor->drawn && dist > 0x2000 && actor->subMode == 1 && st->sub == 0) {
        break;
      }

      switch (actor->subMode) {
      case 0: {
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

        actor->subMode = 1;
        break;
      }
      case 1: {
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

            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;

            RefreshActorDerivedState(actor, 2);

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

            if (g_pLevelOverlayInitRecord != 0) {

              if (SPY.u09C == 0) {

                int res = CheckLineOfSightSegment(&g_pLevelOverlayInitRecord->posX, &actor->posX);

                if (res) {
                  ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

                  if (cs->owner == 0) {
                    if (!GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                       g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0])) {
                      g_nSfxOverrideFlags = 1;
                      g_nSfxOverrideVolR = 0x2000;
                      g_nSfxOverrideVolL = 0x2000;
                      PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                    }

                    g_pLevelOverlayInitRecord->subMode = 4;
                    cs->owner = actor;
                  }
                }
              }
            }
          }
        }

        break;
      }
      case 2: {
        int speed;

        ScaleVector3Sat(svG, st->vel, g_nFrameStep);
        RShiftVector3(svG, 1);

        speed = VectorLength(svG, 1);

        if (220 < speed) {
          ScaleVector3ByRatio(svG, speed, 220);
        }

        AddVector(svG, &actor->posX, svG);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (svG[2] < 0) {
          if (actor->type != 14 && actor->type != 15) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          int r;
          register int *pos asm("$4");

          svG[2] += 240;

          r = CollideSphereWithWorldAndActors(svG, 240, 240);
          pos = &actor->posX;
          if (r) {
            if (GetGroundTriggerRegionType(pos) == 0) {
              if (actor->type != 14 && actor->type != 15) {
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
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

              if ((svG[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = 1;
                CopyVector(&actor->posX, svG);
                actor->posZ = groundHeight;

                if (actor->type != 0xF) {

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

              PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

              if (ReflectVectorOffNormal(st->vel, svH, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            svG[2] -= 240;
            CopyVector(pos, svG);
            FindGroundHeightBelow(svG, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }
        }

        if (dist < 1434) {
          int zDist = (((int *)&SPY.posX)[2] - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pLevelOverlayInitRecord != 0) {

              if (SPY.u09C == 0 && st->count < 3) {

                ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

                if (cs->owner == 0) {
                  if (!GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                     g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0])) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);
                  }

                  g_pLevelOverlayInitRecord->subMode = 4;
                  cs->owner = actor;
                }
              }
            }
          }
        }

        break;
      }
      case 3: {
        if (st->timer >= 32) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(svI, &spy->posX, st->vel);
          RShiftVector3(svI, 5);

          if (VectorLength(svI, 1) > 480) {
            CopyVector(&actor->posX, &spy->posX);
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

      case 4: {
        continue;
      }
      }
      if (actor->subMode < 3) {
        if (actor->type == 14) {
          actor->rotX = (g_anCosineLut[st->roll] >> 9) + st->pitch;
          actor->rotY = (g_anSineLut[st->roll] >> 9) + st->yaw;
          st->roll += g_nFrameStep << 1;
        } else if (actor->type == 15) {
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
          (actor->subMode == 3 && st->timer > 64)) {

        if (g_pLevelOverlayInitRecord != 0) {
          ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == 14) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
          RefreshActorDerivedState(actor, 4);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)8);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)0x10);

          if (g_nExtraLives < 99) {
            g_nExtraLives++;
          }

          CommitActorKillGems(0, actor);
          DespawnActorRecord(actor);
          continue;
        } else if (actor->type == 15) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
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

    /* Sparx (FlierState). */
    case 120: {
      FlierState *st = (FlierState *)actor->state;
      char pitchAngle;

      if (*(int *)base >= 2) {

        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(actor->mtx, svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);

        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
      }

      if (SPY.u164 >= 3) {
        if (st->fx == 0) {
          FxNode *g = AllocWorldSparkle();
          st->fx = g;
          if (g != 0) {
            g->anchor = &actor->posX;
            st->fx->size = 0x40;
            st->fx->offX = 0;
            st->fx->offY = 0;
            st->fx->offZ = 0;
            st->fx->r = 0xC0;
            st->fx->g = 0xC0;
            st->fx->b = 0x60;
            st->fx->kind = 9;
            st->fx->desc = &g_anSparkleGlowDesc;
          }
        } else {
          st->fx->size += 0x20;
          if (st->fx->size > 0x400) {
            st->fx->size = 0x400;
          }
        }
      } else {
        if (st->fx != 0) {
          ClearWord(st->fx);
          st->fx = 0;
        }
      }

      if (SPY.u164 <= 0) {
        Actor *target = st->rider;
        if (target != 0 && target->type == 16) {
          target->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        break;
      }

      if (st->rider != 0 && ApproxDist2D(&actor->posX, &SPY.posX) > 8192) {
        st->timer = 0;
      }

      if (actor->subMode == 99)
        break;

      if (st->rider != 0) {
        switch (actor->subMode) {
        case 0: {
          Actor *target = st->rider;
          ChaseState *bp = target->state;
          int isPickupAnimPhase = actor->nextAnim & 1;
          int dist;
          int targetSpeed;

          CopyVector(svA, &target->posX);
          if (isPickupAnimPhase == 0) {
            svA[0] -= g_anCosineLut[st->rider->rotZ] >> 4;
            svA[1] -= g_anSineLut[st->rider->rotZ] >> 4;
          } else {
            svA[0] -= g_anCosineLut[st->rider->rotZ] >> 6;
            svA[1] -= g_anSineLut[st->rider->rotZ] >> 6;
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

          SubtractVector(svA, svA, &actor->posX);
          dist = VectorLength(svA, 1);

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
            ScaleVector3ByRatio(svA, dist, targetSpeed + 5);
          } else {
            ScaleVector3ByRatio(svA, dist, targetSpeed - 5);
          }
          AddVector(&actor->posX, &actor->posX, svA);
          TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], 0), 0xA, 0, 0);
          pitchAngle = ArcTan2(VectorLength(svA, 0), svA[2], 0);
          actor->rotY = pitchAngle;
          actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          break;
        }
        case 1: {
          SPY.u164++;
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
          actor->subMode++;
          DespawnActorRecord(st->rider);
          break;
        }
        case 2: {
          if (g_nActorAnimDone != 0) {
            if (actor->nextAnim != actor->nextAnim - 1) {
              g_nActorAnimDone = 0;
              ANIM_ADVANCE(actor, actor->nextAnim - 1);
            }
            st->rider = 0;
            actor->subMode = 0;
          }
          break;
        }
        case 4: {
          PickupState *cp = st->rider->state;
          int dist;

          SubtractVector(svJ, &st->rider->posX, &actor->posX);
          dist = VectorLength(svJ, 1);

          if (dist < 512) {
            cp->timer = 0;
            cp->pitch = GetRandomU32() & 0xE;
            cp->yaw = GetRandomU32() & 0xE;
            cp->roll = GetRandomU32() & 0xE;

            CopyVector(cp->vel, &st->rider->posX);
            st->rider->subMode = 3;

            if (st->rider->type == 14)
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_dwBurstColorType14);
            else if (st->rider->type == 15)
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_dwBurstColorType15);
            else
              g_pfnLevelEmitParticles(1, 0xC, (int *)actor, g_adwActorBurstColors[st->rider->type]);

            st->rider = 0;
            if (!GetSoundVoiceStatusByOwner(actor, AS_PTRU8(g_pLevelSampleBankHeader)[0x2F])) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolR = 0x3600;
              g_nSfxOverrideVolL = 0x3600;
              PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);
            }
          } else {
            int dcheck = 310;

            if (dcheck < dist)
              ScaleVector3ByRatio(svJ, dist, 310);
            else
              ScaleVector3ByRatio(svJ, dist, 290);

            AddVector(&actor->posX, &actor->posX, svJ);
            TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
            pitchAngle = ArcTan2(VectorLength(svJ, 0), svJ[2], 0);
            actor->rotY = pitchAngle;
            actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          }
          break;
        }
        }
      } else {

        actor->cullRadius = 0x10;
        if (st->timer <= 0) {

          svF[0] = -0x258;
          svF[1] = (GetRandomU32() & 0x310) - 0x188;
          if (CAM.state == 3) {
            svF[2] = (GetRandomU32() & 0x7F) + 0x64;
          } else {
            svF[2] = (GetRandomU32() & 0x1FF) - 0xC8;
          }
          st->vx = svF[0];
          st->vy = svF[1];
          st->vz = svF[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == 3) {
            svF[0] = st->vx + (SPY.u118 >> 2);
            svF[1] = st->vy;
            svF[2] = st->vz;
            if (svF[2] < 0x64)
              svF[2] = 0x64;
          } else {
            if (CAM.state == 0x80000009)
              svF[0] = st->vx - 512;
            else {
              svF[0] = st->vx;
            }
            svF[1] = st->vy;
            svF[2] = st->vz;
          }
          RotateVectorByMatrix(&SPY.u034, svF, svF);
          AddVector(svF, svF, &SPY.posX);
          SubtractVector(svF, svF, &actor->posX);
          RShiftVector3(svF, 2);
          AddVector(&actor->posX, &actor->posX, svF);

          if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
          }

          if (svF[2] > 32)
            svF[2] = 32;
          if (svF[2] < -32)
            svF[2] = -32;
          actor->rotX = 0;
          actor->rotY = svF[2];
          TurnActorYawTowardAngle(actor, SPY.bodyRotZ, 4, 0, 0);
        }

        switch (SPY.u164) {
        case 1:
          if (actor->nextAnim != 0) {
            if (actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
          }
          break;
        case 2:
          if (actor->nextAnim != 2) {
            if (actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
          }

          break;
        case 3:
          if (actor->nextAnim != 4) {
            if (actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
          }
          break;
        }
      }
      RefreshActorDerivedState(actor, 4);
      break;
    }

    /* Key (SpinState). */
    case 173: {
      SpinState *st;
      int dz;
      int ang;
      int i;

      st = (SpinState *)actor->state;
      if (actor->subMode != 0) {
        continue;
      }

      actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
      actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
      st->angle = st->angle + g_nFrameStep * 2;

      if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < 0x200
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
          RefreshActorDerivedState(actor, 4);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)(0x8080 + 0x1000000 * i));
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

      if (st->anim != 0xFF) {
        RefreshActorDerivedState(actor, 4);
        RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->anim].pos);
        AddVector(g_aRotBillboards[st->anim].pos, g_aRotBillboards[st->anim].pos,
                      &actor->posX);
        if (g_aRotBillboards[st->anim].life < 5) {
          st->anim = 0xFF;
        }
      } else if (st->timer >= 0xF8) {
        ang = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);
        if (ang >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
          st->anim = ang;
        }
        st->timer = (GetRandomU32() & 0x3F) + 0x18;
      }

      st->timer = st->timer + g_nFrameStep;
      continue;
    }

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
              
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(&g_anCollisionProbeVec, 0x1000);

              dot = (st->vel[0] * g_anCollisionProbeVec + st->vel[1] * g_nCollisionProbeVecY +
                     st->vel[2] * g_nCollisionProbeVecZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(&g_anCollisionProbeVec, 0x1000, -dot);
                st->vel[0] += g_anCollisionProbeVec;
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

    /* Flight-level chest: flaming or charging it frees its reward and
     * scatters shards; otherwise it hums its ambient sound. */
    case 299: {
      int *st = actor->state;
      int i;

      if (actor->flags & 0xB0000) {
        Actor *reward;

        PlayActorMeshVoice(actor, 0, 0);
        TallyFlightTarget(st[1]);
        reward = g_pfnLevelSpawnActor(st[0] + 0x158, actor);
        if (reward != 0) {
          reward->mode = 1;
        }

        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x1DE, actor);
          g_pfnLevelSpawnActor(0x1DF, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x1E0, actor);
        }

        g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
        DespawnActorRecord(actor);
        break;
      }

      {
        Model **models = g_apActorMeshTable;

        if (GetSoundVoiceStatusByOwner(actor, g_pLevelSoundIds[5]) == 0 && g_nGamestate == 0) {
          PlaySoundEffect(models[actor->type]->m_Sounds[1], actor, 8,
                        &actor->sfxSlot);
        }
      }
      break;
    }
    case 0x134: {
      RollerState *st = actor->state;

      if (st->ready == 0) {
        int best = 30000;
        int i;

        for (i = 0; i < st->path[0]; i++) {
          int d = ApproxDist2D(&actor->posX, (int *)(st->path + ((i << 4) + 8)));

          if (d < best) {
            best = d;
            st->path[1] = i;
          }
        }
        st->ready = 1;
      }

      if ((actor->flags & 0x90000) && actor->mode == 0) {
        Actor *reward;
        int i;

        reward = g_pfnLevelSpawnActor(st->reward + 0x158, actor);
        if (reward != 0) {
          reward->mode = 1;
        }
        TallyFlightTarget(st->tally);
        st->zVel = 0;
        st->moveSpeed = 0x15E;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x28, 0x40);
        for (i = 0; i < 3; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x1E1, actor);
          g_pfnLevelSpawnActor(0x1E2, actor);
          g_pfnLevelSpawnActor(0x1E3, actor);
        }
        PlayActorMeshVoice(actor, 0, 0);
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        break;
      }

      switch (actor->mode) {
      case 0: {
        int d = actor->rotZ;

        if (g_nActiveLevelId == 0xF || g_nActiveLevelId == 0x2D) {
          TweenActorAlongPath(actor, st->path, st->legVec, &st->legStep, 0xC, 4);
        } else {
          TweenActorAlongPath(actor, st->path, st->legVec, &st->legStep, 0x16, 0);
        }
        d = (actor->rotZ - d) & 0xFF;
        if (d > 0x80) {
          d -= 0x100;
        }
        st->rollVel += ((-d << 8) - (st->rollVel << 3) - st->roll) >> 6;
        st->roll = (st->roll + st->rollVel) & 0xFFF;
        if (st->roll > 0x800) {
          st->roll -= 0x1000;
        }
        if (st->roll < -0x200) {
          st->roll = -0x200;
        }
        if (st->roll > 0x200) {
          st->roll = 0x200;
        }
        actor->rotX = st->roll >> 4;
        break;
      }
      case 1:
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zVel, 0xC,
                          0xC) == 3) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0x1D], actor, 8, &actor->sfxSlot);
          g_pfnLevelSpawnActor(0x190, actor);
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = 2;
          ANIM_RESET(actor, 2);
        }
        break;
      case 2:
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zVel, 0xC,
                          0xC) == 3) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0x1D], actor, 8, &actor->sfxSlot);
          g_pfnLevelSpawnActor(0x190, actor);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }
    /* Flight-level target (ring/chest) that hands a helper to its owner and
     * pays out when Spyro flies through it. */
    case 0x154: {
      TargetState *st = actor->state;

      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] == 0 && st->helper == 0) {
        st->helper = g_pfnLevelSpawnActor(st->helperType + 0x158, actor);
        actor->rotX = -actor->anim << 6;
      }

      actor->rotX = WrapAddAngle8(actor->rotX, 8);
      if ((actor->rotX) < 8) {
        PlayActorMeshVoice(actor, 0, 0);
      }

      RefreshActorDerivedState(actor, 4);

      if (ApproxDist2D(&actor->posX, &spy->posX) < 3000) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < 3000 : SPY.posZ - height < 3000) {
          SubtractVector(svT, &actor->posX, &spy->posX);
          if (VectorLength(svT, 1) < 2500 &&
              PlaneDistanceAbs(&spy->posX, st) < 0x200) {
            TallyFlightTarget(st->sound);
            if (st->helper != 0) {
              st->helper->mode = 1;
            } else {
              PlaySoundEffect(g_pLevelSampleBankHeader[0x23], actor, 0x10, 0);
            }
            DespawnActorRecord(actor);
          }
        }
      }
      break;
    }
    /* Time-bonus pickup: turns to face Spyro, then flies to the HUD corner
     * and adds (type - 0x158) seconds when it arrives. */
    case 0x159:
    case 0x15A:
    case 0x15B: {
      int *st = actor->state;

      switch (actor->mode) {
      case 0:
        if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0) {
          DespawnActorRecord(actor);
          break;
        }
        do {
        } while (0);
        TurnActorYawTowardAngle(actor,
                      WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                  spy->posY - actor->posY, 0),
                                    0x40),
                      4, 0, 0);
        break;
      case 1:
        g_nFlightTimeBonus += (actor->type - 0x158) * 60;
        ProjectWorldPointGTEUnscaled(&actor->posX, &actor->posX);
        actor->rotX = 0;
        actor->rotY = 0;
        actor->rotZ = 0xC0;
        actor->cullRadius = 0xFF;
        actor->otBias = 0x20;
        actor->mode = 2;
        PlaySoundEffect(g_pLevelSampleBankHeader[0x23], actor, 0x10, 0);
        break;
      case 2: {
        int d;

        svU[0] = 0x1A4;
        svU[1] = 0x30;
        svU[2] = 0x157C;
        SubtractVector(svU, svU, &actor->posX);
        d = VectorLength(svU, 1);
        if (d < 0x20) {
          actor->mode = 3;
          actor->posX = 0x1A4;
          actor->posY = 0x30;
          actor->posZ = 0x157C;
          *st = 0x1E;
        } else {
          if (d > 0x80) {
            ScaleVector3ByRatio(svU, d, 0x80);
          }
          AddVector(&actor->posX, &actor->posX, svU);
        }
        break;
      }
      case 3:
        if (TickCountdownByFrameDelta(st, 4) != 0) {
          g_nFlightTimeRemaining += (actor->type - 0x158) * 60;
          PlaySoundEffect(g_pLevelSampleBankHeader[0x22], actor, 0x10, 0);
          DespawnActorRecord(actor);
        }
        break;
      }
      break;
    }
    /* Breakable flight target: bursts into 12 shards when Spyro flies
     * through, and otherwise pulses its glow with the frame counter. */
    case 0x161: {
      TargetState *st = actor->state;

      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] == 0 && st->helper == 0) {
        st->helper = g_pfnLevelSpawnActor(st->helperType + 0x158, actor);
      }

      if (ApproxDist2D(&actor->posX, &spy->posX) < 3000) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < 4000 : SPY.posZ - height < 4000) {
          SubtractVector(svV, &actor->posX, &spy->posX);
          svW[2] = (svV[2] - 0xC0) * 4;
          svV[2] = (svW[2] - svV[2]) >> 2;
          if (VectorLength(svV, 1) < 0x578 &&
              PlaneDistanceAbs(&spy->posX, st) < 0x200) {
            int i;

            TallyFlightTarget(st->sound);
            if (st->helper != 0) {
              st->helper->mode = 1;
            }

            for (i = 0; i < 13; i++) {
              Actor *shard;
              BurstState *ss;

              if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
                break;
              }
              shard = g_pfnLevelSpawnActor(i + 0x176, actor);
              ss = shard->state;
              RotateVectorByMatrix(actor->mtx, &g_anActorShardOffsets[i * 3], svV);
              AddVector(&shard->posX, &actor->posX, svV);
              RShiftVector3(svV, 5);
              svW[0] = (GetRandomU32() & 0x7E) - 0x3F;
              svW[1] = (GetRandomU32() & 0x7E) - 0x3F;
              svW[2] = (GetRandomU32() & 0x7E) - 0x3F;
              AddVector(&ss->posX, svV, svW);
              AddVector(&shard->posX, &shard->posX, &ss->posX);
              ss->spinX = (GetRandomU32() & 2) - 1;
              ss->spinY = (GetRandomU32() & 2) - 1;
              ss->spinZ = (GetRandomU32() & 2) - 1;
              ss->life = 0x3C;
            }

            PlayActorMeshVoice(actor, 0, 0);
            DespawnActorRecord(actor);
            break;
          }
        }
      }

      {
        int c = LookupCosine(g_nVblankTickCount << 4);

        c = (ABS(c) << 4) >> 12;
        if (c >= 0x40) {
          c = 0x3F;
        }
        *(int *)&actor->tint0 = (c << 6) | 0xA00000;
      }
      break;
    }
    /* Flight-level HUD controller: the countdown / time-attack clock digits,
     * the per-target-class collection tally, and the four tally slots that
     * finish the level once they are all filled (HudState). */
    case 0x166: {
      HudState *st = actor->state;
      int i;
      int prevMin;
      int prevTenth;
      int t;
      int y;

      CopyVector(&actor->posX, &SPY.posX);

      if (st->icon[0] == 0) {
        g_nFlightTimeBonus = st->time;
        g_nFlightTimeRemaining = st->time;
        g_nFlightRunClock = 0;
        g_nFlightQuitFlag = 0;
        g_anFlightTallyShown[0] = 0;
        g_anFlightTargetTally[0] = 0;
        g_anFlightTallyShown[1] = 0;
        g_anFlightTargetTally[1] = 0;
        g_anFlightTallyShown[2] = 0;
        g_anFlightTargetTally[2] = 0;
        g_anFlightTallyShown[3] = 0;
        g_anFlightTargetTally[3] = 0;
        g_anFlightTallyActiveSlots_e3 = -1;
        g_anFlightTallyActiveSlots_e2 = -1;
        g_anFlightTallyActiveSlots_e1 = -1;
        g_anFlightTallyActiveSlots = -1;
        g_nFlightTallySlot = -1;
        g_nFlightTallyShownClass = -1;

        for (i = 3; i >= 0; i--) {
          st->digit[i] = g_pfnLevelSpawnActor(0x147, actor);
          st->digit[i]->cullRadius = 0xFF;
          st->digit[i]->otBias = 0x20;
          st->digit[i]->posZ = 0x1000;
          st->digit[i]->rotZ = 0;
        }
        st->digit[0]->posX = 0x190;
        st->digit[0]->posY = 0x1E;
        st->digit[1]->posX = 0x1A4;
        st->digit[1]->posY = 0x1E;
        st->digit[2]->posX = 0x1B4;
        st->digit[2]->posY = 0x1E;
        st->digit[3]->posX = 0x1C4;
        st->digit[3]->posY = 0x1E;

        if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0) {
          for (i = 4; i < 8; i++) {
            st->digit[i] = g_pfnLevelSpawnActor(0x147, actor);
            st->digit[i]->cullRadius = 0xFF;
            st->digit[i]->otBias = 0x20;
            st->digit[i]->rotZ = 0;
          }
          st->digit[4]->posX = 0x180;
          st->digit[4]->posY = 0x1F;
          st->digit[4]->posZ = 0x1800;
          st->digit[5]->posX = 0x180;
          st->digit[5]->posY = 0x18;
          st->digit[5]->posZ = 0x1800;
          st->digit[6]->posX = 0x170;
          st->digit[6]->posY = 0x1E;
          st->digit[6]->posZ = 0x1000;
          st->digit[7]->posX = 0x1D4;
          st->digit[7]->posY = 0x1E;
          st->digit[7]->posZ = 0x1000;
        }

        for (i = 0; i < 8; i++) {
          st->icon[i] = g_pfnLevelSpawnActor(st->types[0], actor);
        }

        {
          int *src;
          int off;

          for (i = 0, src = (int *)st, off = 0; i < 4; i++) {
            *(int *)((char *)g_anFlightHudClassWords + off) = src[0x21];
            src++;
            off += 4;
          }
        }

        for (i = 0, y = 0x3C; i < 4; i++) {
          g_abHudIconActorRecords[i].type = st->types[i];
          InitActorRenderDefaults(&g_abHudIconActorRecords[i]);
          HideActorRenderRecord(&g_abHudIconActorRecords[i]);
          g_abHudIconActorRecords[i].posY = y;
          y += 0x14;
          g_abHudIconActorRecords[i].otBias = 0x20;
          g_abHudIconActorRecords[i].cullRadius = 0xFF;
          g_abHudIconActorRecords[i].posX = 0x3C;
          g_abHudIconActorRecords[i].posZ = 0x1000;
          g_abHudIconActorRecords[i].tintMode = i + 3;
        }
      }

      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0) {
        t = g_nFlightRunClock;
      } else {
        t = g_nFlightTimeRemaining;
      }
      if (t < 0) {
        t = 0;
      }
      prevMin = t / 60;
      prevTenth = t / 6;

      if (g_anFlightTargetTally[0] + g_anFlightTargetTally[1] + g_anFlightTargetTally[2] + g_anFlightTargetTally[3] !=
          0x20) {
        if (g_bDebugNoDepletion == 0 && g_nSpyroStateFlags < 8 &&
            g_anWorldPersistentFlags[g_nActiveWorldIndex] == 0) {
          g_nFlightTimeBonus -= g_nFrameStep;
          g_nFlightTimeRemaining -= g_nFrameStep;
          if (g_nFlightTimeBonus < 0) {
            g_pfnLevelFallImpactCallback();
          }
        }
        g_nFlightRunClock += g_nFrameStep;
        if (g_nFlightRunClock > 0x8C9F) {
          g_nFlightRunClock = 0x8C9F;
        }
      } else {
        ChangeSpyroState(0x20);
        g_nSpyroStateFlags = 3;
        g_dwSpyroRequestMask = 0x80004000;
      }

      if (g_anWorldPersistentFlags[g_nActiveWorldIndex] != 0) {
        t = g_nFlightRunClock;
        i = t / 3600;
        if (i != 0) {
          st->digit[6]->type = i + 0x104;
          st->digit[4]->cullRadius = 0xFF;
          st->digit[5]->cullRadius = 0xFF;
          st->digit[6]->cullRadius = 0xFF;
        } else {
          st->digit[4]->cullRadius = 0;
          st->digit[5]->cullRadius = 0;
          st->digit[6]->cullRadius = 0;
        }
        i = t * 10 / 6 % 10;
        st->digit[7]->type = i + 0x104;
        st->digit[7]->cullRadius = 0xFF;
      } else {
        t = g_nFlightTimeRemaining;
        if (t < 0) {
          t = 0;
        }
      }

      i = t / 600;
      if (i != 0) {
        i = i % 6;
        st->digit[0]->type = i + 0x104;
        st->digit[0]->cullRadius = 0xFF;
      } else {
        st->digit[0]->cullRadius = 0;
      }

      {
        int min = t / 60;

        i = min % 10;
        st->digit[1]->type = i + 0x104;
        i = t / 6 - min * 10;
        st->digit[3]->type = i + 0x104;

        if (g_anWorldPersistentFlags[g_nActiveWorldIndex] == 0) {
          i = min;
          if (i < prevMin) {
            if (i < 6) {
              st->hurry = 1;
            } else {
              st->hurry = 0;
            }
            if (i < 10) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0x21], actor, 0x10, 0);
            }
          }
          if (st->hurry != 0) {
            i = t / 6;
            if (i < prevTenth) {
              PlaySoundEffect(g_pLevelSampleBankHeader[0x25], actor, 0x10, 0);
            }
          }
        }
      }

      if (st->spent[0] != 0) {
        if (st->count != 0 && TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->count--;
          st->spent[st->count]->mode = 2;
          st->timer = 0x10;
          PlaySoundEffect(AS_PTRU8(g_pLevelSampleBankHeader)[0x24], actor, 0x10, 0);
        }
        for (i = 0; i < 8; i++) {
          Actor *a = st->spent[i];

          if (a != 0 && a->mode == 4) {
            if (st->slot[g_nFlightTallySlot] == 0) {
              st->slot[g_nFlightTallySlot] = a;
              a->posX = g_nFlightTallySlot * 50 + 0x1E;
              st->slot[g_nFlightTallySlot]->posY = 0xC8;
              st->slot[g_nFlightTallySlot]->posZ = 0x1000;
              st->slot[g_nFlightTallySlot]->mode = 1;
            } else {
              DespawnActorRecord(a);
            }
            st->spent[i] = 0;
            PlaySoundEffect(AS_PTRU8(g_pLevelSampleBankHeader)[0x24], actor, 0x10, 0);
          }
        }
      } else if (st->slot[0] != 0 && st->slot[1] != 0 && st->slot[2] != 0 &&
                 st->slot[3] != 0) {
        int s = st->slot[0]->mode;

        if (s == 4 || (g_dwPadPressed & 0x40)) {
          g_pfnLevelFallImpactCallback();
        } else if (s == 1) {
          for (i = 0; i < 4; i++) {
            st->slot[i]->subMode = i;
            st->slot[i]->mode = 5;
          }
        }
      } else {
        if (g_anFlightTallyActiveSlots >= 0) {
          g_nFlightTallyShownClass = g_anFlightTallyActiveSlots;
          if (g_anFlightTargetTally[g_nFlightTallyShownClass] != g_anFlightTallyShown[g_nFlightTallyShownClass]) {
            int *cnt = g_anFlightTargetTally;
            int x;

            for (i = 0, x = 0x1E; i < 8; i++) {
              st->icon[i]->type = st->types[g_nFlightTallyShownClass];
              if (cnt[g_nFlightTallyShownClass] < i + 1) {
                st->icon[i]->cullRadius = 0xFF;
                st->icon[i]->posX = x;
                st->icon[i]->tintMode = 0;
                st->icon[i]->rotZ = 0;
                st->icon[i]->mode = 0;
              } else {
                st->icon[i]->cullRadius = 0xFF;
                st->icon[i]->posX = x;
                st->icon[i]->tintMode = g_nFlightTallyShownClass + 3;
                st->icon[i]->mode = 1;
              }
              x += 0x1E;
            }

            g_anFlightTallyShown[g_nFlightTallyShownClass] = g_anFlightTargetTally[g_nFlightTallyShownClass];
            if (g_anFlightTargetTally[g_nFlightTallyShownClass] == 8) {
              for (i = 0; i < 8; i++) {
                st->spent[i] = st->icon[i];
                st->icon[i] = g_pfnLevelSpawnActor(st->types[0], actor);
              }
              st->timer = 0;
              st->count = 8;
              g_nFlightTallySlot = g_nFlightTallyShownClass;
            }
          }
          g_anFlightTallyActiveSlots = g_anFlightTallyActiveSlots_e1;
          g_anFlightTallyActiveSlots_e1 = g_anFlightTallyActiveSlots_e2;
          g_anFlightTallyActiveSlots_e2 = g_anFlightTallyActiveSlots_e3;
          g_anFlightTallyActiveSlots_e3 = -1;
        }
        g_nFlightTallyShownClass = -1;
      }

      if (g_nGamestate == 7) {
        for (i = 0; i < 8; i++) {
          if (st->icon[i] != 0) {
            st->icon[i]->cullRadius = 0;
          }
          if (st->spent[i] != 0) {
            st->spent[i]->cullRadius = 0;
          }
        }
        for (i = 0; i < 8; i++) {
          if (st->digit[i] != 0) {
            st->digit[i]->cullRadius = 0;
          }
          if (st->slot[i] != 0) {
            st->slot[i]->cullRadius = 0;
          }
        }
      }
      break;
    }
    /* Fade-out props: removed as soon as their animation finishes. */
    case 0x190:
    case 405:
    case 477:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* A 0x161 burst shard: drifts and tumbles until its life runs out or it
     * leaves the screen. */
    case 0x177:
    case 0x178:
    case 0x179:
    case 0x17A:
    case 0x17B:
    case 0x17C:
    case 0x17D:
    case 0x17E:
    case 0x17F:
    case 0x180:
    case 0x181:
    case 0x182: {
      BurstState *ss = actor->state;

      if (actor->drawn != 0 && TickCountdownByFrameDelta(&ss->life, 4) == 0) {
        AddVector(&actor->posX, &actor->posX, &ss->posX);
        actor->rotX += ss->spinX;
        actor->rotY += ss->spinY;
        actor->rotZ += ss->spinZ;
      } else {
        DespawnActorRecord(actor);
      }
      break;
    }
    /* HUD collectible icon: swings from where it was picked up onto its
     * slot in the flight tally, spinning all the way (TallyState). */
    case 0x183:
    case 0x184:
    case 0x185:
    case 0x189: {
      TallyState *st = actor->state;

      switch (actor->mode) {
      case 0:
        break;
      case 1:
        actor->rotZ += 8;
        break;
      case 2:
        svW[0] = g_nFlightTallySlot * 50 + 0x1E;
        svW[1] = 0xC8;
        svW[2] = 0x1000;
        AddVector(&st->x, svW, &actor->posX);
        RShiftVector3(&st->x, 1);
        SubtractVector(svW, &st->x, &actor->posX);
        st->radius = VectorLength(svW, 0);
        st->angle =
            ArcTan2(actor->posX - st->x, st->y - actor->posY, 1);
        st->phase = 0x800;
        actor->mode = 3;
        actor->rotZ += 8;
        break;
      case 3:
        st->phase -= 0x40;
        actor->posX =
            st->x - ((st->radius * LookupCosine(st->angle + st->phase)) >> 12);
        actor->posY =
            st->y + ((st->radius * LookupSine(st->angle + st->phase)) >> 12);
        actor->rotZ += 8;
        if (st->phase <= 0) {
          actor->mode = 4;
        }
        break;
      case 5:
        svW[0] = (g_anCosineLut[actor->subMode * 64] >> sh6) + 0x100;
        svW[1] = ((g_anSineLut[actor->subMode * 64] * 5) >> 9) + 0x78;
        svW[2] = 0x1000;
        AddVector(&st->x, svW, &actor->posX);
        RShiftVector3(&st->x, 1);
        SubtractVector(svW, &st->x, &actor->posX);
        st->radius = VectorLength(svW, 0);
        st->angle =
            ArcTan2(actor->posX - st->x, st->y - actor->posY, 1);
        st->phase = 0x800;
        actor->mode = 6;
        actor->rotZ += 8;
        break;
      case 6:
        st->phase -= 0x40;
        actor->posX =
            st->x - ((st->radius * LookupCosine(st->angle + st->phase)) >> 12);
        actor->posY =
            st->y + ((st->radius * LookupSine(st->angle + st->phase)) >> 12);
        actor->rotZ += 8;
        if (st->phase <= 0) {
          st->angle =
              ArcTan2(actor->posX - 0x100, actor->posY - 0x78, 1);
          st->phase = -0x3000;
          st->radius = 0x40;
          actor->mode = 7;
        }
        break;
      case 7:
        st->phase += 0x80;
        st->radius -= 2;
        actor->posX =
            ((st->radius * (LookupCosine(st->angle + st->phase) << 6)) >> 18) +
            0x100;
        {
          int s = LookupSine(st->angle + st->phase) * 40;

          actor->posY = ((st->radius * s) >> 18) + 0x78;
        }
        actor->rotZ += 8;
        if (st->phase >= 0) {
          actor->mode = 4;
        }
        break;
      }
      break;
    }
    /* Floating-word letters (OrbitState). */
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
      OrbitState *st = actor->state;

      if (st->owner->mode == 0 || st->owner->subMode != actor->mode) {
        DespawnActorRecord(actor);
      } else if (st->owner->type == 9) {
        if (ApproxDist2D(&st->owner->posX, &spy->posX) < 0x400) {
          int oldval;
          oldval = g_anCosineLut[actor->subMode];
          actor->subMode += 8;
          actor->rotZ = actor->rotZ - (oldval * 3 >> 9) +
                         (g_anCosineLut[actor->subMode] * 3 >> 9);
        } else {
          int anglespyro;
          int vecA[3];
          int vecB[3];
          int vecC[3];
          int vecD[3];

          actor->subMode += 8;
          anglespyro = ArcTan2(spy->posX - st->owner->posX,
                                     SPY.posY - st->owner->posY, 0);
          vecA[0] = g_anCosineLut[anglespyro] * 3 >> 4;
          vecA[1] = g_anSineLut[anglespyro] * 3 >> 4;
          vecA[2] = 0;
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
          vecD[0] = vecA[0] * g_anCosineLut[anglespyro];
          vecD[1] = vecA[1] * g_anCosineLut[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->ringPhase * 4)) & 0xFF;
          ScaleVector3Sat(vecB, vecB, st->ringPhase);
          SubtractVector(vecC, vecC, vecB);
          actor->posX = vecA[0] * g_anCosineLut[anglespyro];
          actor->posY = vecA[1] * g_anCosineLut[anglespyro];
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, vecD);
          RShiftVector3(&actor->posX, 10);
          AddVector(&actor->posX, &actor->posX, vecA);
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

    case 0x1E1:
    case 0x1E2:
    case 0x1E3: {
      EmberState *st = actor->state;
      int trail[3];

      if (st->life > 0 && actor->drawn) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -128)
          st->vel[2] = -128;
        actor->posZ += st->vel[2];

        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        if ((st->life & 3) == 0) {
          trail[0] = GetRandomU32() & 3;
          trail[1] = GetRandomU32() & 3;
          trail[2] = 0x14;
          g_pfnLevelEmitParticles(1, 1, &actor->posX, trail);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    default: /* 77..82, 88..109, 258..259 and everything unlisted */
      break;
    }
  }
}
