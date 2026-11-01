/* Level23_UpdateActors (0x8007CFB4, 21,128 B) -- matching notes.
 *
 * Sibling of Level11_UpdateActors (the flight actor-update family head),
 * templated from the matched level_5 copy. See the level_11 file for the
 * load-bearing source forms shared by the whole family. Differences from
 * level_5:
 *   - new arm 0xA2 (a roller cycling over four paths); no 0x4E, 0x16A,
 *     0x197 or 0x198; 0x18C (not 0x1EA) joins the 0x183 arm.
 *   - scratch vectors as level_17 minus `svT`; no extra array in 0x4C.
 *   - the &g_anCollisionProbeVec group must be hoisted by loop.c (the original
 *     rematerialises it per call). This overlay's loop is long enough that
 *     4 x life 92 x threshold 14 falls short, so the 0x1DE arm carries two
 *     empty scopes between its uses: each block note adds two luids of
 *     lifetime (92 -> 96), and the group moves. No code is emitted for them.
 *   - 0x183's retire block stores scale after groundCode.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_update_actors.h"
#include "flight_actor_states.h"
#include "level_update_actors_macros.h"
#include "gamestate.h"
#include "match_asm.h"
#include "pad_buttons.h"
#include "spyro_state.h"
#include "level_sfx.h"
#include "level_id.h"
#include "emit_record.h"

/* Level23_UpdateActors -- per-frame actor update for level 11 (Night Flight).
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


/* Type 286: ambient sound source; mode 2 also coasts along a path. */


/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */


/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */


/* Glow node owned by Sparx; follows its anchor position. */


/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */


/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = 0x10;                                                           \
  (a)->frameRate = 0x10;                                                           \
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
    (a)->frameProgress = 0x10;                                                         \
    (a)->frameRate = 0x10;                                                         \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }                                                                            \
  continue

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks (splice leftovers) ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */


/* ==== Flight-level classes ==== */


/* ==== The function ==== */


/* Type 0xA2: a roller that cycles over four paths. */
typedef struct CyclerState {
  unsigned char *path;     /* 0x00 current path */
  unsigned char *paths[4]; /* 0x04 */
  int pathIdx;             /* 0x14 */
  int reward;              /* 0x18 */
  int tally;               /* 0x1C */
  int roll;                /* 0x20 */
  int rollVel;             /* 0x24 */
  int zVel;               /* 0x28 */
  int heading;             /* 0x2C */
  int moveSpeed;               /* 0x30 */
  int legVec[3];            /* 0x34 */
  int legStep;               /* 0x40 */
} CyclerState;


