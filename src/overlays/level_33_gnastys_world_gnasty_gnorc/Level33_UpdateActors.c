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
 *   - `base = (char *)g_anSparxHealthBlock` before the loop: the balloon-stage address
 *     is a spilled equivalence rematerialised at its two uses, while the
 *     stage itself is read through the scalar g_nSparxHealth view.
 *   - Gnasty's camera blocks read the anchor path through a scalar view of
 *     the state block so the loads stay behind the scalar flag stores
 *     (A297/A302); state 8 writes the C4C flag word as g_nSpyroSwingEulerTargetY[19] so
 *     only the g_nCinematicActive store sinks to the jal.
 */

#include "actor.h"
#include "cutscene_id.h"
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

/* 256-entry sine and cosine tables. */
extern short g_anSineLut_s[];

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. The
 * clones that need a loop-top carrier for 0x10 declare `int sixteen`; here
 * it is the plain constant, so arm bodies read the same in every level. */
#define ANIM_RATE16 sixteen
#define sixteen 0x10

/* Type 143 state: the shockwave a trail bomb sets off. */
typedef struct ShockwaveState {
  int radius; /* 0x00 radius */
  int timer; /* 0x04 timer */
} ShockwaveState;

/* Type 169's trail point. */
typedef struct L33Trail {
  int pos[3]; /* 0x00 */
  int pad0C[3]; /* 0x0C */
  int age;    /* 0x18 */
} L33Trail;   /* 0x1C */

/* Type 169 state: a trail bomb (Gnasty's), drawn with a motion trail. */
typedef struct TrailBombState {
  L33Trail *trail; /* 0x00 */
  short spawnTimer;       /* 0x04 spawn timer */
  short lifeTimer;       /* 0x06 life timer */
  short landDelay;       /* 0x08 landing delay */
  short trailLen;       /* 0x0A trail length */
  int speed;         /* 0x0C speed */
  int climb;         /* 0x10 climb */
} TrailBombState;

extern L33Trail *g_apMotionTrailSlices[]; /* trails */

#define FLY_FLAGS(p, i) (*(int *)((char *)(p) + ((i) << 4) + 0x14))

/* Type 159 state: a lane patroller. */
typedef struct LanePatrollerState {
  PathData *path; /* 0x00 */
  int lane;       /* 0x04 lane; indexes g_anLevelEscapeRunCount */
  int slowTimer;       /* 0x08 slow-down timer */
  int pad0C;       /* 0x0C */
  int speed;       /* 0x10 speed */
  int knockHeading;       /* 0x14 knock-back heading */
  int knockSpeed;       /* 0x18 knock-back speed */
  int knockZSpeed;       /* 0x1C vertical speed */
  int endNodeBack;       /* 0x20 end node going back */
  int endNodeFwd;       /* 0x24 end node going forward */
  TriggerBox box; /* 0x28 trigger box */
  int pad40[8];
  int partner;       /* 0x60 partner actor index, -1 none; level data: ACTOR_TYPE_LOOT_VORTEX */
} LanePatrollerState;

extern int g_anLevelEscapeRunCount[]; /* laps per lane */

/* Type 161 state (Gnasty Gnorc). */
typedef struct GnastyState {
  PathData *path;            /* 0x00 current escape path */
  PathData *path2;           /* 0x04 the other escape path */
  PathData *path3;           /* 0x08 camera reference path */
  int pad0C;                  /* 0x0C */
  int pad10;                  /* 0x10 */
  int knockHeading;                  /* 0x14 knock-back heading */
  int speed;                  /* 0x18 speed */
  int vSpeed;                  /* 0x1C vertical speed */
  int queuedJumpSpeed;                  /* 0x20 queued jump speed */
  int pad24[12];              /* 0x24 */
  int unread54;                  /* 0x54 */
  int stateTimer;                  /* 0x58 timer */
  int phase;                /* 0x5C fight phase */
  int timer;                /* 0x60 throw timer */
  int pad64;                  /* 0x64 */
  int throws;               /* 0x68 bombs thrown */
  int passedPt6;                  /* 0x6C latched on nearing path point 6 (mode 0xB once per phase) */
  TriggerBox box70;             /* 0x70 trigger box */
  TriggerBox box88;             /* 0x88 trigger box */
  TriggerBox boxA0;             /* 0xA0 trigger box */
  int padB8;                  /* 0xB8 */
  int key2;                 /* 0xBC actor index, -1 none; level data: ACTOR_TYPE_LOOT_VORTEX */
  int key;                  /* 0xC0 actor index; level data: ACTOR_TYPE_FOLLOWER */
  int seen;                 /* 0xC4 */
  unsigned char *home;      /* 0xC8 */
  int camTimer;             /* 0xCC */
  int voice;                  /* 0xD0 */
  int drop[3];              /* 0xD4 key actors dropped while hit */
  L33Trail trails[3][150];  /* 0xE0 one per live bomb */
} GnastyState;

/* Gravity toward the floor under the actor. */
#define GNASTY_FALL(a, st)                                                     \
  {                                                                            \
    int h_ = ProbeActorGroundHeightRaised(a, 0xBB8);                                          \
    int z_;                                                                    \
                                                                               \
    (st)->vSpeed -= 0x19;                                                         \
    if ((st)->vSpeed < -0x190) {                                                  \
      (st)->vSpeed = -0x190;                                                      \
    }                                                                          \
    z_ = (a)->posZ + (st)->vSpeed;                                                \
    if (z_ < h_) {                                                             \
      (a)->posZ = h_;                                                          \
    } else {                                                                   \
      (a)->posZ = z_;                                                          \
    }                                                                          \
  }

