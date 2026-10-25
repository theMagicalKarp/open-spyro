/* Level33_UpdateActors -- per-frame actor update for level 33 (Gnasty
 * Gnorc). 0x8007AD64, 33,216 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 0/7/9/13/16/25/31/34; the level-33 classes
 * are Gnasty Gnorc himself (161), his trail bombs (169) and their shockwave
 * (143), the lane patrollers (159), a counter (168) and a follower (414).
 *
 * Load-bearing source forms:
 *   - Carriers as in level 25 (`oneq`, `one`) but neither is used: every
 *     constant 1 is a literal, so the SImode 1 group is hoisted into fp and
 *     the byte stores stay inline. `spyc` (case 181's headLook) is hoisted
 *     too. Case 21's pose 3 is ENTER_POSE_B (A285) and case 169's posY sine
 *     read uses the g_anSineLut alias, which keeps 3 and g_anSineLut_s out of
 *     loop.c's hoist set, as in the original.
 *   - `base = (char *)g_anLevelReadyFlagBlock` before the loop: the balloon-stage address
 *     is a spilled equivalence rematerialised at its two uses, while the
 *     stage itself is read through the scalar g_nLevelReadyFlag view.
 *   - Gnasty's camera blocks read the anchor path through a scalar view of
 *     the state block so the loads stay behind the scalar flag stores
 *     (A297/A302); state 8 writes the C4C flag word as g_nCameraScriptStickRequestY[19] so
 *     only the g_nGameplayBlocked store sinks to the jal.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"
#include "level_actor_states.h"
#include "level_update_actors_macros.h"
#include "gamestate.h"


/* Type 286: ambient sound source; mode 2 also coasts along a path. */


/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */

/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */


/* 256-entry sine and cosine tables. */
extern short g_anSineLut_s[];


/* Glow node owned by Sparx; follows its anchor position. */

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


/* ==== Level 16 classes ==== */

/* Case 328 -- floor-snapped marker. */


/* ENTER_POSE with the already-there test first. */
/* ==== Level 2 classes ==== */


/* ==== Level 9 classes ==== */

/* ==== Level 26 classes ==== */

#define sixteen 0x10

/* ==== Classes shared with level 31 ==== */


/* ==== Level 25 classes ==== */

/* Case 50 -- grabber. Only one grabber at a time may hold Spyro; the shared
 * slot names the holder. */


/* ==== Level 33 classes ==== */

/* Type 143 state. */
typedef struct L33State143 {
  int u00; /* 0x00 radius */
  int u04; /* 0x04 timer */
} L33State143;

/* Type 169's trail point. */
typedef struct L33Trail {
  int pos[3]; /* 0x00 */
  int u0C[3]; /* 0x0C */
  int age;    /* 0x18 */
} L33Trail;   /* 0x1C */

/* Type 169 state. */
typedef struct L33State169 {
  L33Trail *trail; /* 0x00 */
  short h04;       /* 0x04 spawn timer */
  short h06;       /* 0x06 life timer */
  short h08;       /* 0x08 landing delay */
  short h0A;       /* 0x0A trail length */
  int u0C;         /* 0x0C speed */
  int u10;         /* 0x10 climb */
} L33State169;

extern L33Trail *g_apMotionTrailSlices[]; /* trails */

#define FLY_FLAGS(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x14))

/* Type 159 state. */
typedef struct L33State159 {
  FlyPath *path; /* 0x00 */
  int u04;       /* 0x04 lane; indexes g_anLevelEscapeRunCount */
  int u08;       /* 0x08 slow-down timer */
  int u0C;       /* 0x0C */
  int u10;       /* 0x10 speed */
  int u14;       /* 0x14 knock-back heading */
  int u18;       /* 0x18 knock-back speed */
  int u1C;       /* 0x1C vertical speed */
  int u20;       /* 0x20 end node going back */
  int u24;       /* 0x24 end node going forward */
  int u28[14];   /* 0x28 trigger box */
  int u60;       /* 0x60 partner actor index, -1 none */
} L33State159;

extern int g_anLevelEscapeRunCount[]; /* laps per lane */

/* Type 161 state (Gnasty Gnorc). */
typedef struct GnastyState {
  FlyPath *path;            /* 0x00 current escape path */
  FlyPath *path2;           /* 0x04 the other escape path */
  FlyPath *path3;           /* 0x08 camera reference path */
  int u0C;                  /* 0x0C */
  int u10;                  /* 0x10 */
  int u14;                  /* 0x14 knock-back heading */
  int u18;                  /* 0x18 speed */
  int u1C;                  /* 0x1C vertical speed */
  int u20;                  /* 0x20 queued jump speed */
  int u24[12];              /* 0x24 */
  int u54;                  /* 0x54 */
  int u58;                  /* 0x58 timer */
  int phase;                /* 0x5C fight phase */
  int timer;                /* 0x60 throw timer */
  int u64;                  /* 0x64 */
  int throws;               /* 0x68 bombs thrown */
  int u6C;                  /* 0x6C */
  int box70[6];             /* 0x70 trigger box */
  int box88[6];             /* 0x88 trigger box */
  int boxA0[6];             /* 0xA0 trigger box */
  int uB8;                  /* 0xB8 */
  int key2;                 /* 0xBC actor index, -1 none */
  int key;                  /* 0xC0 actor index */
  int seen;                 /* 0xC4 */
  unsigned char *home;      /* 0xC8 */
  int camTimer;             /* 0xCC */
  int uD0;                  /* 0xD0 */
  int drop[3];              /* 0xD4 key actors dropped while hit */
  L33Trail trails[3][150];  /* 0xE0 one per live bomb */
} GnastyState;