void Level23_UpdateActors(void) {
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
  int svU[3];
  int svV[3];
  int svW[3];
  int sh6;

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, 0);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apActiveActorList;
  sh6 = 6;
  base = (char *)&g_nSparxHealth;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    one = 1;
    spy = (SpyroObj *)(base - 0x164);

    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & ACTOR_ANIM_DONE;
    g_nActorAnimNewFrame = actor->animFlags & ACTOR_ANIM_NEW_FRAME;
    g_nActorFrameDelta = g_nFrameStep;

    switch (actor->type) {
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

              SPY.sparxHealth = one;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            ChaserState *cs = (ChaserState *)g_pSparxActor->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, ATAN_ANGLE8)) < 0) {
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

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, YAW_STEP_IN_BAND);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, MOVE_LIFT);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, ATAN_ANGLE8);

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
        SteerActorAroundPoint(actor, &spy->posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
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
    case ACTOR_TYPE_AIMED_SHOT: {
      DriftState *st = actor->state;

      if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
        AddVector(svX, &actor->posX, st->vel);
        if (CastRayWorldAndActors(&actor->posX, svX) == 0 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0, SPYRO_EV_HIT) == 0) {
          CopyVector(&actor->posX, svX);
          break;
        }
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Dragon egg (EggFlightState). */
    case ACTOR_TYPE_DRAGON_EGG: {
      EggFlightState *st;

      st = (EggFlightState *)actor->state;

      if (actor->mode != EGG_CARRIED && actor->mode < EGG_COLLECTED) {
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (SPY.state != SPYRO_ST_IDLE && SPY.state != SPYRO_ST_CHARGE_BONK) {
          actor->mode = EGG_COLLECTED;
        }
      }
      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, (void *)0x602080);
      }

      switch (actor->mode) {
      case EGG_RELEASED:
        g_nCinematicActive = 1;
        VectorToSVectorShr2(st->from, &actor->posX);
        st->heading =
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
            0x40;
        st->speed = 0;
        st->phase = 0;
        g_nHudEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode = actor->mode + 1;
        break;

      case EGG_SPIRAL_UP:
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
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_TO_SPYRO:
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
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_SPIRAL_DOWN:
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
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = EGG_COLLECTED;
        }
        break;

      case EGG_COLLECTED:
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, svE, 0);
        g_nCinematicActive = 0;
        ZeroVector(&SPY.headLook);
        CommitActorKillGems(0, st->killActor);
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

    /* Bouncing chest fragments (ChestShardState). */
    case ACTOR_TYPE_FLIGHT_CHEST_SHARD:
    case ACTOR_TYPE_FLIGHT_CHEST_SHARD + 1: {
      ChestShardState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
          
          int dot;

          SetVector3Magnitude(&g_anCollisionProbeVec, 0x1000);
          { int pad0; { int pad1; } } /* see header: loop.c lifetime dial */
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
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svD);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Falling chest fragments (TumbleState). */
    case ACTOR_TYPE_FLIGHT_CHEST_SHARD + 2: {
      TumbleState *st = actor->state;

      if (st->life != 0 && actor->drawn && actor->posZ > st->floorZ) {
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
      } else {
        DespawnActorRecord(actor);
        continue;
      }
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

        if (st->sparkle != 255) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset5, g_aRotBillboards[st->sparkle].pos);
          } else {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset3, g_aRotBillboards[st->sparkle].pos);
          }

          AddVector(g_aRotBillboards[st->sparkle].pos, g_aRotBillboards[st->sparkle].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->sparkle].life < 5) {
            st->sparkle = 255;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);
            if (handle >= 0 && dist < 0x4000) {
              st->sparkle = handle;
            }

            st->timer = (GetRandomU32() & 0x1F) + 33;
          }
        }
      }

      if (!actor->drawn && dist > 0x2000 && actor->subMode == COLLECT_REST && st->restMode == COLLREST_STILL) {
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
        if (st->restMode != COLLREST_STILL) {

          if (st->restMode == COLLREST_SNAP) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= ACTOR_TYPE_GEM_1) {
              int *nrm = &g_anCollisionProbeVec;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                  g_nCollisionProbeVecY);
              st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
            }

          } else if (st->restMode == COLLREST_ON_TRI) {

            UnpackWorldCollisionTri(st->groundTri, acc[0]);

            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);

            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->restMode == COLLREST_FIND_TRI) {
            int colIndex;

            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              colIndex = g_nLastCollisionHitChunkId;
              st->restMode = COLLREST_ON_TRI;
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
          int r;
          register int *pos asm("$4");

          svG[2] += 240;

          r = CollideSphereWithWorldAndActors(svG, 240, 240);
          pos = &actor->posX;
          if (r) {
            if (GetGroundTriggerRegionType(pos) == 0) {
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
            CopyVector(pos, svG);
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

          SubtractVector(svI, &spy->posX, st->vel);
          RShiftVector3(svI, 5);

          if (VectorLength(svI, VEC_LEN_XYZ) > 480) {
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

    /* Sparx (SparxState). */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = (SparxState *)actor->state;
      char pitchAngle;

      if (*(int *)base >= 2) {

        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(actor->mtx, svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);

        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);
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

          CopyVector(svA, &target->posX);
          if (isPickupAnimPhase == 0) {
            svA[0] -= g_anCosineLut[st->target->rotZ] >> 4;
            svA[1] -= g_anSineLut[st->target->rotZ] >> 4;
          } else {
            svA[0] -= g_anCosineLut[st->target->rotZ] >> 6;
            svA[1] -= g_anSineLut[st->target->rotZ] >> 6;
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
          dist = VectorLength(svA, VEC_LEN_XYZ);

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
          TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
          pitchAngle = ArcTan2(VectorLength(svA, VEC_LEN_XY), svA[2], ATAN_ANGLE8);
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
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, g_dwBurstColorExtraLife);
            else if (st->target->type == ACTOR_TYPE_SPARX_FOOD)
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, g_dwBurstColorSparxFood);
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
            TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            pitchAngle = ArcTan2(VectorLength(svJ, VEC_LEN_XY), svJ[2], ATAN_ANGLE8);
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
          st->drift[0] = svF[0];
          st->drift[1] = svF[1];
          st->drift[2] = svF[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == 3) {
            svF[0] = st->drift[0] + (SPY.motionSpeed >> 2);
            svF[1] = st->drift[1];
            svF[2] = st->drift[2];
            if (svF[2] < 0x64)
              svF[2] = 0x64;
          } else {
            if (CAM.state == 0x80000009)
              svF[0] = st->drift[0] - 512;
            else {
              svF[0] = st->drift[0];
            }
            svF[1] = st->drift[1];
            svF[2] = st->drift[2];
          }
          RotateVectorByMatrix(&SPY.bodyMtx, svF, svF);
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
          TurnActorYawTowardAngle(actor, SPY.bodyRotZ, 4, 0, YAW_HOLD_IN_BAND);
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

    case ACTOR_TYPE_CYCLING_FLIGHT_TARGET: {
      CyclerState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode == FTARGET_ACTIVE) {
        Actor *reward;
        int i;

        reward = g_pfnLevelSpawnActor(TIME_BONUS_TYPE(st->reward), actor);
        if (reward != 0) {
          reward->mode = TIMEBONUS_COLLECT;
        }
        TallyFlightTarget(st->tally);
        st->zVel = 0xC8;
        st->moveSpeed = 0x15E;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, 0x28, 0x40);
        for (i = 0; i < 3; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_TUMBLE_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_TUMBLE_SHARD + 1, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_TUMBLE_SHARD + 2, actor);
        }
        PlayActorMeshVoice(actor, 0, 0);
        actor->mode = FTARGET_STRUCK;
        ANIM_RESET(actor, 1);
        break;
      }

      switch (actor->mode) {
      case FTARGET_ACTIVE: {
        int d;

        if (st->legStep == 0xFF) {
          st->path[1]++;
          if (st->path[1] >= st->path[0]) {
            st->path[1] = 0;
          }
          st->legStep = 0;
          ZeroVector(st->legVec);
        }
        d = actor->rotZ;
        if (g_nSpyroSubstate < SPYRO_SUB_SCRIPTED &&
            TweenActorAlongPath(actor, st->path, st->legVec, &st->legStep, 0xA, 4) == 2) {
          st->pathIdx = (st->pathIdx + 1) & 3;
          switch (st->pathIdx) {
          case 0:
            st->path = st->paths[0];
            break;
          case 1:
            st->path = st->paths[1];
            break;
          case 2:
            st->path = st->paths[2];
            break;
          case 3:
            st->path = st->paths[3];
            break;
          }
          st->path[1] = 0;
        }
        d = (actor->rotZ - d) & 0xFF;
        if (d > 0x80) {
          d -= 0x100;
        }
        st->rollVel += ((-d * 0xC0) - (st->rollVel << 3) - st->roll) >> 6;
        st->roll = (st->roll + st->rollVel) & 0xFFF;
        if (st->roll > 0x800) {
          st->roll -= 0x1000;
        }
        if (st->roll < -0x140) {
          st->roll = -0x140;
        }
        if (st->roll > 0x140) {
          st->roll = 0x140;
        }
        actor->rotX = st->roll >> 4;
        break;
      }
      case FTARGET_STRUCK:
        if (g_nActorAnimDone != 0) {
          actor->mode = FTARGET_SPENT;
          if (actor->nextAnim != 2) {
            actor->frameProgress = 0x10;
            actor->frameRate = 0x10;
            actor->nextAnim = 2;
            actor->nextFrame = 0;
            OnActorAnimQueued(actor);
          }
          continue;
        }
        /* fall through */
      case FTARGET_SPENT:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zVel, 0xC,
                          0xC) == 3) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }
    /* Key (KeyState). */
    case ACTOR_TYPE_KEY: {
      KeyState *st;
      int dz;
      int ang;
      int i;

      st = (KeyState *)actor->state;
      if (actor->subMode != KEY_WAITING) {
        continue;
      }

      actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
      actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
      st->angle = st->angle + g_nFrameStep * 2;

      if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < 0x200
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)actor, (void *)(0x8080 + 0x1000000 * i));
          }

          g_nLevelKeyState = KEYSTATE_CARRIED;
          actor->cullRadius = 0;
          actor->updateDist = 0;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->visZone = 0xFF;
          actor->subMode = KEY_TAKEN;
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
          g_pfnLevelEmitParticles(10, EMIT_STREAK_BURST_WHITE, &actor->posX, 0);
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Flight-level chest: flaming or charging it frees its reward and
     * scatters shards; otherwise it hums its ambient sound. */
    case ACTOR_TYPE_FLIGHT_CHEST: {
      int *st = actor->state;
      int i;

      if (actor->flags & ACTOR_HITS_SPYRO) {
        Actor *reward;

        PlayActorMeshVoice(actor, 0, 0);
        TallyFlightTarget(st[1]);
        reward = g_pfnLevelSpawnActor(TIME_BONUS_TYPE(st[0]), actor);
        if (reward != 0) {
          reward->mode = TIMEBONUS_COLLECT;
        }

        for (i = 0; i < 2; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_FLIGHT_CHEST_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_FLIGHT_CHEST_SHARD + 1, actor);
        }
        for (i = 0; i < 6; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_FLIGHT_CHEST_SHARD + 2, actor);
        }

        g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
        DespawnActorRecord(actor);
        break;
      }

      {
        Model **models = g_apActorMeshTable;

        if (GetSoundVoiceStatusByOwner(actor, g_pLevelSoundIds[5]) == 0 && g_nGamestate == GS_GAMEPLAY) {
          PlaySoundEffect(models[actor->type]->m_Sounds[1], actor, SFX_MODE_3D,
                        &actor->voiceRow);
        }
      }
      break;
    }
    case ACTOR_TYPE_BANKING_FLIGHT_TARGET: {
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

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode == BANKER_FLY) {
        Actor *reward;
        int i;

        reward = g_pfnLevelSpawnActor(TIME_BONUS_TYPE(st->reward), actor);
        if (reward != 0) {
          reward->mode = TIMEBONUS_COLLECT;
        }
        TallyFlightTarget(st->tally);
        st->zVel = 0;
        st->moveSpeed = 0x15E;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, 0x28, 0x40);
        for (i = 0; i < 3; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS + 1, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS + 2, actor);
        }
        PlayActorMeshVoice(actor, 0, 0);
        actor->mode = BANKER_STRUCK;
        ANIM_RESET(actor, 1);
        break;
      }

      switch (actor->mode) {
      case BANKER_FLY: {
        int d = actor->rotZ;

        if (g_nActiveLevelId == LEVEL_SUNNY_FLIGHT || g_nActiveLevelId == LEVEL_WILD_FLIGHT) {
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
      case BANKER_STRUCK:
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zVel, 0xC,
                          0xC) == 3) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = BANKER_FLEE;
          ANIM_RESET(actor, 2);
        }
        break;
      case BANKER_FLEE:
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zVel, 0xC,
                          0xC) == 3) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }
    /* Time-bonus pickup: turns to face Spyro, then flies to the HUD corner
     * and adds TIME_BONUS_SECONDS(type) seconds when it arrives. */
    case ACTOR_TYPE_TIME_BONUS:
    case ACTOR_TYPE_TIME_BONUS + 1:
    case ACTOR_TYPE_TIME_BONUS + 2: {
      int *st = actor->state;

      switch (actor->mode) {
      case TIMEBONUS_WAIT:
        if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0) {
          DespawnActorRecord(actor);
          break;
        }
        EBB_BREAK();
        TurnActorYawTowardAngle(actor,
                      WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                  spy->posY - actor->posY, ATAN_ANGLE8),
                                    0x40),
                      4, 0, YAW_HOLD_IN_BAND);
        break;
      case TIMEBONUS_COLLECT:
        g_nFlightTimeBonus += TIME_BONUS_SECONDS(actor->type) * 60;
        ProjectWorldPointGTEUnscaled(&actor->posX, &actor->posX);
        actor->rotX = 0;
        actor->rotY = 0;
        actor->rotZ = 0xC0;
        actor->cullRadius = 0xFF;
        actor->otBias = 0x20;
        actor->mode = TIMEBONUS_TO_HUD;
        PlaySoundEffect(g_pLevelSampleBankHeader[SFX_FLIGHT_COLLECT], actor, SFX_MODE_PANNED, 0);
        break;
      case TIMEBONUS_TO_HUD: {
        int d;

        svU[0] = 0x1A4;
        svU[1] = 0x30;
        svU[2] = 0x157C;
        SubtractVector(svU, svU, &actor->posX);
        d = VectorLength(svU, VEC_LEN_XYZ);
        if (d < 0x20) {
          actor->mode = TIMEBONUS_ARRIVED;
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
      case TIMEBONUS_ARRIVED:
        if (TickCountdownByFrameDelta(st, 4) != 0) {
          g_nFlightTimeRemaining += TIME_BONUS_SECONDS(actor->type) * 60;
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_FLIGHT_TIME_ADDED], actor, SFX_MODE_PANNED, 0);
          DespawnActorRecord(actor);
        }
        break;
      }
      break;
    }
    /* Breakable flight target: bursts into 12 shards when Spyro flies
     * through, and otherwise pulses its glow with the frame counter. */
    case ACTOR_TYPE_FLIGHT_TARGET: {
      TargetState *st = actor->state;

      if (g_anFlightClearTimes[g_nActiveWorldIndex] == 0 && st->helper == 0) {
        st->helper = g_pfnLevelSpawnActor(TIME_BONUS_TYPE(st->helperType), actor);
      }

      if (ApproxDist2D(&actor->posX, &spy->posX) < 3000) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < 4000 : SPY.posZ - height < 4000) {
          SubtractVector(svV, &actor->posX, &spy->posX);
          svW[2] = (svV[2] - 0xC0) * 4;
          svV[2] = (svW[2] - svV[2]) >> 2;
          if (VectorLength(svV, VEC_LEN_XYZ) < 0x578 &&
              PlaneDistanceAbs(&spy->posX, st) < 0x200) {
            int i;

            TallyFlightTarget(st->tallyClass);
            if (st->helper != 0) {
              st->helper->mode = TIMEBONUS_COLLECT;
            }

            for (i = 0; i < 13; i++) {
              Actor *shard;
              BurstState *ss;

              if (ACTOR_POOL_LOW()) {
                break;
              }
              shard = g_pfnLevelSpawnActor(i + ACTOR_TYPE_FLIGHT_TARGET_SHARD, actor);
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
    case ACTOR_TYPE_FLIGHT_HUD: {
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
          st->digit[i] = g_pfnLevelSpawnActor(ACTOR_TYPE_GLYPH_PERIOD, actor);
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

        if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0) {
          for (i = 4; i < 8; i++) {
            st->digit[i] = g_pfnLevelSpawnActor(ACTOR_TYPE_GLYPH_PERIOD, actor);
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
            FLIGHT_TALLY_TINT_PRESET(off) = src[0x21];
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

      if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0) {
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
        if (g_bDebugNoDepletion == 0 && g_nSpyroSubstate < SPYRO_SUB_SCRIPTED &&
            g_anFlightClearTimes[g_nActiveWorldIndex] == 0) {
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
        ChangeSpyroState(SPYRO_ST_FLY);
        g_nSpyroSubstate = 3;
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_NO_CLIP);
      }

      if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0) {
        t = g_nFlightRunClock;
        i = t / 3600;
        if (i != 0) {
          st->digit[6]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
          st->digit[4]->cullRadius = 0xFF;
          st->digit[5]->cullRadius = 0xFF;
          st->digit[6]->cullRadius = 0xFF;
        } else {
          st->digit[4]->cullRadius = 0;
          st->digit[5]->cullRadius = 0;
          st->digit[6]->cullRadius = 0;
        }
        i = t * 10 / 6 % 10;
        st->digit[7]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
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
        st->digit[0]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
        st->digit[0]->cullRadius = 0xFF;
      } else {
        st->digit[0]->cullRadius = 0;
      }

      {
        int min = t / 60;

        i = min % 10;
        st->digit[1]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
        i = t / 6 - min * 10;
        st->digit[3]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;

        if (g_anFlightClearTimes[g_nActiveWorldIndex] == 0) {
          i = min;
          if (i < prevMin) {
            if (i < 6) {
              st->hurry = 1;
            } else {
              st->hurry = 0;
            }
            if (i < 10) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_FLIGHT_CLOCK], actor, SFX_MODE_PANNED, 0);
            }
          }
          if (st->hurry != 0) {
            i = t / 6;
            if (i < prevTenth) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_FLIGHT_HURRY], actor, SFX_MODE_PANNED, 0);
            }
          }
        }
      }

      if (st->spent[0] != 0) {
        if (st->count != 0 && TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->count--;
          st->spent[st->count]->mode = TALLY_ICON_SPEND;
          st->timer = 0x10;
          PlaySoundEffect(AS_PTRU8(g_pLevelSampleBankHeader)[SFX_FLIGHT_TALLY], actor, SFX_MODE_PANNED, 0);
        }
        for (i = 0; i < 8; i++) {
          Actor *a = st->spent[i];

          if (a != 0 && a->mode == TALLY_ICON_PARKED) {
            if (st->slot[g_nFlightTallySlot] == 0) {
              st->slot[g_nFlightTallySlot] = a;
              a->posX = g_nFlightTallySlot * 50 + 0x1E;
              st->slot[g_nFlightTallySlot]->posY = 0xC8;
              st->slot[g_nFlightTallySlot]->posZ = 0x1000;
              st->slot[g_nFlightTallySlot]->mode = TALLY_ICON_LIT;
            } else {
              DespawnActorRecord(a);
            }
            st->spent[i] = 0;
            PlaySoundEffect(AS_PTRU8(g_pLevelSampleBankHeader)[SFX_FLIGHT_TALLY], actor, SFX_MODE_PANNED, 0);
          }
        }
      } else if (st->slot[0] != 0 && st->slot[1] != 0 && st->slot[2] != 0 &&
                 st->slot[3] != 0) {
        int s = st->slot[0]->mode;

        if (s == TALLY_ICON_PARKED || (g_dwPadPressed & PAD_CROSS)) {
          g_pfnLevelFallImpactCallback();
        } else if (s == TALLY_ICON_LIT) {
          for (i = 0; i < 4; i++) {
            st->slot[i]->subMode = i;
            st->slot[i]->mode = TALLY_ICON_FINALE;
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
                st->icon[i]->mode = TALLY_ICON_UNLIT;
              } else {
                st->icon[i]->cullRadius = 0xFF;
                st->icon[i]->posX = x;
                st->icon[i]->tintMode = g_nFlightTallyShownClass + 3;
                st->icon[i]->mode = TALLY_ICON_LIT;
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

      if (g_nGamestate == GS_FLIGHT_RESULTS) {
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
    case ACTOR_TYPE_TUMBLE_SHARD:
    case ACTOR_TYPE_TUMBLE_SHARD + 1:
    case ACTOR_TYPE_TUMBLE_SHARD + 2: {
      TumbleState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
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
      } else {
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }
    case ACTOR_TYPE_SPLASH:
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* A flight-target burst shard: drifts and tumbles until its life runs out or it
     * leaves the screen. */
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 1:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 2:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 3:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 4:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 5:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 6:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 7:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 8:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 9:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 10:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 11:
    case ACTOR_TYPE_FLIGHT_TARGET_SHARD + 12: {
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
    case ACTOR_TYPE_TALLY_ICON_RING:
    case ACTOR_TYPE_TALLY_ICON_TARGET:
    case ACTOR_TYPE_TALLY_ICON_CHEST:
    case ACTOR_TYPE_TALLY_ICON_BANKER:
    case ACTOR_TYPE_TALLY_ICON_CYCLER: {
      TallyState *st = actor->state;

      switch (actor->mode) {
      case TALLY_ICON_UNLIT:
        break;
      case TALLY_ICON_LIT:
        actor->rotZ += 8;
        break;
      case TALLY_ICON_SPEND:
        svW[0] = g_nFlightTallySlot * 50 + 0x1E;
        svW[1] = 0xC8;
        svW[2] = 0x1000;
        AddVector(&st->x, svW, &actor->posX);
        RShiftVector3(&st->x, 1);
        SubtractVector(svW, &st->x, &actor->posX);
        st->radius = VectorLength(svW, VEC_LEN_XY);
        st->angle =
            ArcTan2(actor->posX - st->x, st->y - actor->posY, ATAN_ANGLE12);
        st->phase = 0x800;
        actor->mode = TALLY_ICON_TO_SLOT;
        actor->rotZ += 8;
        break;
      case TALLY_ICON_TO_SLOT:
        st->phase -= 0x40;
        actor->posX =
            st->x - ((st->radius * LookupCosine(st->angle + st->phase)) >> 12);
        actor->posY =
            st->y + ((st->radius * LookupSine(st->angle + st->phase)) >> 12);
        actor->rotZ += 8;
        if (st->phase <= 0) {
          actor->mode = TALLY_ICON_PARKED;
        }
        break;
      case TALLY_ICON_FINALE:
        svW[0] = (g_anCosineLut[actor->subMode * 64] >> sh6) + 0x100;
        svW[1] = ((g_anSineLut[actor->subMode * 64] * 5) >> 9) + 0x78;
        svW[2] = 0x1000;
        AddVector(&st->x, svW, &actor->posX);
        RShiftVector3(&st->x, 1);
        SubtractVector(svW, &st->x, &actor->posX);
        st->radius = VectorLength(svW, VEC_LEN_XY);
        st->angle =
            ArcTan2(actor->posX - st->x, st->y - actor->posY, ATAN_ANGLE12);
        st->phase = 0x800;
        actor->mode = TALLY_ICON_TO_RING;
        actor->rotZ += 8;
        break;
      case TALLY_ICON_TO_RING:
        st->phase -= 0x40;
        actor->posX =
            st->x - ((st->radius * LookupCosine(st->angle + st->phase)) >> 12);
        actor->posY =
            st->y + ((st->radius * LookupSine(st->angle + st->phase)) >> 12);
        actor->rotZ += 8;
        if (st->phase <= 0) {
          st->angle =
              ArcTan2(actor->posX - 0x100, actor->posY - 0x78, ATAN_ANGLE12);
          st->phase = -0x3000;
          st->radius = 0x40;
          actor->mode = TALLY_ICON_SPIRAL;
        }
        break;
      case TALLY_ICON_SPIRAL:
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
          actor->mode = TALLY_ICON_PARKED;
        }
        break;
      }
      break;
    }
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
                                     SPY.posY - st->owner->posY, ATAN_ANGLE8);
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

    case ACTOR_TYPE_SPARK_DEBRIS:
    case ACTOR_TYPE_SPARK_DEBRIS + 1:
    case ACTOR_TYPE_SPARK_DEBRIS + 2: {
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
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, trail);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);
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