extern unsigned char g_abSpyroPersistentEuler; /* SPY.bodyRotX, absolute form */
#define PATH_END(p) ((int *)((char *)(p) + (((p)->count << 4) - 8)))

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
  int eggArcBase[3];
  int eggArcOffset[3];
  int svD[3];
  int eggViewPos[3];
  int svF[3];
  int acc[3][3];
  int bouncePos[3];
  int bounceNormal[3];
  int homingStep[3];
  int sparxToTarget[3];
  int fx[6];
  int col[3];
  int spyroDelta[3];
  int nearestNode[2];
  int sightPoint[3];
  int o150[3];
  int spyroFloor[3];
  int pathHit[2];
  int settleOffset[3];
  int sparkleTwinkle[3];
  int shardTwinkle[3];
  int ps[6];
  int s448[3];
  int s480[3];
  int col2[3];
  int letterSlot[3];
  int letterSwingRest[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, ANIM_TICK_NORMAL);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? ANIM_TICK_EXTRA | ANIM_TICK_HALF_RATE : ANIM_TICK_EXTRA);
  }

  actorList = g_apActiveActorList;
  base = (char *)g_anSparxHealthBlock;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    spyc = (SpyroObj *)&g_anSpyroWorldPos;
    oneq = 1;
    one = 1;

    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & ACTOR_ANIM_DONE;
    g_nActorAnimNewFrame = actor->animFlags & ACTOR_ANIM_NEW_FRAME;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {

    /* Tethered follower (FollowState): rides a parent actor until the parent
     * goes dormant, then takes the parent's transform over and re-points the
     * other followers of that parent at itself. */
    case ACTOR_TYPE_FOLLOWER: {
      FollowState *st = actor->state;
      Actor *parent = &g_pActorListBase[st->parent];

      if (st->detached == 0) {
        if (st->trackPos != 0) {
          SubtractVector(st->pos, st->pos, &parent->posX);
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
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

          actor->mode = ACTOR_SPAWN_MODE;
          ANIM_RESET(actor, ACTOR_SPAWN_ANIM);
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
          if (TICK_DOWN(st->release) != 0) {
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
            MarkActorLiveKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = SFX_OVERRIDE_PITCH;
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
                ((FollowState *)parent->state)->parent == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = SFX_OVERRIDE_PITCH;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->voiceRow);
          }
        }
      }

      break;
    }

    /* Butterfly (ButterflyState). */
    case ACTOR_TYPE_BUTTERFLY: {
      ButterflyState *st = (ButterflyState *)actor->state;

      if (g_nSparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, (int *)SPY_FROM_SPARX(base)) < BUTTERFLY_CALL_RANGE) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < BUTTERFLY_CALL_DZ) {
          if (g_pSparxActor == 0) {
            if (g_nSparxHealth >= SPARX_GONE) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);

              g_nSparxHealth = SPARX_GREEN;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            SparxState *cs = g_pSparxActor->state;

            if (cs->target == 0) {
              if (WrapAngleDelta8(SPY.bodyRotZ,
                                ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, ATAN_ANGLE8)) < 0) {
                st->circleDir = 1;
              } else {
                st->circleDir = -1;
              }

              g_pSparxActor->subMode = SPARX_SUB_CHASE;
              cs->target = actor;
              cs->timer = RandomIntInRange(SPARX_CHASE_MIN_FRAMES, SPARX_CHASE_MAX_FRAMES);

              actor->mode = BUTTERFLY_CIRCLE;
            }
          }
        }
      }

      if (actor->cullRadius & ACTOR_CULL_ALWAYS_BIT)
        break;

      switch (actor->mode) {
      case BUTTERFLY_FLUTTER: {
        if (TICK_DOWN(st->offscreenTimer) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TICK_DOWN(st->headingTimer) != 0) {
            int r = RandomSignedInRange(30, 90);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->headingTimer = RandomIntInRange(60, 140);
          }

          if (TICK_DOWN(st->climbTimer) != 0) {
            st->vz = RandomSignedInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < BUTTERFLY_FLUTTER_MIN_HEIGHT) {
            st->vz = RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (BUTTERFLY_FLUTTER_MAX_HEIGHT < refHeight && refHeight < BUTTERFLY_DROP_HEIGHT) {
            st->vz = -RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, YAW_STEP_IN_BAND);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, MOVE_LIFT);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, st->home) > BUTTERFLY_LEASH) {
            int angle = ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, ATAN_ANGLE8);

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
        SteerActorAroundPoint(actor, &SPY.posX, BUTTERFLY_CIRCLE_RADIUS,
                      WrapAddAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, ATAN_ANGLE8),
                                    st->circleDir * 8),
                      8, 0x82, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);

        if (g_pSparxActor == 0 || g_pSparxActor->subMode == SPARX_SUB_PARKED) {
          actor->mode = BUTTERFLY_FLUTTER;
        } else {
          int refHeight;

          if (TICK_DOWN(st->climbTimer)) {
            st->vz = RandomSignedInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < BUTTERFLY_CIRCLE_MIN_HEIGHT) {
            st->vz = RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (BUTTERFLY_CIRCLE_MAX_HEIGHT < refHeight && refHeight < BUTTERFLY_DROP_HEIGHT) {
            st->vz = -RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }
        }
        break;
      }
      }

      break;
    }

    /* Dragon egg (EggRiseState). */
    case ACTOR_TYPE_DRAGON_EGG: {
      EggRiseState *st;

      st = (EggRiseState *)actor->state;

      if (actor->mode != EGG_CARRIED && actor->mode < EGG_COLLECTED) {
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (SPY.state != SPYRO_ST_IDLE && SPY.state != SPYRO_ST_CHARGE_BONK) {
          actor->mode = EGG_COLLECTED;
        }
      }
      if ((unsigned int)(actor->mode - EGG_SPIRAL_UP) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, EGG_SPARK_COLOR);
      }

      switch (actor->mode) {
      case EGG_RELEASED:
        g_nCinematicActive = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode = actor->mode + 1;
        break;

      case EGG_SPIRAL_UP:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < EGG_SPIRAL_FRAMES) {
          int step = g_nFrameStep << 2;
          st->radius = st->radius + step;
          st->angle = st->angle + step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX += (LookupCosine(st->angle << 4) * (short)st->radius) >> 12;
          actor->posY += (LookupSine(st->angle << 4) * (short)st->radius) >> 12;
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
          svA[2] = svA[2] + EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->apex, svA);
          VectorToSVectorShr2(st->centre, &actor->posX);
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_TO_SPYRO:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < EGG_FLIGHT_FRAMES) {
          SVectorToVectorShl2(eggArcBase, st->centre);
          SVectorToVectorShl2(eggArcOffset, st->apex);
          SubtractVector(eggArcOffset, eggArcOffset, eggArcBase);
          ScaleVector3Sat(eggArcOffset, eggArcOffset, st->phase);
          RShiftVector3(eggArcOffset, 6);
          AddVector(&actor->posX, eggArcBase, eggArcOffset);
          actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
        } else {
          CopyVector(svD, &SPY.posX);
          svD[0] = svD[0] + (LookupCosine(SPY.bodyRotZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(SPY.bodyRotZ << 4) >> 3);
          svD[2] = svD[2] + EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->centre, svD);
          st->angle = SPY.bodyRotZ - 0x40;
          st->radius = EGG_SPIRAL_RADIUS;
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_SPIRAL_DOWN:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < EGG_SPIRAL_FRAMES) {
          int step = g_nFrameStep << 2;
          st->radius = st->radius - step;
          st->angle = st->angle - step;
          SVectorToVectorShl2(&actor->posX, st->centre);
          actor->posX += (LookupCosine(st->angle << 4) * (short)st->radius) >> 12;
          actor->posY += (LookupSine(st->angle << 4) * (short)st->radius) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 2);
          }
        } else {
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_COLLECTED:
        SubtractVector(eggViewPos, &actor->posX, &CAM.posX);
        WorldToCameraRotate(eggViewPos, eggViewPos);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, eggViewPos, 0);
        g_nCinematicActive = 0;
        ZeroVector(&SPY.headLook);
        CommitActorKillGems(0, st->owner);
        g_nTotalEggsRecovered = g_nTotalEggsRecovered + 1;
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        break;

      default:
        break;
      }
      break;
    }

    /* Trail-bomb puff: replays its animation subMode times, then shares the
     * one-shot effects' exit. */
    case ACTOR_TYPE_TRAIL_BOMB_PUFF:
      if (actor->subMode != 0) {
        actor->subMode--;
        actor->frame = 0;
        actor->nextFrame = 1;
        actor->frameProgress = 0;
      }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD1_ENTER_FX:
    case ACTOR_TYPE_HAZARD1_DEATH_FX:
    case ACTOR_TYPE_HAZARD2_ENTER_FX:
    case ACTOR_TYPE_HAZARD2_DEATH_FX:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Explosion shard with a ground bounce (TumbleState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1: {
      TumbleState *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {

      if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
        int *nrm = &g_anCollisionHitNormal;
        int along;
        register int push asm("$7"); /* a3, as in the original (A374) */

        SetVector3Magnitude(nrm, FIXED_ONE);
        along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionHitNormalY) +
                (st->vel[2] * g_nCollisionHitNormalZ);
        push = along >> 11;

        if (push < 0) {
          ScaleVector3ByRatio(nrm, FIXED_ONE, (along >> 13) - push);
          st->vel[0] += nrm[0];
          st->vel[1] += AS_I32(g_nCollisionHitNormalY);
          st->vel[2] += AS_I32(g_nCollisionHitNormalZ);
        }
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= DEBRIS_GRAVITY;
      if (st->vel[2] < -DEBRIS_MAX_FALL) {
        st->vel[2] = -DEBRIS_MAX_FALL;
      }
      actor->posZ += st->vel[2];
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      if (!(st->life & 3)) {
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)svD);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
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

      st->vel[2] -= DEBRIS_GRAVITY;
      if (st->vel[2] < -DEBRIS_MAX_FALL) {
        st->vel[2] = -DEBRIS_MAX_FALL;
      }
      actor->posZ += st->vel[2];

      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      st->life -= 1;
      break;
    }

    /* Collectibles: extra life (14), Sparx food (15), gems (83..87)
     * (CollectibleState). ->subMode is the outer state, st->mode the sub-state. */
    case ACTOR_TYPE_EXTRA_LIFE:
    case ACTOR_TYPE_SPARX_FOOD:
    case ACTOR_TYPE_GEM_1:
    case ACTOR_TYPE_GEM_2:
    case ACTOR_TYPE_GEM_5:
    case ACTOR_TYPE_GEM_10:
    case ACTOR_TYPE_GEM_25: {
      CollectibleState *st = actor->state;
      int toSpyro;

      toSpyro = ApproxDist2D(&actor->posX, &SPY.posX);

      if (st->timer < 0xFA) {
        st->timer += g_nFrameStep;
      }

      if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
        if (st->attach != ROT_BILLBOARD_NONE) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset5,
                          g_aRotBillboards[st->attach].pos);
          } else {
            RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset3,
                          g_aRotBillboards[st->attach].pos);
          }
          AddVector(g_aRotBillboards[st->attach].pos,
                        g_aRotBillboards[st->attach].pos, &actor->posX);

          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = ROT_BILLBOARD_NONE;
          }
        } else if (st->timer >= ROT_BILLBOARD_GLINT_TICK) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);

          if (handle >= 0 && toSpyro < ROT_BILLBOARD_ATTACH_RANGE) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x1F) + 0x21;
        }
      }

      if (actor->drawn != 0 || toSpyro <= COLLECT_IDLE_RANGE || actor->subMode != COLLECT_REST ||
          st->mode != COLLREST_STILL) {
        switch (actor->subMode) {
        case COLLECT_SETTLE:
          /* Idle: sample the floor normal and face along it. */
          CopyVector(svA, &actor->posX);
          svA[2] += 0x400;
          FindGroundHeightBelow(svA, 0x10000);
          {
            int *nrm = &g_anCollisionHitNormal;

            st->lean = -ArcTan2_8bit(
                IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionHitNormalZ * g_nCollisionHitNormalZ)),
                g_nCollisionHitNormalY);
            st->roll = -ArcTan2_8bit(g_nCollisionHitNormalZ, nrm[0]);
          }

          if (st->lean != 0 || st->roll != 0) {
            actor->rotZ = 0;
          }
          actor->subMode = COLLECT_REST;
          goto enemy_pose;

        case COLLECT_REST: {

          if (st->mode == COLLREST_STILL) {
          } else if (st->mode == COLLREST_SNAP) {
            SnapActorToGround(actor);
            EncodeHitNormalToActorGroundCode(actor);
            if (actor->type >= ACTOR_TYPE_GEM_1) {
              {
                int *nrm = &g_anCollisionHitNormal;

                st->lean = -ArcTan2_8bit(
                    IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionHitNormalZ * g_nCollisionHitNormalZ)),
                    g_nCollisionHitNormalY);
                st->roll = -ArcTan2_8bit(g_nCollisionHitNormalZ, nrm[0]);
              }
            }
          } else if (st->mode == COLLREST_ON_TRI) {
            UnpackWorldCollisionTri(st->groundTri, acc[0]);
            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);
            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeHitNormalToActorGroundCode(actor);
          } else if (st->mode == COLLREST_FIND_TRI) {
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              st->groundTri = g_nCollisionHitTriIndex;
              st->mode = COLLREST_ON_TRI;
            }
          }

          if (toSpyro < SPARX_FETCH_RANGE) {
            int top = actor->posZ + SPYRO_RADIUS;
            register int spyZ asm("$2") = SPY.posZ; /* v0, as in the original (A374) */
            int dz = spyZ - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < SPARX_FETCH_DZ && g_pSparxActor != 0 && g_nSpyroAirborneFrames == 0 &&
                CheckLineOfSightSegment(&g_pSparxActor->posX, &actor->posX) != 0) {
              {
                SparxState *sparx = g_pSparxActor->state;

                if (sparx->target == 0) {
                  if (GetSoundVoiceStatusByOwner(g_pSparxActor,
                                    g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) ==
                      0) {
                    g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
                    g_nSfxOverrideVolR = SPARX_BUZZ_VOLUME;
                    g_nSfxOverrideVolL = SPARX_BUZZ_VOLUME;
                    PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);
                  }
                  g_pSparxActor->subMode = SPARX_SUB_FETCH;
                  sparx->target = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

        case COLLECT_BOUNCE: {
          /* Knocked back: arc away, bounce, then settle. */
          int *at;
          int dist;

          ScaleVector3Sat(bouncePos, (int *)st, g_nFrameStep);
          RShiftVector3(bouncePos, 1);
          dist = VectorLength(bouncePos, VEC_LEN_XYZ);

          if (dist > COLLECT_BOUNCE_MAX_STEP) {
            ScaleVector3ByRatio(bouncePos, dist, COLLECT_BOUNCE_MAX_STEP);
          }

          at = &actor->posX;
          AddVector(bouncePos, at, bouncePos);

          if (st->vel[2] > -COLLECT_BOUNCE_MAX_FALL) {
            st->vel[2] -= g_nFrameStep * COLLECT_BOUNCE_GRAVITY;
          }

          if (bouncePos[2] < 0) {
            if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          bouncePos[2] += COLLECT_RADIUS;

          if (CollideSphereWithWorldAndActors(bouncePos, COLLECT_RADIUS, COLLECT_RADIUS) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(bounceNormal, &g_anCollisionHitNormal);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= COLLECT_RADIUS;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(bouncePos, 0x400);
              int slope = (signed char)ArcTan2_8bit(bounceNormal[2], VectorLength(bounceNormal, VEC_LEN_XY));

              if ((bouncePos[2] - COLLECT_REST_DROP) < floor && slope < COLLECT_REST_MAX_SLOPE) {
                actor->subMode = COLLECT_REST;
                CopyVector(at, bouncePos);
                actor->posZ = floor;
                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((bounceNormal[0] * bounceNormal[0]) + (bounceNormal[2] * bounceNormal[2])),
                      bounceNormal[1]);
                  st->roll = -ArcTan2_8bit(bounceNormal[2], bounceNormal[0]);
                  if (st->lean != 0 || st->roll != 0) {
                    actor->rotZ = 0;
                  }
                }
                EncodeHitNormalToActorGroundCode(actor);
              } else {
                st->bounce += 1;
              }

              if (st->bounce == 0) {
                goto enemy_settle;
              }
            }

            g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
            g_nSfxOverrideVolL = COLLECT_BOUNCE_VOLUME >> (3 - st->bounce);
            g_nSfxOverrideVolR = COLLECT_BOUNCE_VOLUME >> (3 - st->bounce);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);

            if (ReflectVectorOffNormal((int *)st, bounceNormal, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            bouncePos[2] -= 0xF0;
            CopyVector(at, bouncePos);
            FindGroundHeightBelow(bouncePos, 0x10000);
            EncodeHitNormalToActorGroundCode(actor);
          }

        enemy_settle:
          if (toSpyro < SPARX_FETCH_RANGE) {
            int top = actor->posZ + SPYRO_RADIUS;
            register int spyZ asm("$2") = SPY.posZ; /* v0, as in the original (A374) */
            int dz = spyZ - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < SPARX_FETCH_DZ && g_pSparxActor != 0 && g_nSpyroAirborneFrames == 0 &&
                st->bounce < 3) {
              {
                SparxState *sparx = g_pSparxActor->state;

                if (sparx->target == 0) {
                  if (GetSoundVoiceStatusByOwner(g_pSparxActor,
                                    g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) ==
                      0) {
                    g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
                    g_nSfxOverrideVolR = SPARX_BUZZ_VOLUME;
                    g_nSfxOverrideVolL = SPARX_BUZZ_VOLUME;
                    PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);
                  }
                  g_pSparxActor->subMode = SPARX_SUB_FETCH;
                  sparx->target = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;

        }

          goto enemy_pose;

        case COLLECT_HOMING:
          /* Fetched by Sparx: pulled toward Spyro, then collected. */
          if (st->timer >= COLLECT_HOMING_DONE) {
            CopyVector(&actor->posX, &SPY.posX);
          } else {
            SubtractVector(homingStep, &spy->posX, (int *)st);
            RShiftVector3(homingStep, 5);

            if (VectorLength(homingStep, VEC_LEN_XYZ) > COLLECT_HOMING_SNAP_STEP) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = COLLECT_HOMING_DONE;
            } else {
              ScaleVector3Sat(homingStep, homingStep, st->timer);
              AddVector(&actor->posX, (int *)st, homingStep);
              actor->posZ += (short)(g_anSineLut_s[st->timer * 4] / 12);
            }
          }

          actor->rotX += (unsigned char)(st->lean - 7);
          EBB_BREAK();
          actor->rotY += (unsigned char)(st->roll - 7);
          EBB_BREAK();
          actor->rotZ += (unsigned char)(st->spin - 7);
          goto enemy_pose;

        case COLLECT_INERT:
          continue;

        default:
        enemy_pose:
          if (actor->subMode < COLLECT_HOMING) {
            if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) + (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut_s[st->spin] << 16 >> 25) + (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
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

          if ((st->timer > COLLECT_ARM_FRAMES && toSpyro < COLLECT_TOUCH_RANGE &&
               (unsigned int)((SPY.posZ - actor->posZ) + COLLECT_TOUCH_BELOW) < COLLECT_TOUCH_BELOW + COLLECT_TOUCH_ABOVE) ||
              (actor->subMode == COLLECT_HOMING && st->timer > COLLECT_HOMING_COLLECT_TICK)) {
            if (g_pSparxActor != 0) {
              SparxState *sparx = g_pSparxActor->state;

              if (sparx->target == actor) {
                sparx->target = 0;
              }
            }

            if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
              RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
              g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 8);
              g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x10);
              if (g_nExtraLives < MAX_EXTRA_LIVES) {
                g_nExtraLives += 1;
              }
              CommitActorKillGems(0, actor);
              DespawnActorRecord(actor);
              continue;
            }

            if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
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

    /* Sparx (SparxState): hovers beside Spyro, glows by health, catches
     * butterflies (health + 1) and fetches nearby pickups. Despawns at 0. */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = actor->state;

      if (g_nSparxHealth >= SPARX_BLUE) {
        svF[0] = 0;
        svF[1] = SPARX_WING_SPAN;
        svF[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);
        svF[0] = 0;
        svF[1] = -SPARX_WING_SPAN;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);
      }

      if (g_nSparxHealth >= SPARX_GOLD) {
        FxNode *fx = st->glow;

        if (fx == 0) {
          fx = AllocWorldSparkle();
          st->glow = fx;
          if (fx != 0) {
            fx->anchor = &actor->posX;
            st->glow->size = SPARX_GLOW_START_SIZE;
            st->glow->offX = 0;
            st->glow->offY = 0;
            st->glow->offZ = 0;
            st->glow->r = 0xC0;
            st->glow->g = 0xC0;
            st->glow->b = 0x60;
            st->glow->points = 9;
            st->glow->desc = &g_anSparkleGlowDesc;
          }
        } else {
          fx->size += SPARX_GLOW_GROW;
          if (st->glow->size > SPARX_GLOW_MAX_SIZE) {
            st->glow->size = SPARX_GLOW_MAX_SIZE;
          }
        }
      } else if (st->glow != 0) {
        ClearWord(st->glow);
        st->glow = 0;
      }

      if (g_nSparxHealth <= SPARX_GONE) {
        if (st->target != 0 && st->target->type == ACTOR_TYPE_BUTTERFLY) {
          st->target->mode = BUTTERFLY_FLUTTER;
        }
        g_pSparxActor = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->target != 0 &&
          ApproxDist2D(&actor->posX, (int *)SPY_FROM_SPARX(base)) >
              SPARX_TARGET_LEASH) {
        st->timer = 0;
      }

      if (actor->subMode != SPARX_SUB_PARKED) {
        if (st->target != 0) {
          switch (actor->subMode) {
          case SPARX_SUB_CHASE: {
            /* Closing in on the butterfly. */
            CollectibleState *pk = st->target->state;
            int high = actor->nextAnim & 1;
            int dist;
            int reach;
            int limit;

            CopyVector(svA, &st->target->posX);

            if (high == 0) {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              svA[1] -=
                  (unsigned short)g_anSineLut_s[st->target->rotZ] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              svA[1] -=
                  (unsigned short)g_anSineLut_s[st->target->rotZ] << 16 >> 22;
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
            dist = VectorLength(svA, VEC_LEN_XYZ);

            if (high == 0 &&
                (TICK_DOWN(st->timer) != 0 ||
                 (st->timer < SPARX_CATCH_EARLY_FRAMES &&
                  AbsAngleDelta8(
                      (actor->rotZ + (pk->vel[0] * 0x6E)) & 0xFF,
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
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(svA, VEC_LEN_XY), svA[2], ATAN_ANGLE8);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case SPARX_SUB_EAT:
            g_nSparxHealth += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
            actor->subMode += 1;
            DespawnActorRecord(st->target);
            break;

          case SPARX_SUB_EAT_DONE:
            if (g_nActorAnimDone != 0) {
              if (actor->nextAnim != actor->nextAnim - 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim - 1);
              }
              st->target = 0;
              actor->subMode = SPARX_SUB_CHASE;
            }
            break;

          case SPARX_SUB_FETCH: {
            /* Fetching a pickup: once close it homes in on Spyro. */
            CollectibleState *pk = st->target->state;
            int dist;

            SubtractVector(sparxToTarget, &st->target->posX, &actor->posX);
            dist = VectorLength(sparxToTarget, VEC_LEN_XYZ);

            if (dist < SPARX_CATCH_RANGE) {
              pk->timer = 0;
              pk->lean = GetRandomU32() & 0xE;
              pk->roll = GetRandomU32() & 0xE;
              pk->spin = GetRandomU32() & 0xE;
              CopyVector((int *)pk, &st->target->posX);
              st->target->subMode = COLLECT_HOMING;

              if (st->target->type == ACTOR_TYPE_EXTRA_LIFE) {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorExtraLife);
              } else if (st->target->type == ACTOR_TYPE_SPARX_FOOD) {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorSparxFood);
              } else {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_adwActorBurstColors[st->target->type]);
              }
              st->target = 0;
              EBB_BREAK();

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[SFX_GEM_BOUNCE]) == 0) {
                g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
                g_nSfxOverrideVolR = SPARX_HANDOVER_VOLUME;
                g_nSfxOverrideVolL = SPARX_HANDOVER_VOLUME;
                PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);
              }
            } else {
              int lim = SPARX_FETCH_STEP_FAR;

              if (lim < dist) {
                ScaleVector3ByRatio(sparxToTarget, dist, lim);
              } else {
                ScaleVector3ByRatio(sparxToTarget, dist, SPARX_FETCH_STEP_NEAR);
              }
              AddVector(&actor->posX, &actor->posX, sparxToTarget);
              TurnActorYawTowardAngle(actor, ArcTan2(sparxToTarget[0], sparxToTarget[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
              actor->rotY = ArcTan2(VectorLength(sparxToTarget, VEC_LEN_XY), sparxToTarget[2], ATAN_ANGLE8);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* No target: hover at a jittered offset beside Spyro. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            svF[0] = -SPARX_HOVER_BEHIND;
            svF[1] = (GetRandomU32() & 0x310) - 0x188;
            svF[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + SPARX_CHARGE_HOVER_MIN_Z
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = svF[0];
            st->drift[1] = svF[1];
            st->drift[2] = svF[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              svF[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              svF[1] = st->drift[1];
              svF[2] = st->drift[2];
              if (svF[2] < SPARX_CHARGE_HOVER_MIN_Z) {
                svF[2] = SPARX_CHARGE_HOVER_MIN_Z;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                svF[0] = st->drift[0] - SPARX_FREE_LOOK_PULLBACK;
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

            if (svF[2] > SPARX_MAX_PITCH) {
              svF[2] = SPARX_MAX_PITCH;
            }
            if (svF[2] < -SPARX_MAX_PITCH) {
              svF[2] = -SPARX_MAX_PITCH;
            }

            actor->rotX = 0;
            actor->rotY = svF[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, YAW_HOLD_IN_BAND);
          }

          switch (SPY.sparxHealth) {
          case SPARX_GREEN:
            if (actor->nextAnim != SPARX_ANIM_GREEN && actor->anim != SPARX_ANIM_GREEN) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_GREEN);
            }
            break;

          case SPARX_BLUE:
            if (actor->nextAnim != SPARX_ANIM_BLUE && actor->anim != SPARX_ANIM_BLUE) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_BLUE);
            }
            break;

          case SPARX_GOLD:
            if (actor->nextAnim != SPARX_ANIM_GOLD && actor->anim != SPARX_ANIM_GOLD) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_GOLD);
            }
            break;
          }
        }

        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      }

      break;
    }

    case ACTOR_TYPE_SHOCKWAVE: {
      ShockwaveState *st = actor->state;

      st->radius += dt * 14;
      actor->scale = 0xBB80 / st->radius;
      if (actor->scale < 5) {
        actor->scale = 5;
      }
      if (actor->scale >= 0x80) {
        actor->scale = 0x7F;
      }
      if (st->timer != 0) {
        RaiseSpyroTriggerInRadius(&actor->posX, st->radius, SPYRO_EV_HIT | SPYRO_EV_CHARGE_HIT);
      }
      if (actor->nextAnim == SHOCKWAVE_ANIM_FADE && g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TICK_DOWN(st->timer) != 0) {
        ANIM_SET(actor, SHOCKWAVE_ANIM_FADE);
      }
      continue;
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
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
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
        g_pfnLevelEmitParticles(1, EMIT_SPARK_RGB, fx, (int)col);
      }
      break;
    }

    /* Type 159: a path patroller that turns back toward Spyro. */
    case ACTOR_TYPE_LANE_PATROLLER: {
      LanePatrollerState *st = actor->state;
      int dist;

      SubtractVector(spyroDelta, &SPY.posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &SPY.posX);
      if (dist < 0x32C8) {
        dist = VectorLength(spyroDelta, VEC_LEN_XY);
      }

      if (actor->flags != 0 && actor->mode != LANEPAT_KNOCKED) {
        st->knockSpeed = 0xFA;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knockSpeed = 0x145;
        }
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, ATAN_ANGLE8),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->knockZSpeed = 0x28;
        ENTER_POSE(actor, LANEPAT_KNOCKED);
      }
      actor->flags = 0;

      if (st->lane == 1 && actor->cullRadius == 0 &&
          CHUNK_ANIM_STEP(GetWorldChunkAnimState(LANEPAT_CHUNK_REVEAL)) > 0) {
        actor->cullRadius = 0x28;
        g_pActorListBase[st->partner].cullRadius = 0x28;
      }

      if (actor->nextAnim == LANEPAT_WATCH && actor->cullRadius != ACTOR_CULL_HIDDEN) {
        if (actor->nextFrame == 1 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[LANEPAT_VOICE_STAND_EARLY]) == 0) {
          PlayActorMeshVoice(actor, LANEPAT_VOICE_STAND_EARLY, 0);
        }
        if (actor->nextFrame == 0xA && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[LANEPAT_VOICE_STAND_LATE]) == 0) {
          PlayActorMeshVoice(actor, LANEPAT_VOICE_STAND_LATE, 0);
        }
      }

      switch (actor->mode) {
      case LANEPAT_WATCH:
        TrackEntityYawTowardSpyro(actor, 5, 0, YAW_HOLD_IN_BAND);
        if (dist < 0x1964 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                               : -SPY_DZ(actor) < 0x578)) {
          st->speed = 0x12C;
          if (st->lane == 0) {
            st->path->dir = 1;
            st->slowTimer = 0x6E;
          } else {
            st->path->dir = 1;
            st->slowTimer = 0x96;
          }
          st->slowTimer -= g_anLevelEscapeRunCount[st->lane] * 6;
          if (IsPointInRotatedBox(&SPY.posX, &st->box) != 0) {
            st->path->dir *= -1;
          }
          if (st->path->dir == -1) {
            st->path->cur = st->endNodeBack;
          } else {
            st->path->cur = st->endNodeFwd;
          }
          ENTER_POSE(actor, LANEPAT_RUN);
        }
        break;

      case LANEPAT_RUN: {
        PathData *p;
        int spd;
        int r;
        int h;
        int z;

        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX,
                        PATH_PT(st->path, st->path->cur));
          SnapActorToGround(actor);
        }
        if (TICK_DOWN(st->slowTimer) != 0) {
          int lo = 0xDC - g_anLevelEscapeRunCount[st->lane] * 6;

          st->speed -= 0xA;
          if (st->speed < lo) {
            st->speed = lo;
          }
        }
        spd = st->speed;
        if (FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_SPEED) {
          spd = FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_SPEED;
        }
        r = PivotActorAlongPath(actor, st->path, 0x1C2, spd, 0, 0xA, 0x28, PIVOT_TURN_ANY, MOVE_LIFT);
        h = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->knockZSpeed -= 0x1E;
        if (st->knockZSpeed < -0x190) {
          st->knockZSpeed = -0x190;
        }
        z = actor->posZ + st->knockZSpeed;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        if ((r & PATH_ARRIVED_FLAG) && (FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_JUMP) == PATH_JUMP_LOW) {
          st->knockZSpeed = 0x12C;
        }
        if ((st->path->dir == -1 && r == (st->endNodeBack | 0x100)) ||
            (st->path->dir == 1 && r == (st->endNodeFwd | 0x100))) {
          g_anLevelEscapeRunCount[st->lane]++;
          if (g_anLevelEscapeRunCount[st->lane] >= 6) {
            g_anLevelEscapeRunCount[st->lane] = 5;
          }
          ENTER_POSE(actor, LANEPAT_WATCH);
        }
        FindFarthestPathPointFromSpyro(st->path);
        FindNearestPathPointToSpyro(st->path, nearestNode);
        if (dist < 0x3000) {
          int a;
          int n;
          int i;

          a = AbsAngleDelta8(
              ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                            FLY_Y(st->path, st->path->cur) - actor->posY, ATAN_ANGLE8),
              actor->rotZ);
          n = 0;
          p = st->path;
          i = p->cur;
          while (i != nearestNode[0]) {
            i -= p->dir;
            i = (i + p->count) % p->count;
            n++;
          }
          if ((unsigned)(n - 1) < 7 && a < 0x10 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(SPY.posX - actor->posX,
                                          SPY.posY - actor->posY, ATAN_ANGLE8)) < 0x3A &&
              n < st->path->count >> 1) {
            st->path->dir = -st->path->dir;
            st->path->cur = (st->path->cur - st->path->dir + st->path->count) %
                            st->path->count;
          }
        }
        break;
      }

      case LANEPAT_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, 0xC, sixteen);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      if (actor->mode != LANEPAT_KNOCKED && st->partner != -1) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, &g_pActorListBase[st->partner].posX);
        ProbeActorGroundHeight(&g_pActorListBase[st->partner]);
        EncodeHitNormalToActorGroundCode(&g_pActorListBase[st->partner]);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 161: Gnasty Gnorc. Runs his escape paths, throws trail bombs
     * (169) at Spyro and drops the vault keys as he is hit. */
    case ACTOR_TYPE_GNASTY_GNORC: {
      GnastyState *st = actor->state;
      int dist;

      dist = ApproxDist2D(&actor->posX, &SPY.posX);
      TickActorFlameGlow(actor);

      if (actor->flags & ACTOR_HIT_FLAME) {
        if (st->phase == GNASTY_PHASE_MAIN &&
            ApproxDist2D(&actor->posX,
                          PATH_END(st->path)) < 0xBB8 &&
            actor->mode != GNASTY_FLAMED) {
          PathData *t;

          ModifyWorldChunkAnimFlags(GNASTY_CHUNK_ESCAPE, CHUNK_ANIM_RESUME, 0);
          g_pActorListBase[st->key].subMode = 1;
          st->camTimer = 0xA0;
          st->vSpeed = 0;
          st->queuedJumpSpeed = 0;
          st->passedPt6 = 0;
          st->phase++;
          t = st->path;
          st->path = st->path2;
          st->path2 = t;
          ENTER_POSE(actor, GNASTY_FLAMED);
        } else if (st->phase == GNASTY_PHASE_ESCAPE &&
                   ApproxDist2D(&actor->posX,
                                 PATH_END(st->path)) < 0xBB8 &&
                   actor->mode != GNASTY_DEFEATED) {
          st->phase = GNASTY_PHASE_BEATEN;
          st->speed = 0xD2;
          st->knockHeading = ArcTan2(actor->posX - SPY.posX,
                                  actor->posY - SPY.posY, ATAN_ANGLE8);
          if (actor->dropKind != DROP_GEM_ROLL) {
            g_anLevelKillBitmapTable[0xF0] &= ~1;
          }
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          ENTER_POSE(actor, GNASTY_DEFEATED);
        }
      }

      if (st->phase < GNASTY_PHASE_MAIN) {
        CopyVector(sightPoint, &actor->posX);
        sightPoint[2] += 0x3E8;
        if (g_nSpyroState == SPYRO_ST_CHARGE || g_nSpyroState == SPYRO_ST_CHARGE_AIR) {
          st->camTimer = 0x3C;
        }
        if (TICK_DOWN(st->camTimer) != 0 && dist < 0x4000 &&
            (CheckSpyroLineOfSightTo(sightPoint) != 0 || dist < 0x3000)) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FOLLOW_CAMERA);
          g_pScriptedCameraActor = actor;
        }
      }

      if (st->key2 != -1 && g_pActorListBase[st->key2].subMode != 0 &&
          AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                        ArcTan2(actor->posX - SPY.posX,
                                      actor->posY - SPY.posY, ATAN_ANGLE8)) < 0x28 &&
          (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1B58 ||
           st->throws >= 4)) {
        st->key2 = -1;
        actor->mode = GNASTY_TO_ESCAPE;
        continue;
      }

      if (st->seen == 0 && g_nSpyroAirborneFrames == 0 &&
          (IsPointInRotatedBox(&SPY.posX, &st->box70) != 0 ||
           IsPointInRotatedBox(&SPY.posX, &st->boxA0) != 0)) {
        st->seen = 1;
      }

      if ((unsigned)(actor->mode - 6) < 2) {
        int ca = (ArcTan2(PATH_X(st->path2,
                                       st->path2->count - 1) -
                                    PATH_X(st->path3, 0),
                                PATH_Y(st->path2,
                                       st->path2->count - 1) -
                                    PATH_Y(st->path3, 0),
                                ATAN_ANGLE8) +
                  0x80) &
                 0xFF;

        if (TICK_DOWN(st->camTimer) == 0) {
          int *anc;

          g_dwSpyroRequestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA | SPYRO_RQ_FREEZE_INPUT);
          /* st->path2 by byte offset: the in-struct form reorders the
             loads (9/8304 insns differ). */
          anc = PATH_END(FIELD_RAW(st, GnastyState, PathData *, path2));
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          *(int *)&g_anCameraActiveTargetParams = -ca << 4;
          g_nCameraActiveTargetDistance = 0x1770;
          g_nCameraActiveTargetPitch = 0x19;
          g_nCameraActiveTargetRoll = 0;
          g_nCameraActiveTargetPitchBias = -0x32;
          g_nCameraActiveTargetYawBias = 0;
          g_nCinematicActive = 1;
          g_pSpyroRequestCameraAnchor = anc;
        } else {
          g_nCinematicActive = 0;
        }
      }

      if ((unsigned)(actor->mode - 8) < 2) {
        int ca = (SPY.bodyRotZ + 0x40) & 0xFF;
        int *anc;

        AddVector(PATH_PT(st->path, 0), &actor->posX,
                      &SPY.posX);
        RShiftVector3(PATH_PT(st->path, 0), 1);
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA | SPYRO_RQ_FREEZE_INPUT);
        anc = (int *)((char *)*(PathData **)st + 8);
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
      case GNASTY_WAIT:
        SnapActorToGround(actor);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && st->seen != 0 &&
            AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], ATAN_ANGLE8)) <
                0x1E &&
            ApproxDist2D(&SPY.posX, PATH_PT(st->home, 0)) < 0x3000 &&
            (unsigned)(dist - 0x1801) < 0x4FFF) {
          st->throws = 0;
          actor->mode = GNASTY_THROW;
          continue;
        }
        break;

      case GNASTY_THROW:
        if (st->phase == GNASTY_PHASE_MAIN) {
          if (SPY_DZ(actor) < 0 && dist < 0x1388) {
            actor->mode = GNASTY_RUN;
            continue;
          }
        } else if (st->phase == GNASTY_PHASE_ESCAPE) {
          if (st->throws < 4) {
            if (dist >= 0xE10) {
              goto anim2;
            }
            st->timer = 0;
          } else {
            st->timer = 0x96;
          }
          actor->mode = GNASTY_STAND;
          continue;
        } else if (st->throws >= 4 ||
                   AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                                 ArcTan2(actor->posX - g_anCameraPos[0],
                                               actor->posY - g_anCameraPos[1],
                                               ATAN_ANGLE8)) > 0x28 ||
                   (unsigned)(dist - 0x1770) > 0x55F0) {
          st->timer = 0x96;
          ENTER_POSE(actor, GNASTY_WAIT);
        }
      anim2:
        if (TrackEntityYawTowardSpyro(actor, 5, 0x14, YAW_STEP_IN_BAND) == 0) {
          continue;
        }
        if (TICK_DOWN(st->timer) != 0) {
          ANIM_GO(actor, GNASTY_THROW);
        } else if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, GNASTY_WAIT);
        }
        if (st->timer == 0 && actor->nextFrame >= 9) {
          Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_TRAIL_BOMB, actor);
          TrailBombState *q = o->state;
          int n;
          int spd = 0x190;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, &o->posX);
          o->rotZ =
              ArcTan2(SPY.posX - o->posX, SPY.posY - o->posY, ATAN_ANGLE8);
          CopyVector(spyroFloor, &SPY.posX);
          spyroFloor[2] = FindGroundHeightBelow(spyroFloor, 0x1000);
          SubtractVector(o150, spyroFloor, &o->posX);
          n = VectorLength(o150, VEC_LEN_XY) / spd;
          q->speed = spd;
          if (n <= 0) {
            n = 1;
          }
          q->climb = (spyroFloor[2] - o->posZ) / n;
          if (q->climb < -0x1F4) {
            q->climb = -0x1F4;
          }
          q->trail = st->trails[st->throws % 3];
          q->trailLen = 0;
          q->lifeTimer = 0xA0;
          q->landDelay = n - 1;
          st->throws++;
          PlayActorMeshVoice(actor, GNASTY_VOICE_THROW, &st->voice);
          if (st->phase == GNASTY_PHASE_MAIN && st->throws >= 2) {
            actor->mode = GNASTY_RUN;
            continue;
          }
          st->timer = 0x50;
        }
        break;

      case GNASTY_SWIPE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          actor->mode = GNASTY_RUN;
          continue;
        }
        break;

      case GNASTY_LOOKOUT:
        GNASTY_FALL(actor, st);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if ((dist < 0x2000 && SPY_DZ(actor) >= -0x6A3) || dist < 0x16A8) {
          st->unread54 = 0;
          actor->mode = GNASTY_RUN;
          continue;
        }
        if (ApproxDist2D(&SPY.posX, PATH_PT(st->path, 12)) <
            0x2000) {
          CopyVector(&actor->posX,
                        PATH_END(st->path));
          SnapActorToGround(actor);
          st->path->cur = 0;
          actor->mode = GNASTY_PATH_END;
          continue;
        }
        break;

      case GNASTY_RUN: {
        int spd;
        int far;
        unsigned int i;
        int r;

        ANIM_GO(actor, GNASTY_ANIM_RUN);
        if (st->queuedJumpSpeed != 0) {
          st->vSpeed = st->queuedJumpSpeed;
          st->queuedJumpSpeed = 0;
        }
        i = st->path->cur;
        spd = 0x13B;
        if (FLY_FLAGS(st->path, i) & PATH_NODE_SPEED) {
          spd = FLY_FLAGS(st->path, i) & PATH_NODE_SPEED;
        }
        if (i >= 0xE) {
          far = 0x1964;
          if (st->speed >= 0x10F) {
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
        st->speed = spd;
        r = PivotActorAlongPath(actor, st->path, 0x200, spd, 0, 7, 0x28, PIVOT_TURN_ANY, 0);
        GNASTY_FALL(actor, st);
        if (r == PATH_ARRIVED(1) && ApproxDist2D(&actor->posX, &SPY.posX) > 0x2000) {
          ENTER_POSE(actor, GNASTY_LOOKOUT);
        }
        if (r == PATH_ARRIVED(0)) {
          FindNearestPathPointToSpyro(st->path, pathHit);
          if (dist > 0x4800 && pathHit[0] > 0x10 &&
              pathHit[0] < st->path->count - 1) {
            st->throws = 0;
            actor->mode = GNASTY_THROW;
            continue;
          }
          st->stateTimer = 0xC8;
          actor->mode = GNASTY_PATH_END;
          continue;
        }
        break;
      }

      case GNASTY_ESCAPE: {
        int spd;
        int r;
        int h;
        int z;

        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, GNASTY_ANIM_RUN);
        }
        if (TICK_DOWN(st->stateTimer) == 0) {
          TurnActorYawTowardAngle(
              actor,
              ArcTan2(
                  PATH_X(st->path, st->path->cur) -
                      actor->posX,
                  PATH_Y(st->path, st->path->cur) -
                      actor->posY,
                  ATAN_ANGLE8),
              6, 0, YAW_HOLD_IN_BAND);
          h = ProbeActorGroundHeightRaised(actor, 0xBB8);
          st->vSpeed -= 0x19;
          if (st->vSpeed < -0x190) {
            st->vSpeed = -0x190;
          }
          z = actor->posZ + st->vSpeed;
          if (z < h) {
            actor->posZ = h;
            ANIM_GO(actor, GNASTY_ANIM_RUN);
          } else {
            actor->posZ = z;
          }
          break;
        }
        if (st->queuedJumpSpeed != 0) {
          st->vSpeed = st->queuedJumpSpeed;
          st->queuedJumpSpeed = 0;
        }
        spd = 0x104;
        if (FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_SPEED) {
          spd = FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_SPEED;
        }
        r = PivotActorAlongPath(actor, st->path, 0x12C, spd, 0, 0xA, 0x28, PIVOT_TURN_ANY, 0);
        GNASTY_FALL(actor, st);
        if (r == PATH_ARRIVED(0)) {
          actor->mode = GNASTY_STAND;
          continue;
        }
        if (st->passedPt6 == 0 &&
            ApproxDist2D(&actor->posX, PATH_PT(st->path, 6)) <
                0x258) {
          st->passedPt6 = 1;
          actor->mode = GNASTY_AT_DOOR;
          continue;
        }
        if (r & PATH_ARRIVED_FLAG) {
          switch (FLY_FLAGS(st->path, st->path->cur) & PATH_NODE_JUMP) {
          case PATH_JUMP_LOW:
            st->queuedJumpSpeed = 0xDC;
            break;
          case PATH_JUMP_MID:
            st->queuedJumpSpeed = 0x10E;
            break;
          case PATH_JUMP_HIGH:
            st->queuedJumpSpeed = 0x140;
            break;
          }
        }
        break;
      }

      case GNASTY_FLAMED:
        if (g_nActorAnimDone != 0) {
          st->stateTimer = 0;
          ANIM_GO(actor, GNASTY_ANIM_RUN);
          actor->mode = GNASTY_ESCAPE;
          continue;
        }
        break;

      case GNASTY_DEFEATED:
        g_nSpyroSwingEulerTargetY[19] = /* g_dwSpyroRequestMask */
            g_dwSpyroRequestMask | (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        g_nCinematicActive = 1;
        DriveActorTrackedMove(actor, &st->speed, st->knockHeading, 0, 0xC, 0);
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
          Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_FOLLOW_FACER, actor);

          *(Actor **)o->state = actor;
          CopyVector(&o->posX, &SPY.posX);
          o->rotX = g_abSpyroPersistentEuler;
          o->rotY = g_bSpyroPersistentEulerY;
          o->rotZ = g_bSpyroPersistentEulerZ;
          g_nSpyroDrawSuppressed = 1;
        }
        if (g_nActorAnimDone != 0) {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->drawn = 0;
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x40);
          st->stateTimer = 0x3C;
          actor->mode = GNASTY_FADE_OUT;
          continue;
        }
        break;

      case GNASTY_FADE_OUT:
        g_dwSpyroRequestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        TICK_DOWN(st->stateTimer);
        if (st->stateTimer < 0x10) {
          g_nScreenFadeLevel = sixteen - st->stateTimer;
          if (g_nScreenFadeLevel >= 0x10) {
            g_nScreenFadeLevel = 0xF;
          }
        }
        if (st->stateTimer == 0) {
          g_abLevelExited[g_nLevelIndex] = 1;
          g_nCreditsPageIndex = 0;
          g_nCutsceneId[0] = CUTSCENE_GNASTY_ENDING;
          g_nGamestate = GS_CUTSCENE;
          g_nSkipGamestateDraw = 1;
          g_nCinematicActive = 0;
          g_nSpyroDrawSuppressed = 0;
          g_pPathTableHead->frame = 0;
        }
        break;

      case GNASTY_TO_ESCAPE:
        ANIM_GO(actor, GNASTY_ANIM_RUN);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(
                              PATH_X(st->path, 1) -
                                  actor->posX,
                              PATH_Y(st->path, 1) -
                                  actor->posY,
                              ATAN_ANGLE8),
                          7, 0x14, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongHeading(actor, 0xF0, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (ApproxDist2D(&actor->posX, PATH_PT(st->path, 1)) <
            0x320) {
          st->phase = GNASTY_PHASE_MAIN;
          st->path->cur = 2;
          ENTER_POSE(actor, GNASTY_LOOKOUT);
        }
        break;

      case GNASTY_AT_DOOR:
        GNASTY_FALL(actor, st);
        if (TrackEntityYawTowardSpyro(actor, 5, 0, YAW_HOLD_IN_BAND) != 0) {
          ANIM_GO(actor, GNASTY_LOOKOUT);
        }
        if (IsPointInRotatedBox(&SPY.posX, &st->box88) != 0) {
          ModifyWorldChunkAnimFlags(GNASTY_CHUNK_DOOR, CHUNK_ANIM_RESUME, 0);
          ANIM_GO(actor, GNASTY_ANIM_RUN);
          actor->mode = GNASTY_ESCAPE;
          continue;
        }
        break;

      case GNASTY_STAND: {
        int h = ProbeActorGroundHeightRaised(actor, 0xBB8);
        int z;

        ANIM_GO(actor, GNASTY_WAIT);
        st->vSpeed -= 0x19;
        if (st->vSpeed < -0x190) {
          st->vSpeed = -0x190;
        }
        z = actor->posZ + st->vSpeed;
        if (z < h) {
          actor->posZ = h;
        } else {
          actor->posZ = z;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (st->timer == 0 && (unsigned)(dist - 0xE10) < 0x16A8 &&
            g_nSpyroAirborneFrames == 0) {
          st->throws = 0;
          actor->mode = GNASTY_THROW;
          continue;
        }
        if (TICK_DOWN(st->timer) != 0 && dist < 0xE10) {
          ANIM_GO(actor, GNASTY_SWIPE);
          actor->mode = GNASTY_STAND_SWIPE;
          continue;
        }
        break;
      }

      case GNASTY_STAND_SWIPE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          ANIM_GO(actor, GNASTY_WAIT);
          actor->mode = GNASTY_STAND;
          continue;
        }
        break;

      case GNASTY_PAUSE:
        if (TICK_DOWN(st->stateTimer) != 0) {
          actor->mode = GNASTY_RUN;
          continue;
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND) != 0) {
          ANIM_GO(actor, GNASTY_WAIT);
        }
        break;

      case GNASTY_PATH_END:
        if (TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND) != 0) {
          ANIM_GO(actor, GNASTY_WAIT);
        }
        if (TICK_DOWN(st->stateTimer) != 0 && SPY_DZ(actor) < 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x251C) {
          actor->mode = GNASTY_RUN;
          continue;
        }
        if (dist < 0xE10) {
          ENTER_POSE_B(actor, GNASTY_SWIPE);
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 169: a rolling wave that lays a trail and spawns shockwaves (143)
     * where it lands. */
    case ACTOR_TYPE_TRAIL_BOMB: {
      TrailBombState *st = actor->state;
      int s = actor->mode;

      if (s < 3) {
        int n;
        int i;

        if (s < 0) {
          goto out169;
        }

        CopyVector(o150, &actor->posX);
        if (actor->mode == TRAILBOMB_FLY) {
          st->spawnTimer += g_nFrameStep;
          if (st->spawnTimer >= 0xA) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_TRAIL_BOMB_PUFF, actor);

            CopyVector(&o->posX, &actor->posX);
            o->subMode = 0;
            o->rotY = ArcTan2(st->speed, st->climb, ATAN_ANGLE8);
            o->rotZ = actor->rotZ;
            o->cullRadius = 0x50;
            st->spawnTimer = 0;
          }
          actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
          actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
          actor->posZ += st->climb;
          if (TICK_DOWN(st->lifeTimer) != 0) {
            actor->mode = TRAILBOMB_HIT;
          }
          if (actor->posX < 0 || actor->posY < 0 || actor->posZ < 0) {
            actor->cullRadius = ACTOR_CULL_HIDDEN;
            actor->drawn = 0;
            actor->mode = TRAILBOMB_HIT;
            continue;
          }
          if (--st->landDelay < 0) {
            st->landDelay = 0;
          }
          if (st->landDelay == 0 && CastRayWorldAndActors(o150, &actor->posX) != 0) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_SHOCKWAVE, actor);
            ShockwaveState *p = o->state;

            p->radius = 0x190;
            p->timer = 0x12;
            o->scale = 0xBB80 / p->radius;
            if (o->scale < 5) {
              o->scale = 5;
            }
            if (o->scale >= 0x80) {
              o->scale = 0x7F;
            }
            actor->mode = TRAILBOMB_HIT;
            PlayActorMeshVoice(actor, TRAILBOMB_VOICE_HIT, 0);
          }
          if (actor->mode == TRAILBOMB_HIT) {
            actor->cullRadius = ACTOR_CULL_HIDDEN;
            actor->drawn = 0;
          }
        }
        n = 0;
        if (actor->mode == TRAILBOMB_FLY && g_nMotionTrailRibbonCount < 3 && st->trailLen < 0x78) {
          int k = st->trailLen;

          CopyVector(st->trail[k].pos, &actor->posX);
          st->trail[k].age = 0;
          st->trailLen++;
        }
        for (i = 0; i < st->trailLen; i++) {
          st->trail[i].age += g_nFrameStep;
          if (st->trail[i].age < 0x51) {
            n++;
          }
        }
        if (n == 0 && actor->mode != TRAILBOMB_FLY) {
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nMotionTrailRibbonCount < 3) {
          int k = g_nMotionTrailRibbonCount;

          g_anMotionTrailSliceCounts[k] = st->trailLen;
          g_nMotionTrailRibbonCount = k + 1;
          g_apMotionTrailSlices[k] = st->trail;
        }
        if (actor->mode == TRAILBOMB_HIT) {
          actor->mode = TRAILBOMB_FADE;
        }
      }
    out169:
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Key (KeyState): rocks in place and sparkles; touching it picks it up. */
    case ACTOR_TYPE_KEY: {
      KeyState *st = actor->state;

      if (actor->subMode == KEY_WAITING) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut_s[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &SPY.posX) < KEY_TOUCH_RANGE) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < KEY_TOUCH_DZ : (SPY.posZ - height) < KEY_TOUCH_DZ) {
            int i;

            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
            RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (0x8080 + (i << 24)));
            }

            g_nLevelKeyState = KEYSTATE_CARRIED;
            actor->scale = 0x40;
            actor->cullRadius = ACTOR_CULL_HIDDEN;
            actor->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
            actor->drawn = 0;
            actor->groundCode = 0;
            actor->gateChunk = GATE_CHUNK_ALWAYS;
            actor->subMode = KEY_TAKEN;
          }
        }

        if (st->attach != ROT_BILLBOARD_NONE) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset4,
                        g_aRotBillboards[st->attach].pos);
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = ROT_BILLBOARD_NONE;
          }
        } else if (st->timer >= ROT_BILLBOARD_GLINT_TICK) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (handle >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < ROT_BILLBOARD_ATTACH_RANGE) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }

        st->timer += g_nFrameStep;
      }

      break;
    }

    /* Type 181: loot vortex (LootVortexState). */
    case ACTOR_TYPE_LOOT_VORTEX: {
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
            (g_pActorListBase[st->key].mode >= ACTOR_MODE_DORMANT &&
             g_pActorListBase[st->key].type == ACTOR_TYPE_LOOT_THIEF)) {
          int i;
          int c;

          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          i = 0;
          c = 0x8080;
        spark:
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, c);
          i++;
          c += 0x1000000;
          if (i < 6) {
            goto spark;
          }
          g_nLevelKeyState = KEYSTATE_CARRIED;
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->gateChunk = GATE_CHUNK_ALWAYS;
          actor->subMode = 1;
          st->mode = LVORTEX_OPEN;
        }
        if (st->spark != 0xFF && actor->cullRadius != ACTOR_CULL_HIDDEN) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->spark].pos);
          AddVector(g_aRotBillboards[st->spark].pos, g_aRotBillboards[st->spark].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->spark].life < 5) {
            st->spark = 0xFF;
          }
        } else if (st->timer >= 0xF8 && actor->cullRadius != ACTOR_CULL_HIDDEN) {
          int s = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (s >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < ROT_BILLBOARD_ATTACH_RANGE) {
            st->spark = s;
          }
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
      }

      switch (st->mode) {
      case LVORTEX_LOCKED: {
        if (g_pActorListBase[st->key].type != ACTOR_TYPE_LOOT_THIEF) {
          break;
        }
        if (g_pActorListBase[st->key].mode < 0x7F) {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->gateChunk = GATE_CHUNK_ALWAYS;
          break;
        }
        actor->subMode = 1;
        st->mode = LVORTEX_OPEN;
        g_nLevelKeyState = KEYSTATE_CARRIED;
        break;
      }

      case LVORTEX_OPEN:
        if (((unsigned)SPY.state <= SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG || SPY.state == SPYRO_ST_RUN) &&
            IsPointInRotatedBox(&SPY.posX, &st->box) &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(g_pActorListBase[st->target].posX - SPY.posX,
                                        g_pActorListBase[st->target].posY - SPY.posY,
                                        ATAN_ANGLE8)) < 0x30) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          ResetSpyroLinearMotion();
          ZeroVector(&spyc->headLook);
          actor->subMode = 2;
          actor->rotX = 0;
          actor->rotY = 0;
          actor->rotZ = g_bSpyroPersistentEulerZ + 0x80;
          actor->drawn = 1;
          actor->cullRadius = 0x18;
          st->mode = LVORTEX_ACTIVE;
          st->timer = 0;
          if (st->seen != 0) {
            if (g_nCutsceneId[st->seen] != 0) {
              int d;
              int r;

              st->timer = 0x80;
              d = AbsAngleDelta8(g_pActorListBase[st->target].rotZ,
                                (unsigned char)(SPY.bodyRotZ + 0x40));
              actor->rotY = 0xC0;
              r = g_bSpyroPersistentEulerZ + 0x40;
              actor->rotZ = r + d * (st->timer - 0x80) / 64;
            } else {
              g_nCutsceneId[st->seen] = 1;
            }
          }
          CopyVector(&actor->posX, &SPY.posX);
        }
        break;

      case LVORTEX_ACTIVE:
        if (g_dwPadPressed & PAD_CROSS) {
          st->timer = 0xD2;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        actor->cullRadius = 0x18;
        st->timer += g_nFrameStep;
        g_nCinematicActive = 1;
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
          g_nSpyroSwingEulerTargetY[0] =
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
          int *w = settleOffset;

          actor->rotZ = g_pActorListBase[st->target].rotZ;
          w[0] = 0x200 - (st->timer - 0xC0) * 8;
          w[2] = 0x140;
          w[1] = 0;
          RotateVectorByMatrix(actor->mtx, w, w);
          AddVector(&actor->posX, w, &g_pActorListBase[st->target].posX);
        }
        if (st->timer >= 0xD0 || !((unsigned)SPY.state <= SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
                                    SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->drawn = 0;
          g_nCinematicActive = 0;
          g_pfnLevelEmitParticles(0x20, EMIT_STREAK_BURST, &actor->posX, 0x18);
          MarkActorLiveKill(actor);
          st->mode = LVORTEX_FINISH;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        if ((short)(st->timer % 4) < 2) {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, 0x8080);
        } else {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, 0x5008080);
        }
        break;

      case LVORTEX_FINISH:
        PlaySoundEffect(g_pLevelSampleBankHeader[SFX_LOOT_VORTEX_DONE], 0, SFX_MODE_PANNED, 0);
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        g_nSpyroSwingEulerTargetY[1] = 0;
        g_nSpyroSwingEulerTargetY[0] = 0;
        g_nSpyroSwingEulerTargetY[-1] = 0;
        g_nLevelKeyState = KEYSTATE_NONE;
        switch (st->chunkAnim) {
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
      PlayActorMeshVoice(actor, WOODCHEST_VOICE_BREAK, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (ACTOR_POOL_LOW()) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 1, actor);
      }
      for (i = 0; i < 6; i++) {
        if (ACTOR_POOL_LOW()) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 2, actor);
      }

      g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x20);
      SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
      MarkActorLiveKill(actor);
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
        PlayActorMeshVoice(actor, WOBBLECHEST_VOICE_BREAK, 0);

        for (i = 0; i < 2; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }

        for (i = 0; i < 6; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
        }

        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->scorchBand = StepActorScorchTint(actor, st->scorchBand);

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

      if (!(actor->cullRadius & ACTOR_CULL_ALWAYS_BIT) && (actor->cullRadius != ACTOR_CULL_HIDDEN)) {

        if (st->life >= 1) {

          actor->rotZ += 1;

          st->vel[2] -= DEBRIS_GRAVITY;
          if (st->vel[2] < -DEBRIS_MAX_FALL) {
            st->vel[2] = -DEBRIS_MAX_FALL;
          }

          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          actor->posZ += st->vel[2];

          if (actor->posZ > 1023) {
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
              int *nrm = &g_anCollisionHitNormal;
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(nrm, FIXED_ONE);

              dot = (st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionHitNormalY +
                     st->vel[2] * g_nCollisionHitNormalZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(nrm, FIXED_ONE, -dot);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionHitNormalY);
                st->vel[2] += AS_I32(g_nCollisionHitNormalZ);
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

    /* Sparkling shard: 1 puff per frame. */
    case ACTOR_TYPE_SPARKLE_SHARD: {
      TumbleState *st = actor->state;

      if (st->life != 0 && actor->drawn != 0) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= DEBRIS_GRAVITY;
        if (st->vel[2] < -DEBRIS_MAX_FALL) {
          st->vel[2] = -DEBRIS_MAX_FALL;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];

        sparkleTwinkle[0] = (GetRandomU32() & 0xFE) - 0x7F;
        sparkleTwinkle[1] = (GetRandomU32() & 0xFE) - 0x7F;
        sparkleTwinkle[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(sparkleTwinkle, sparkleTwinkle, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, sparkleTwinkle, 1);
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
      TumbleState *st = actor->state;
      register int i asm("$16");

      if (st->life > 0) {
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }

        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
          int *nrm = &g_anCollisionHitNormal;
          int along;

          SetVector3Magnitude(nrm, FIXED_ONE);
          along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionHitNormalY) +
                  (st->vel[2] * g_nCollisionHitNormalZ);
          i = along >> 11;

          if (i < 0) {
            ScaleVector3ByRatio(nrm, FIXED_ONE, (along >> 13) - i);
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionHitNormalY);
            st->vel[2] += AS_I32(g_nCollisionHitNormalZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= DEBRIS_GRAVITY;
        if (st->vel[2] < -DEBRIS_MAX_FALL) {
          st->vel[2] = -DEBRIS_MAX_FALL;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        i = 0;
        {
          actor->rotY += st->spin[1];
          actor->rotZ += st->spin[2];

          do {
            i += 1;
            shardTwinkle[0] = (GetRandomU32() & 0xFE) - 0x7F;
            shardTwinkle[1] = (GetRandomU32() & 0xFE) - 0x7F;
            shardTwinkle[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(shardTwinkle, shardTwinkle, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, shardTwinkle, 1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
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
      case BOULDER_INIT:
        *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
        actor->subMode = BOULDER_INTACT;
        break;

      case BOULDER_INTACT:
        if (actor->flags & ACTOR_HIT_BLAST) {
          actor->subMode = BOULDER_CRACKING;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          actor->cullRadius = 0x7F;
          if (st->timer < BOULDER_BLAST_TICK) {
            st->timer = BOULDER_BLAST_TICK;
          }
        } else if (actor->flags & ACTOR_HIT_FLAME) {
          actor->subMode = BOULDER_CRACKING;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
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
          actor->subMode = BOULDER_BURST;
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;

      case BOULDER_CRACKING: {
        int t = st->timer + g_nFrameStep;
        int i;
        int n;

        st->timer = t;
        if (t < BOULDER_CRACK_FRAMES) {
        if ((actor->flags & ACTOR_HIT_BLAST) && t < BOULDER_BLAST_TICK) {
          st->timer = BOULDER_BLAST_TICK;
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
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, ps, (int)(ps + 3));
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
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, ps, (int)s448);
          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, CBOULDER_VOICE_CRACK, 0);
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
          actor->subMode = BOULDER_BURST;
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;
      }

      case BOULDER_BURST:
        if (st->timer == 0) {
          int i;
          Actor *c;
          DebrisFallState *cs;

          PlayActorMeshVoice(actor, CBOULDER_VOICE_BURST, 0);
          SpawnActorDeathDrop(actor, DROP_HIGH_HOP, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (ACTOR_POOL_LOW()) {
              break;
            }
            p = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 2, actor);
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
            if (ACTOR_POOL_LOW()) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 1, actor);
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
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, SPYRO_EV_KNOCKBACK_2 | SPYRO_EV_CHARGE_HIT) != 0) {
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
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_BLAST), actor, CONTACT_STOP_AT_SOLID);
        if (++st->timer >= 8) {
          MarkActorLiveKill(actor);
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
    case ACTOR_TYPE_SEED_POD: {
      SeedPodState *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case SEEDPOD_CLOSED:
        if (actor->subMode == SEEDPOD_SUB_UNTINTED) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
          actor->subMode = SEEDPOD_SUB_TINTED;
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
          st->prize->cullRadius = ACTOR_CULL_HIDDEN;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, EMIT_GEM_COLOR_BURST, &actor->posX, (st->prize->type - ACTOR_TYPE_GEM_1));
          PlayActorMeshVoice(actor, SEEDPOD_VOICE_OPEN, 0);
          actor->mode = SEEDPOD_OPEN;
          ANIM_RESET(actor, SEEDPOD_OPEN);
          continue;
        }
        if (st->timer >= ROT_BILLBOARD_GLINT_TICK) {
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
          ZeroVector(s448);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)s448);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < SEEDPOD_PRIZE_RANGE &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == SPYRO_ST_SUPERCHARGE || g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
            (g_nSpyroState == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, SEEDPOD_VOICE_BURST, 0);
          for (i = 0; i < 2; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & ACTOR_HIT_SUPER) {
          st->prize->subMode = COLLECT_BOUNCE;
          PlayActorMeshVoice(actor, SEEDPOD_VOICE_BURST, 0);
          for (i = 0; i < 2; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x20);
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
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, s480, (int)col2);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= ACTOR_KILL_GROUP_MASK;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, SEEDPOD_VOICE_CLOSE, 0);
          actor->mode = SEEDPOD_CLOSED;
          ANIM_RESET(actor, SEEDPOD_CLOSED);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Type 414: turns toward the actor it follows. */
    case ACTOR_TYPE_FOLLOW_FACER: {
      Actor **st = actor->state;

      TurnActorYawTowardAngle(actor,
                    ArcTan2((*st)->posX - actor->posX,
                                  (*st)->posY - actor->posY, ATAN_ANGLE8),
                    4, 0, YAW_HOLD_IN_BAND);
      continue;
    }

    /* Name letter (NameLetterState): one letter of a floating name
     * (SpawnNameLetterActors), riding its sign at a fixed radius, or just
     * spinning in place behind a non-spinner. */
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
      NameLetterState *st = actor->state;

      if (st->sign->mode == ORBIT_OWNER_FAR || st->sign->subMode != actor->mode) {
        DespawnActorRecord(actor);
        continue;
      }

      if (st->sign->type == ACTOR_TYPE_LEVEL_EXIT) {
        if (ApproxDist2D(&st->sign->posX, &spy->posX) < LEVELEXIT_ENTER_RANGE) {
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
          bearing = ArcTan2(spy->posX - st->sign->posX,
                                  SPY.posY - st->sign->posY, ATAN_ANGLE8);
          s448[0] = (g_anCosineLut[bearing] * 3) >> 4;
          s448[1] = (g_anSineLut_s[bearing] * 3) >> 4;
          s448[2] = 0;
          s480[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[1] = (unsigned short)g_anSineLut_s[(bearing + 0x40) & 0xFF] << 16 >> 21;
          s480[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, s480, st->count - 1);
          LShiftVector3(s480, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = s448[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = s448[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(s480, s480, st->index);
          SubtractVector(letterSlot, letterSlot, s480);
          swing = &g_anCosineLut[bearing];
          actor->posX = s448[0] * *swing;
          actor->posY = s448[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, s448);
          AddVector(&actor->posX, &actor->posX, &st->sign->posX);
          SubtractVector(&actor->posX, &actor->posX, letterSlot);
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