/* Gravity toward the floor under the actor. */
#define GNASTY_FALL(a, st)                                                     \
  {                                                                            \
    int h_ = ProbeActorGroundHeightRaised(a, 0xBB8);                                          \
    int z_;                                                                    \
                                                                               \
    (st)->u1C -= 0x19;                                                         \
    if ((st)->u1C < -0x190) {                                                  \
      (st)->u1C = -0x190;                                                      \
    }                                                                          \
    z_ = (a)->posZ + (st)->u1C;                                                \
    if (z_ < h_) {                                                             \
      (a)->posZ = h_;                                                          \
    } else {                                                                   \
      (a)->posZ = z_;                                                          \
    }                                                                          \
  }

extern unsigned char g_abSpyroPersistentEuler; /* SPY.bodyRotX, absolute form */
#define PATH_END(p) ((int *)((char *)(p) + (((p)->count << 4) - 8)))

/* ==== The function ==== */
#define spy (&SPY)
void Level33_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  short oneq;
  int one;
  SpyroObj *spyc;
  char *base;
  int dt;

  /* Scratch vectors, in frame order. */
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
  int fx[6];
  int col[3];
  int o128[3];
  int o138[2];
  int o140[3];
  int o150[3];
  int o160[3];
  int o170[2];
  int w181[3];
  int s392[3];
  int v310[3];
  int ps[6];
  int s448[3];
  int s480[3];
  int col2[3];
  int s496[3];
  int s512[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  base = (char *)g_anLevelReadyFlagBlock;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    spyc = (SpyroObj *)&g_anSpyroWorldPos;
    oneq = 1;
    one = 1;

    if (actor->mode >= 0x80) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {

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

      if (g_nLevelReadyFlag < 3 && ApproxDist2D(&actor->posX, (int *)(base - 0x164)) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pLevelOverlayInitRecord == 0) {
            if (g_nLevelReadyFlag >= 0) {
              g_pLevelOverlayInitRecord = g_pfnLevelSpawnActor(120, actor);

              PlayActorMeshVoice(g_pLevelOverlayInitRecord, 0, 0);

              g_nLevelReadyFlag = 1;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pLevelOverlayInitRecord->subMode != 99) {
            ChaserState *cs = (ChaserState *)g_pLevelOverlayInitRecord->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(SPY.bodyRotZ,
                                ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0)) < 0) {
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
        SteerActorAroundPoint(actor, &SPY.posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, 0),
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
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
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
          CopyVector(svA, &SPY.posX);
          {
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int s = LookupSine(SPY.bodyRotZ << 4);
            int cr = c * 724;
            int sr = s * 724;
            svA[0] = svA[0] + ((sr + cr) >> 12);
          }
          {
            int s = LookupSine(SPY.bodyRotZ << 4);
            int c = LookupCosine(SPY.bodyRotZ << 4);
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
          CopyVector(svD, &SPY.posX);
          svD[0] = svD[0] + (LookupCosine(SPY.bodyRotZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(SPY.bodyRotZ << 4) >> 3);
          svD[2] = svD[2] + 0x300;
          VectorToSVectorShr2(st->from, svD);
          st->heading = SPY.bodyRotZ - 0x40;
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

    /* Type 168: counts its sub-state down, restarting its animation each
     * time, then shares 363's exit. */
    case 168:
      if (actor->subMode != 0) {
        actor->subMode--;
        actor->frame = 0;
        actor->nextFrame = 1;
        actor->frameProgress = 0;
      }

    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 363:
    case 364:
    case 507:
    case 508:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 255:
    case 256: {
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
    case 69:
    case 151:
    case 257: {
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
          AddVector(g_aRotBillboards[st->attach].pos,
                        g_aRotBillboards[st->attach].pos, &actor->posX);

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

                st->lean = -ArcTan2_8bit(
                    IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
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
                  if (GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                    g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) ==
                      0) {
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
              int slope = (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

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
                  if (GetSoundVoiceStatusByOwner(g_pLevelOverlayInitRecord,
                                    g_apActorMeshTable[g_pLevelOverlayInitRecord->type]->m_Sounds[0]) ==
                      0) {
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
              actor->posZ += (short)(g_anSineLut_s[st->timer * 4] / 12);
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

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) + (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut_s[st->spin] << 16 >> 25) + (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == 15) {
              actor->rotZ += 8;
              actor->rotY -= 6;
            } else {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 23) + (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut_s[st->spin] << 16 >> 23) + (unsigned char)st->roll;
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

    /* The balloonist (BalloonState): smokes, waits for his gnorc, then flies
     * Spyro out once the world is finished. */
    case 120: {
      BalloonState *st = actor->state;

      if (g_nLevelReadyFlag >= 2) {
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

      if (g_nLevelReadyFlag >= 3) {
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

      if (g_nLevelReadyFlag <= 0) {
        if (st->gnorc != 0 && st->gnorc->type == 16) {
          st->gnorc->mode = 0;
        }
        g_pLevelOverlayInitRecord = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->gnorc != 0 &&
          ApproxDist2D(&actor->posX, (int *)(base - 0x164)) >=
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
                  (unsigned short)g_anSineLut_s[st->gnorc->rotZ] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->gnorc->rotZ] << 16 >> 22;
              svA[1] -=
                  (unsigned short)g_anSineLut_s[st->gnorc->rotZ] << 16 >> 22;
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
            g_nLevelReadyFlag += 1;
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

    /* Type 143. */
    case 143: {
      L33State143 *st = actor->state;

      st->u00 += dt * 14;
      actor->scale = 0xBB80 / st->u00;
      if (actor->scale < 5) {
        actor->scale = 5;
      }
      if (actor->scale >= 0x80) {
        actor->scale = 0x7F;
      }
      if (st->u04 != 0) {
        RaiseSpyroTriggerInRadius(&actor->posX, st->u00, 7);
      }
      if (actor->nextAnim == 1 && g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TickCountdownByFrameDelta(&st->u04, 4) != 0) {
        ANIM_SET(actor, 1);
      }
      continue;
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

    /* Type 159: a path patroller that turns back toward Spyro. */
    case 159: {
      L33State159 *st = actor->state;
      int dist;

      SubtractVector(o128, &SPY.posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &SPY.posX);
      if (dist < 0x32C8) {
        dist = VectorLength(o128, 0);
      }

      if (actor->flags != 0 && actor->mode != 2) {
        st->u18 = 0xFA;
        if (actor->flags & 0x20000) {
          st->u18 = 0x145;
        }
        st->u14 = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->u1C = 0x28;
        ENTER_POSE(actor, 2);
      }
      actor->flags = 0;

      if (st->u04 == 1 && actor->cullRadius == 0 &&
          ((GetWorldChunkAnimState(4) >> 8) & 0xFF) > 0) {
        actor->cullRadius = 0x28;
        g_pActorListBase[st->u60].cullRadius = 0x28;
      }

      if (actor->nextAnim == 0 && actor->cullRadius != 0) {
        if (actor->nextFrame == 1 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 0xA && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 5, 0, 0);
        if (dist < 0x1964 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                               : -SPY_DZ(actor) < 0x578)) {
          st->u10 = 0x12C;
          if (st->u04 == 0) {
            st->path->dir = 1;
            st->u08 = 0x6E;
          } else {
            st->path->dir = 1;
            st->u08 = 0x96;
          }
          st->u08 -= g_anLevelEscapeRunCount[st->u04] * 6;
          if (IsPointInRotatedBox(&SPY.posX, st->u28) != 0) {
            st->path->dir *= -1;
          }
          if (st->path->dir == -1) {
            st->path->cur = st->u20;
          } else {
            st->path->cur = st->u24;
          }
          ENTER_POSE(actor, 1);
        }
        break;

      case 1: {
        FlyPath *p;
        int spd;
        int r;
        int h;
        int z;

        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, st->path->cur));
          SnapActorToGround(actor);
        }
        if (TickCountdownByFrameDelta(&st->u08, 4) != 0) {
          int lo = 0xDC - g_anLevelEscapeRunCount[st->u04] * 6;

          st->u10 -= 0xA;
          if (st->u10 < lo) {
            st->u10 = lo;
          }
        }
        spd = st->u10;
        if (FLY_FLAGS(st->path, st->path->cur) & 0xFFC) {
          spd = FLY_FLAGS(st->path, st->path->cur) & 0xFFC;
        }
        r = PivotActorAlongPath(actor, st->path, 0x1C2, spd, 0, 0xA, 0x28, 0xFF, 1);
        h = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->u1C -= 0x1E;
        if (st->u1C < -0x190) {
          st->u1C = -0x190;
        }
        z = actor->posZ + st->u1C;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        if ((r & 0x100) && (FLY_FLAGS(st->path, st->path->cur) & 3) == 1) {
          st->u1C = 0x12C;
        }
        if ((st->path->dir == -1 && r == (st->u20 | 0x100)) ||
            (st->path->dir == 1 && r == (st->u24 | 0x100))) {
          g_anLevelEscapeRunCount[st->u04]++;
          if (g_anLevelEscapeRunCount[st->u04] >= 6) {
            g_anLevelEscapeRunCount[st->u04] = 5;
          }
          ENTER_POSE(actor, 0);
        }
        FindFarthestPathPointFromSpyro((PathData *)st->path);
        FindNearestPathPointToSpyro(st->path, o138);
        if (dist < 0x3000) {
          int a;
          int n;
          int i;

          a = AbsAngleDelta8(
              ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                            FLY_Y(st->path, st->path->cur) - actor->posY, 0),
              actor->rotZ);
          n = 0;
          p = st->path;
          i = p->cur;
          while (i != o138[0]) {
            i -= p->dir;
            i = (i + p->count) % p->count;
            n++;
          }
          if ((unsigned)(n - 1) < 7 && a < 0x10 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(SPY.posX - actor->posX,
                                          SPY.posY - actor->posY, 0)) < 0x3A &&
              n < st->path->count >> 1) {
            st->path->dir = -st->path->dir;
            st->path->cur = (st->path->cur - st->path->dir + st->path->count) %
                            st->path->count;
          }
        }
        break;
      }

      case 2:
        DriveActorTrackedMove(actor, &st->u18, st->u14, &st->u1C, 0xC, sixteen);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      if (actor->mode != 2 && st->u60 != -1) {
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, &g_pActorListBase[st->u60].posX);
        ProbeActorGroundHeight(&g_pActorListBase[st->u60]);
        EncodeCachedVecToActorDirCode(&g_pActorListBase[st->u60]);
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 161: Gnasty Gnorc. Runs his escape paths, throws trail bombs
     * (169) at Spyro and drops the vault keys as he is hit. */
    case 161: {
      GnastyState *st = actor->state;
      int dist;

      dist = ApproxDist2D(&actor->posX, &SPY.posX);
      TickActorFlameGlow(actor);

      if (actor->flags & 0x10000) {
        if (st->phase == 2 &&
            ApproxDist2D(&actor->posX,
                          PATH_END(st->path)) < 0xBB8 &&
            actor->mode != 7) {
          FlyPath *t;

          ModifyWorldChunkAnimFlags(1, 0xFC, 0);
          g_pActorListBase[st->key].subMode = 1;
          st->camTimer = 0xA0;
          st->u1C = 0;
          st->u20 = 0;
          st->u6C = 0;
          st->phase++;
          t = st->path;
          st->path = st->path2;
          st->path2 = t;
          ENTER_POSE(actor, 7);
        } else if (st->phase == 3 &&
                   ApproxDist2D(&actor->posX,
                                 PATH_END(st->path)) < 0xBB8 &&
                   actor->mode != 8) {
          st->phase = 4;
          st->u18 = 0xD2;
          st->u14 = ArcTan2(actor->posX - SPY.posX,
                                  actor->posY - SPY.posY, 0);
          if (actor->dropKind != 0xFF) {
            g_anLevelKillBitmapTable[0xF0] &= ~1;
          }
          SpawnActorDeathDrop(actor, 4, 0, 0);
          ENTER_POSE(actor, 8);
        }
      }

      if (st->phase < 2) {
        CopyVector(o140, &actor->posX);
        o140[2] += 0x3E8;
        if (g_nSpyroState == 0xB || g_nSpyroState == 0x14) {
          st->camTimer = 0x3C;
        }
        if (TickCountdownByFrameDelta(&st->camTimer, 4) != 0 && dist < 0x4000 &&
            (CheckSpyroLineOfSightTo(o140) != 0 || dist < 0x3000)) {
          g_dwSpyroRequestMask = 0x80010000;
          g_pScriptedCameraActor = actor;
        }
      }

      if (st->key2 != -1 && g_pActorListBase[st->key2].subMode != 0 &&
          AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                        ArcTan2(actor->posX - SPY.posX,
                                      actor->posY - SPY.posY, 0)) < 0x28 &&
          (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1B58 ||
           st->throws >= 4)) {
        st->key2 = -1;
        actor->mode = 0xA;
        continue;
      }

      if (st->seen == 0 && g_nSpyroAirborneFrames == 0 &&
          (IsPointInRotatedBox(&SPY.posX, st->box70) != 0 ||
           IsPointInRotatedBox(&SPY.posX, st->boxA0) != 0)) {
        st->seen = 1;
      }

      if ((unsigned)(actor->mode - 6) < 2) {
        int ca = (ArcTan2(PATH_X((unsigned char *)st->path2,
                                       st->path2->count - 1) -
                                    PATH_X((unsigned char *)st->path3, 0),
                                PATH_Y((unsigned char *)st->path2,
                                       st->path2->count - 1) -
                                    PATH_Y((unsigned char *)st->path3, 0),
                                0) +
                  0x80) &
                 0xFF;

        if (TickCountdownByFrameDelta(&st->camTimer, 4) == 0) {
          int *anc;

          g_dwSpyroRequestMask |= 0x80002200;
          anc = PATH_END(*(FlyPath **)((char *)st + 4));
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          *(int *)&g_anCameraActiveTargetParams = -ca << 4;
          g_nCameraActiveTargetDistance = 0x1770;
          g_nCameraActiveTargetPitch = 0x19;
          g_nCameraActiveTargetRoll = 0;
          g_nCameraActiveTargetPitchBias = -0x32;
          g_nCameraActiveTargetYawBias = 0;
          g_nGameplayBlocked = 1;
          g_pSpyroRequestCameraAnchor = anc;
        } else {
          g_nGameplayBlocked = 0;
        }
      }

      if ((unsigned)(actor->mode - 8) < 2) {
        int ca = (SPY.bodyRotZ + 0x40) & 0xFF;
        int *anc;

        AddVector(PATH_PT((unsigned char *)st->path, 0), &actor->posX,
                      &SPY.posX);
        RShiftVector3(PATH_PT((unsigned char *)st->path, 0), 1);
        g_dwSpyroRequestMask = 0x80002200;
        anc = (int *)((char *)*(FlyPath **)st + 8);
        g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
        *(int *)&g_anCameraActiveTargetParams = -ca << 4;
        g_nCameraActiveTargetDistance = 0xFA0;
        g_nCameraActiveTargetPitch = 0x19;
        g_nCameraActiveTargetRoll = 0;
        g_nCameraActiveTargetPitchBias = -0x32;
        g_nCameraActiveTargetYawBias = 0;
        g_pSpyroRequestCameraAnchor = anc;
      }

      actor->flags = 0;
      switch (actor->mode) {
      case 0:
        SnapActorToGround(actor);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && st->seen != 0 &&
            AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], 0)) <
                0x1E &&
            ApproxDist2D(&SPY.posX, PATH_PT(st->home, 0)) < 0x3000 &&
            (unsigned)(dist - 0x1801) < 0x4FFF) {
          st->throws = 0;
          actor->mode = 2;
          continue;
        }
        break;

      case 2:
        if (st->phase == 2) {
          if (SPY_DZ(actor) < 0 && dist < 0x1388) {
            actor->mode = 5;
            continue;
          }
        } else if (st->phase == 3) {
          if (st->throws < 4) {
            if (dist >= 0xE10) {
              goto anim2;
            }
            st->timer = 0;
          } else {
            st->timer = 0x96;
          }
          actor->mode = 0xC;
          continue;
        } else if (st->throws >= 4 ||
                   AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                                 ArcTan2(actor->posX - g_anCameraPos[0],
                                               actor->posY - g_anCameraPos[1],
                                               0)) > 0x28 ||
                   (unsigned)(dist - 0x1770) > 0x55F0) {
          st->timer = 0x96;
          ENTER_POSE(actor, 0);
        }
      anim2:
        if (TrackEntityYawTowardSpyro(actor, 5, 0x14, 1) == 0) {
          continue;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          ANIM_GO(actor, 2);
        } else if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 0);
        }
        if (st->timer == 0 && actor->nextFrame >= 9) {
          Actor *o = g_pfnLevelSpawnActor(0xA9, actor);
          L33State169 *q = o->state;
          int n;
          int spd = 0x190;

          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 0, &o->posX);
          o->rotZ =
              ArcTan2(SPY.posX - o->posX, SPY.posY - o->posY, 0);
          CopyVector(o160, &SPY.posX);
          o160[2] = FindGroundHeightBelow(o160, 0x1000);
          SubtractVector(o150, o160, &o->posX);
          n = VectorLength(o150, 0) / spd;
          q->u0C = spd;
          if (n <= 0) {
            n = 1;
          }
          q->u10 = (o160[2] - o->posZ) / n;
          if (q->u10 < -0x1F4) {
            q->u10 = -0x1F4;
          }
          q->trail = st->trails[st->throws % 3];
          q->h0A = 0;
          q->h06 = 0xA0;
          q->h08 = n - 1;
          st->throws++;
          PlayActorMeshVoice(actor, 0, (int)&st->uD0);
          if (st->phase == 2 && st->throws >= 2) {
            actor->mode = 5;
            continue;
          }
          st->timer = 0x50;
        }
        break;

      case 3:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0) {
          actor->mode = 5;
          continue;
        }
        break;

      case 4:
        GNASTY_FALL(actor, st);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((dist < 0x2000 && SPY_DZ(actor) >= -0x6A3) || dist < 0x16A8) {
          st->u54 = 0;
          actor->mode = 5;
          continue;
        }
        if (ApproxDist2D(&SPY.posX, PATH_PT((unsigned char *)st->path, 12)) <
            0x2000) {
          CopyVector(&actor->posX,
                        PATH_END(st->path));
          SnapActorToGround(actor);
          st->path->cur = 0;
          actor->mode = 0x15;
          continue;
        }
        break;

      case 5: {
        int spd;
        int far;
        unsigned int i;
        int r;

        ANIM_GO(actor, 1);
        if (st->u20 != 0) {
          st->u1C = st->u20;
          st->u20 = 0;
        }
        i = st->path->cur;
        spd = 0x13B;
        if (FLY_FLAGS(st->path, i) & 0xFFC) {
          spd = FLY_FLAGS(st->path, i) & 0xFFC;
        }
        if (i >= 0xE) {
          far = 0x1964;
          if (st->u18 >= 0x10F) {
            far = 0x1F40;
          }
          spd = 0xEB;
          if (i >= 0x1D) {
            spd = 0xD7;
          }
          if (dist < far &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x4B0
                                 : -SPY_DZ(actor) < 0x4B0)) {
            spd = 0x122;
          }
        }
        st->u18 = spd;
        r = PivotActorAlongPath(actor, st->path, 0x200, spd, 0, 7, 0x28, 0xFF, 0);
        GNASTY_FALL(actor, st);
        if (r == 0x101 && ApproxDist2D(&actor->posX, &SPY.posX) > 0x2000) {
          ENTER_POSE(actor, 4);
        }
        if (r == 0x100) {
          FindNearestPathPointToSpyro(st->path, o170);
          if (dist > 0x4800 && o170[0] > 0x10 &&
              o170[0] < st->path->count - 1) {
            st->throws = 0;
            actor->mode = 2;
            continue;
          }
          st->u58 = 0xC8;
          actor->mode = 0x15;
          continue;
        }
        break;
      }

      case 6: {
        int spd;
        int r;
        int h;
        int z;

        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 1);
        }
        if (TickCountdownByFrameDelta(&st->u58, 4) == 0) {
          TurnActorYawTowardAngle(
              actor,
              ArcTan2(
                  PATH_X((unsigned char *)st->path, st->path->cur) -
                      actor->posX,
                  PATH_Y((unsigned char *)st->path, st->path->cur) -
                      actor->posY,
                  0),
              6, 0, 0);
          h = ProbeActorGroundHeightRaised(actor, 0xBB8);
          st->u1C -= 0x19;
          if (st->u1C < -0x190) {
            st->u1C = -0x190;
          }
          z = actor->posZ + st->u1C;
          if (z < h) {
            actor->posZ = h;
            ANIM_GO(actor, 1);
          } else {
            actor->posZ = z;
          }
          break;
        }
        if (st->u20 != 0) {
          st->u1C = st->u20;
          st->u20 = 0;
        }
        spd = 0x104;
        if (FLY_FLAGS(st->path, st->path->cur) & 0xFFC) {
          spd = FLY_FLAGS(st->path, st->path->cur) & 0xFFC;
        }
        r = PivotActorAlongPath(actor, st->path, 0x12C, spd, 0, 0xA, 0x28, 0xFF, 0);
        GNASTY_FALL(actor, st);
        if (r == 0x100) {
          actor->mode = 0xC;
          continue;
        }
        if (st->u6C == 0 &&
            ApproxDist2D(&actor->posX, PATH_PT((unsigned char *)st->path, 6)) <
                0x258) {
          st->u6C = 1;
          actor->mode = 0xB;
          continue;
        }
        if (r & 0x100) {
          switch (FLY_FLAGS(st->path, st->path->cur) & 3) {
          case 1:
            st->u20 = 0xDC;
            break;
          case 2:
            st->u20 = 0x10E;
            break;
          case 3:
            st->u20 = 0x140;
            break;
          }
        }
        break;
      }

      case 7:
        if (g_nActorAnimDone != 0) {
          st->u58 = 0;
          ANIM_GO(actor, 1);
          actor->mode = 6;
          continue;
        }
        break;

      case 8:
        g_nCameraScriptStickRequestY[19] = g_dwSpyroRequestMask | 0x80002000;
        g_nGameplayBlocked = 1;
        DriveActorTrackedMove(actor, &st->u18, st->u14, 0, 0xC, 0);
        if (st->drop[0] != -1 && actor->nextFrame >= 6) {
          g_pActorListBase[st->drop[0]].subMode = 1;
          st->drop[0] = -1;
        }
        if (st->drop[1] != -1 && actor->nextFrame >= 0xB) {
          g_pActorListBase[st->drop[1]].subMode = 1;
          st->drop[1] = -1;
        }
        if (st->drop[2] != -1 && actor->nextFrame >= 0x10) {
          g_pActorListBase[st->drop[2]].subMode = 1;
          st->drop[2] = -1;
        }
        if (g_nSpyroDrawSuppressed == 0 && actor->nextFrame >= 0xE) {
          Actor *o = g_pfnLevelSpawnActor(0x19E, actor);

          *(Actor **)o->state = actor;
          CopyVector(&o->posX, &SPY.posX);
          o->rotX = g_abSpyroPersistentEuler;
          o->rotY = g_bSpyroPersistentEulerY;
          o->rotZ = g_bSpyroPersistentEulerZ;
          g_nSpyroDrawSuppressed = 1;
        }
        if (g_nActorAnimDone != 0) {
          actor->cullRadius = 0;
          actor->drawn = 0;
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x40);
          st->u58 = 0x3C;
          actor->mode = 9;
          continue;
        }
        break;

      case 9:
        g_dwSpyroRequestMask |= 0x80002000;
        TickCountdownByFrameDelta(&st->u58, 4);
        if (st->u58 < 0x10) {
          g_nGenericCountdown = sixteen - st->u58;
          if (g_nGenericCountdown >= 0x10) {
            g_nGenericCountdown = 0xF;
          }
        }
        if (st->u58 == 0) {
          g_abLevelGemPercent[g_nLevelIntroIndex] = 1;
          g_nCreditsPageIndex = 0;
          g_nCurrentWorldId[0] = 2;
          g_nGamestate = GS_GEM_CUTSCENE;
          g_nGamePaused = 1;
          g_nGameplayBlocked = 0;
          g_nSpyroDrawSuppressed = 0;
          *g_pPathTableHead = 0;
        }
        break;

      case 10:
        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(
                              PATH_X((unsigned char *)st->path, 1) -
                                  actor->posX,
                              PATH_Y((unsigned char *)st->path, 1) -
                                  actor->posY,
                              0),
                          7, 0x14, 1) != 0) {
          MoveActorAlongHeading(actor, 0xF0, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, PATH_PT((unsigned char *)st->path, 1)) <
            0x320) {
          st->phase = 2;
          st->path->cur = 2;
          ENTER_POSE(actor, 4);
        }
        break;

      case 11:
        GNASTY_FALL(actor, st);
        if (TrackEntityYawTowardSpyro(actor, 5, 0, 0) != 0) {
          ANIM_GO(actor, 4);
        }
        if (IsPointInRotatedBox(&SPY.posX, st->box88) != 0) {
          ModifyWorldChunkAnimFlags(0, 0xFC, 0);
          ANIM_GO(actor, 1);
          actor->mode = 6;
          continue;
        }
        break;

      case 12: {
        int h = ProbeActorGroundHeightRaised(actor, 0xBB8);
        int z;

        ANIM_GO(actor, 0);
        st->u1C -= 0x19;
        if (st->u1C < -0x190) {
          st->u1C = -0x190;
        }
        z = actor->posZ + st->u1C;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (st->timer == 0 && (unsigned)(dist - 0xE10) < 0x16A8 &&
            g_nSpyroAirborneFrames == 0) {
          st->throws = 0;
          actor->mode = 2;
          continue;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && dist < 0xE10) {
          ANIM_GO(actor, 3);
          actor->mode = 0xD;
          continue;
        }
        break;
      }

      case 13:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          ANIM_GO(actor, 0);
          actor->mode = 0xC;
          continue;
        }
        break;

      case 20:
        if (TickCountdownByFrameDelta(&st->u58, 4) != 0) {
          actor->mode = 5;
          continue;
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0, 0) != 0) {
          ANIM_GO(actor, 0);
        }
        break;

      case 21:
        if (TrackEntityYawTowardSpyro(actor, 6, 0, 0) != 0) {
          ANIM_GO(actor, 0);
        }
        if (TickCountdownByFrameDelta(&st->u58, 4) != 0 && SPY_DZ(actor) < 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x251C) {
          actor->mode = 5;
          continue;
        }
        if (dist < 0xE10) {
          ENTER_POSE_B(actor, 3);
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 169: a rolling wave that lays a trail and spawns shockwaves (143)
     * where it lands. */
    case 169: {
      L33State169 *st = actor->state;
      int s = actor->mode;

      if (s < 3) {
        int n;
        int i;

        if (s < 0) {
          goto out169;
        }

        CopyVector(o150, &actor->posX);
        if (actor->mode == 0) {
          st->h04 += g_nFrameStep;
          if (st->h04 >= 0xA) {
            Actor *o = g_pfnLevelSpawnActor(0xA8, actor);

            CopyVector(&o->posX, &actor->posX);
            o->subMode = 0;
            o->rotY = ArcTan2(st->u0C, st->u10, 0);
            o->rotZ = actor->rotZ;
            o->cullRadius = 0x50;
            st->h04 = 0;
          }
          actor->posX += (st->u0C * g_anCosineLut[actor->rotZ]) >> 12;
          actor->posY += (st->u0C * g_anSineLut[actor->rotZ]) >> 12;
          actor->posZ += st->u10;
          if (TickCountdownByFrameDelta(&st->h06, 2) != 0) {
            actor->mode = 1;
          }
          if (actor->posX < 0 || actor->posY < 0 || actor->posZ < 0) {
            actor->cullRadius = 0;
            actor->drawn = 0;
            actor->mode = 1;
            continue;
          }
          if (--st->h08 < 0) {
            st->h08 = 0;
          }
          if (st->h08 == 0 && CastRayWorldAndActors(o150, &actor->posX) != 0) {
            Actor *o = g_pfnLevelSpawnActor(0x8F, actor);
            L33State143 *p = o->state;

            p->u00 = 0x190;
            p->u04 = 0x12;
            o->scale = 0xBB80 / p->u00;
            if (o->scale < 5) {
              o->scale = 5;
            }
            if (o->scale >= 0x80) {
              o->scale = 0x7F;
            }
            actor->mode = 1;
            PlayActorMeshVoice(actor, 0, 0);
          }
          if (actor->mode == 1) {
            actor->cullRadius = 0;
            actor->drawn = 0;
          }
        }
        n = 0;
        if (actor->mode == 0 && g_nMotionTrailRibbonCount < 3 && st->h0A < 0x78) {
          int k = st->h0A;

          CopyVector(st->trail[k].pos, &actor->posX);
          st->trail[k].age = 0;
          st->h0A++;
        }
        for (i = 0; i < st->h0A; i++) {
          st->trail[i].age += g_nFrameStep;
          if (st->trail[i].age < 0x51) {
            n++;
          }
        }
        if (n == 0 && actor->mode != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nMotionTrailRibbonCount < 3) {
          int k = g_nMotionTrailRibbonCount;

          g_anMotionTrailSliceCounts[k] = st->h0A;
          g_nMotionTrailRibbonCount = k + 1;
          g_apMotionTrailSlices[k] = st->trail;
        }
        if (actor->mode == 1) {
          actor->mode = 2;
        }
      }
    out169:
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Ambient flyer (FlyState): circles its anchor and pops near Spyro. */
    case 173: {
      FlyState *st = actor->state;

      if (actor->subMode == 0) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut_s[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200) {
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

    /* Type 181: loot vortex (LootVortexState). */
    case 181: {
      LootVortexState *st = actor->state;
      unsigned int ks = g_pActorListBase[st->key].mode;
      int kt = g_pActorListBase[st->key].type;
      int open = 0;

      if (ks >= 0x80 || (kt == 0x9F && ks == 2)) {
        open = 1;
      }
      if (actor->subMode == 0) {
        if (open) {
          actor->rotX = (unsigned short)g_anCosineLut[st->phase] >> 7;
          actor->rotY = (unsigned short)g_anSineLut_s[st->phase] >> 7;
          st->phase += g_nFrameStep * 2;
        }
        if ((ApproxDist2D(&actor->posX, &SPY.posX) < 0x200 &&
             ABS2(SPY_DZ(actor)) < 0x200 && open) ||
            (g_pActorListBase[st->key].mode >= 0x80 &&
             g_pActorListBase[st->key].type == 0xB1)) {
          int i;
          int c;

          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
          RefreshActorDerivedState(actor, 4);
          i = 0;
          c = 0x8080;
        spark:
          g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)c);
          i++;
          c += 0x1000000;
          if (i < 6) {
            goto spark;
          }
          g_nLevelEggCount = 1;
          actor->cullRadius = 0;
          actor->updateDist = 0xFF;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->visZone = 0xFF;
          actor->subMode = 1;
          st->mode = 1;
        }
        if (st->spark != 0xFF && actor->cullRadius != 0) {
          RefreshActorDerivedState(actor, 4);
          RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->spark].pos);
          AddVector(g_aRotBillboards[st->spark].pos, g_aRotBillboards[st->spark].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->spark].life < 5) {
            st->spark = 0xFF;
          }
        } else if (st->timer >= 0xF8 && actor->cullRadius != 0) {
          int s = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (s >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            st->spark = s;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
      }

      switch (st->mode) {
      case 0: {
        if (g_pActorListBase[st->key].type != 0xB1) {
          break;
        }
        if (g_pActorListBase[st->key].mode < 0x7F) {
          actor->cullRadius = 0;
          actor->updateDist = 0xFF;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->visZone = 0xFF;
          break;
        }
        actor->subMode = 1;
        st->mode = 1;
        g_nLevelEggCount = 1;
        break;
      }

      case 1:
        if (((unsigned)SPY.state < 2 || SPY.state == 0x15 || SPY.state == 2) &&
            IsPointInRotatedBox(&SPY.posX, st->pad) &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(g_pActorListBase[st->target].posX - SPY.posX,
                                        g_pActorListBase[st->target].posY - SPY.posY,
                                        0)) < 0x30) {
          g_dwSpyroRequestMask = 0x80002000;
          ResetSpyroLinearMotion();
          ZeroVector(&spyc->headLook);
          actor->subMode = 2;
          actor->rotX = 0;
          actor->rotY = 0;
          actor->rotZ = g_bSpyroPersistentEulerZ + 0x80;
          actor->drawn = 1;
          actor->cullRadius = 0x18;
          st->mode = 2;
          st->timer = 0;
          if (st->seen != 0) {
            if (g_nCurrentWorldId[st->seen] != 0) {
              int d;
              int r;

              st->timer = 0x80;
              d = AbsAngleDelta8(g_pActorListBase[st->target].rotZ,
                                (unsigned char)(SPY.bodyRotZ + 0x40));
              actor->rotY = 0xC0;
              r = g_bSpyroPersistentEulerZ + 0x40;
              actor->rotZ = r + d * (st->timer - 0x80) / 64;
            } else {
              g_nCurrentWorldId[st->seen] = 1;
            }
          }
          CopyVector(&actor->posX, &SPY.posX);
        }
        break;

      case 2:
        if (g_dwPadPressed & 0x40) {
          st->timer = 0xD2;
        }
        g_dwSpyroRequestMask = 0x80002000;
        actor->cullRadius = 0x18;
        st->timer += g_nFrameStep;
        g_nGameplayBlocked = 1;
        if (st->timer < 0xC0) {
          int *v = o150;

          v[0] = 0x200;
          v[1] = 0;
          v[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, v, v);
          AddVector(v, v, &g_pActorListBase[st->target].posX);
          SubtractVector(v, v, &SPY.posX);
          v[2] -= 0x100;
          RShiftVector3(v, 5);
          ScaleVector3Sat(v, v, st->timer);
          DivideVector3(v, v, 6);
          AddVector(v, v, &SPY.posX);
          v[2] += 0x100;
          v[2] += (unsigned short)g_anSineLut_s[(st->timer << 7) / 192] << 16 >> 19;
          CopyVector(&actor->posX, v);
          g_nCameraScriptStickRequestY[0] =
              (unsigned short)g_anSineLut_s[(st->timer << 7) / 192] << 16 >> 19;
          if (st->timer < 0x40) {
            int r;

            actor->rotY = -st->timer;
            r = g_bSpyroPersistentEulerZ + 0x80;
            actor->rotZ = r + st->timer * 4;
          } else if (st->timer < 0x80) {
            int r;

            actor->rotY = 0xC0;
            r = g_bSpyroPersistentEulerZ + 0x80;
            actor->rotZ = r + (st->timer - 0x40) * 3;
          } else {
            int d = AbsAngleDelta8(g_pActorListBase[st->target].rotZ,
                                  (unsigned char)(g_bSpyroPersistentEulerZ + 0x40));
            int r = g_bSpyroPersistentEulerZ + 0x40;

            actor->rotZ = r + d * (st->timer - 0x80) / 64;
          }
        } else if (st->timer < 0xD0) {
          int *w = w181;

          actor->rotZ = g_pActorListBase[st->target].rotZ;
          w[0] = 0x200 - (st->timer - 0xC0) * 8;
          w[2] = 0x140;
          w[1] = 0;
          RotateVectorByMatrix(actor->mtx, w, w);
          AddVector(&actor->posX, w, &g_pActorListBase[st->target].posX);
        }
        if (st->timer >= 0xD0 || !((unsigned)SPY.state < 2 || SPY.state == 0x15 ||
                                    SPY.state == 2 || SPY.state == 3)) {
          actor->cullRadius = 0;
          actor->drawn = 0;
          g_nGameplayBlocked = 0;
          g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, (void *)0x18);
          MarkActorCommittedKill(actor);
          st->mode = 3;
        }
        RefreshActorDerivedState(actor, 4);
        if ((short)(st->timer % 4) < 2) {
          g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)0x5008080);
        }
        break;

      case 3:
        PlaySoundEffect(g_pLevelSampleBankHeader[0x3C], 0, 0x10, 0);
        ModifyWorldChunkAnimFlags(st->snd, 0xFC, 0);
        g_nCameraScriptStickRequestY[1] = 0;
        g_nCameraScriptStickRequestY[0] = 0;
        g_nCameraScriptStickRequestY[-1] = 0;
        g_nLevelEggCount = 0;
        switch (st->snd) {
        case 0:
          g_nGnastyLootFallRefZCap = 0x563B;
          break;
        case 1:
          g_nGnastyLootFallRefZCap = 0x4E3C;
          break;
        case 2:
          g_nGnastyLootFallRefZCap = 0x663A;
          break;
        case 3:
          g_nGnastyLootFallRefZCap = 0x5E3A;
          break;
        }
        DespawnActorRecord(actor);
        continue;
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

        s392[0] = (GetRandomU32() & 0xFE) - 0x7F;
        s392[1] = (GetRandomU32() & 0xFE) - 0x7F;
        s392[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(s392, s392, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, s392, (int *)1);
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
            v310[0] = (GetRandomU32() & 0xFE) - 0x7F;
            v310[1] = (GetRandomU32() & 0xFE) - 0x7F;
            v310[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(v310, v310, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, v310, (int *)1);
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
        *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
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
          s448[0] = (GetRandomU32() & 7) + 0x10;
          s448[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, 0x10, ps, s448);
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
            c->posY += (g_anSineLut_s[i * 16] >> 4) + (GetRandomU32() & 0x1F) - 0xF;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = g_anCosineLut[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = g_anSineLut_s[i * 16] / 32 + (GetRandomU32() & 0x1F) - 0xF;
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
            c->posY += (g_anSineLut_s[(i << 8) / 12] >> 4) + (GetRandomU32() & 0x3F) - 0x1F;
            c->posZ += (GetRandomU32() & 0xFF) + 0x80;
            cs->vel[0] = (short)(g_anCosineLut[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
            cs->vel[1] = (short)(g_anSineLut_s[(i << 8) / 12] / 56) + (GetRandomU32() & 0x1F) - 0xF;
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
                          (0x1000 - ApproxDist2D(&actor->posX, &SPY.posX)) /
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

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case 329: {
      L16Pod *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
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
          ZeroVector(s448);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, s448);
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
            CopyVector(s480, &st->prize->posX);
            s480[0] += g_anCosineLut[j * 43] >> 6;
            s480[1] += g_anSineLut_s[j * 43] >> 6;
            s480[2] += 0x28;
            col2[0] = g_anCosineLut[j * 43] >> 7;
            col2[1] = g_anSineLut_s[j * 43] >> 7;
            col2[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, s480, col2);
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

    /* Type 414: turns toward the actor it follows. */
    case 414: {
      Actor **st = actor->state;

      TurnActorYawTowardAngle(actor,
                    ArcTan2((*st)->posX - actor->posX,
                                  (*st)->posY - actor->posY, 0),
                    4, 0, 0);
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
          s448[0] = (g_anCosineLut[bearing] * 3) >> 4;
          s448[1] = (g_anSineLut_s[bearing] * 3) >> 4;
          s448[2] = 0;
          s480[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[1] = (unsigned short)g_anSineLut_s[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(s496, s480, st->radius - 1);
          LShiftVector3(s480, 1);
          arm = st->radius - 1;
          s496[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          s512[0] = s448[0] * g_anCosineLut[arm * 2];
          s512[1] = s448[1] * g_anCosineLut[arm * 2];
          s512[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(s480, s480, st->phase);
          SubtractVector(s496, s496, s480);
          swing = &g_anCosineLut[bearing];
          actor->posX = s448[0] * *swing;
          actor->posY = s448[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, s512);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, s448);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, s496);
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
