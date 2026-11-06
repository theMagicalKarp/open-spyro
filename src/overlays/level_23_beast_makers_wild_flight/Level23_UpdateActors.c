/* Level23_UpdateActors -- per-frame actor update for level 23 (Wild Flight).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Matching notes (0x8007CFB4, 21,128 B):
 *
 * Sibling of Level11_UpdateActors (the flight actor-update family head),
 * templated from the matched level_5 copy. See the level_11 file for the
 * load-bearing source forms shared by the whole family. Differences from
 * level_5:
 *   - new arm 0xA2 (a roller cycling over four paths); no 0x4E, 0x16A,
 *     0x197 or 0x198; 0x18C (not 0x1EA) joins the 0x183 arm.
 *   - scratch vectors as level_17 minus `svT`; no extra array in 0x4C.
 *   - the &g_anCollisionHitNormal group must be hoisted by loop.c (the original
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
#include "camera.h"
#include "level_sfx.h"
#include "level_id.h"
#include "emit_record.h"


/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. */
#define ANIM_RATE16 0x10

/* Type 0xA2: a roller that cycles over four paths. */
typedef struct CyclerState {
  PathData *path;     /* 0x00 current path */
  PathData *paths[4];      /* 0x04 */
  int pathIdx;             /* 0x14 */
  int reward;              /* 0x18 */
  int tally;               /* 0x1C */
  int roll;                /* 0x20 */
  int rollVel;             /* 0x24 */
  int zSpeed;               /* 0x28 */
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
  int nextPos[3];
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
  int bonusStep[3];
  int targetFromSpyro[3];
  int svW[3];
  int sh6;

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, ANIM_TICK_NORMAL);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? ANIM_TICK_EXTRA | ANIM_TICK_HALF_RATE : ANIM_TICK_EXTRA);
  }

  actorList = g_apActiveActorList;
  sh6 = 6;
  base = (char *)&g_nSparxHealth;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    one = 1;
    spy = (SpyroObj *)SPY_FROM_SPARX(base);

    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & ACTOR_ANIM_DONE;
    g_nActorAnimNewFrame = actor->animFlags & ACTOR_ANIM_NEW_FRAME;
    g_nActorFrameDelta = g_nFrameStep;

    switch (actor->type) {
    /* Butterfly (ButterflyState). */
    case ACTOR_TYPE_BUTTERFLY: {
      ButterflyState *st = (ButterflyState *)actor->state;

      if (SPY.sparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, &SPY.posX) < BUTTERFLY_CALL_RANGE) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < BUTTERFLY_CALL_DZ) {
          if (g_pSparxActor == 0) {
            if (SPY.sparxHealth >= SPARX_GONE) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);

              SPY.sparxHealth = one;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            SparxState *cs = g_pSparxActor->state;

            if (cs->target == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, ATAN_ANGLE8)) < 0) {
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
        SteerActorAroundPoint(actor, &spy->posX, BUTTERFLY_CIRCLE_RADIUS,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
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
    case ACTOR_TYPE_AIMED_SHOT: {
      DriftState *st = actor->state;

      if (TICK_DOWN(st->timer) == 0) {
        AddVector(nextPos, &actor->posX, st->vel);
        if (CastRayWorldAndActors(&actor->posX, nextPos) == 0 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0, SPYRO_EV_HIT) == 0) {
          CopyVector(&actor->posX, nextPos);
          break;
        }
      }
      DespawnActorRecord(actor);
      continue;
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
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
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
          CopyVector(svD, &spy->posX);
          svD[0] = svD[0] + (LookupCosine(g_bSpyroPersistentEulerZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(g_bSpyroPersistentEulerZ << 4) >> 3);
          svD[2] = svD[2] + EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->centre, svD);
          st->angle = g_bSpyroPersistentEulerZ - 0x40;
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

    /* Bouncing chest fragments (ChestShardState). */
    case ACTOR_TYPE_FLIGHT_CHEST_SHARD:
    case ACTOR_TYPE_FLIGHT_CHEST_SHARD + 1: {
      ChestShardState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
          
          int dot;

          SetVector3Magnitude(&g_anCollisionHitNormal, 0x1000);
          { int pad0; { int pad1; } } /* see header: loop.c lifetime dial */
          dot = st->vel[0] * g_anCollisionHitNormal + st->vel[1] * g_nCollisionHitNormalY +
                    st->vel[2] * g_nCollisionHitNormalZ >>
                11;
          if (dot < 0) {
            ScaleVector3ByRatio(&g_anCollisionHitNormal, 0x1000, (dot >> 2) - (dot));
            st->vel[0] += g_anCollisionHitNormal;
            st->vel[1] += AS_I32(g_nCollisionHitNormalY);
            st->vel[2] += AS_I32(g_nCollisionHitNormalZ);
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
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)svD);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
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

        st->vel[2] -= DEBRIS_GRAVITY;
        if (st->vel[2] < -DEBRIS_MAX_FALL) {
          st->vel[2] = -DEBRIS_MAX_FALL;
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

        if (st->sparkle != BILLBOARD_NONE) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset5, g_aRotBillboards[st->sparkle].pos);
          } else {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset3, g_aRotBillboards[st->sparkle].pos);
          }

          AddVector(g_aRotBillboards[st->sparkle].pos, g_aRotBillboards[st->sparkle].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->sparkle].life < 5) {
            st->sparkle = BILLBOARD_NONE;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);
            if (handle >= 0 && dist < ROT_BILLBOARD_ATTACH_RANGE) {
              st->sparkle = handle;
            }

            st->timer = (GetRandomU32() & 0x1F) + 33;
          }
        }
      }

      if (!actor->drawn && dist > COLLECT_IDLE_RANGE && actor->subMode == COLLECT_REST && st->restMode == COLLREST_STILL) {
        break;
      }

      switch (actor->subMode) {
      case COLLECT_SETTLE: {
        int *nrm = &g_anCollisionHitNormal;

        CopyVector(svA, &actor->posX);
        svA[2] += 0x400;
        FindGroundHeightBelow(svA, 0x10000);
        st->pitch = -ArcTan2_8bit(
            IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionHitNormalZ * g_nCollisionHitNormalZ)),
            g_nCollisionHitNormalY);
        st->yaw = -ArcTan2_8bit(g_nCollisionHitNormalZ, nrm[0]);

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
            EncodeHitNormalToActorGroundCode(actor);
            if (actor->type >= ACTOR_TYPE_GEM_1) {
              int *nrm = &g_anCollisionHitNormal;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionHitNormalZ * g_nCollisionHitNormalZ)),
                  g_nCollisionHitNormalY);
              st->yaw = -ArcTan2_8bit(g_nCollisionHitNormalZ, nrm[0]);
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
            EncodeHitNormalToActorGroundCode(actor);

          } else if (st->restMode == COLLREST_FIND_TRI) {
            int colIndex;

            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              colIndex = g_nCollisionHitTriIndex;
              st->restMode = COLLREST_ON_TRI;
              st->groundTri = colIndex;
            }
          }
        }

        if (dist < SPARX_FETCH_RANGE) {
          int zDist = (SPY.posZ - SPYRO_RADIUS) - actor->posZ;

          if (ABS2(zDist) < SPARX_FETCH_DZ) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0) {

                int res = CheckLineOfSightSegment(&g_pSparxActor->posX, &actor->posX);

                if (res) {
                  SparxState *cs = g_pSparxActor->state;

                  if (cs->target == 0) {
                    if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                       g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ])) {
                      g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
                      g_nSfxOverrideVolR = SPARX_BUZZ_VOLUME;
                      g_nSfxOverrideVolL = SPARX_BUZZ_VOLUME;
                      PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);
                    }

                    g_pSparxActor->subMode = SPARX_SUB_FETCH;
                    cs->target = actor;
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

        ScaleVector3Sat(bouncePos, st->vel, g_nFrameStep);
        RShiftVector3(bouncePos, 1);

        speed = VectorLength(bouncePos, VEC_LEN_XYZ);

        if (COLLECT_BOUNCE_MAX_STEP < speed) {
          ScaleVector3ByRatio(bouncePos, speed, COLLECT_BOUNCE_MAX_STEP);
        }

        AddVector(bouncePos, &actor->posX, bouncePos);

        if (-COLLECT_BOUNCE_MAX_FALL < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * COLLECT_BOUNCE_GRAVITY;
        }

        if (bouncePos[2] < 0) {
          if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          int r;
          register int *pos asm("$4"); /* a0, as in the original (A374) */

          bouncePos[2] += COLLECT_RADIUS;

          r = CollideSphereWithWorldAndActors(bouncePos, COLLECT_RADIUS, COLLECT_RADIUS);
          pos = &actor->posX;
          if (r) {
            if (GetGroundTriggerRegionType(pos) == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(bounceNormal, &g_anCollisionHitNormal);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= COLLECT_RADIUS;

            if (st->bounces == 0) {
              int groundHeight = FindGroundHeightBelow(bouncePos, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(bounceNormal[2], VectorLength(bounceNormal, VEC_LEN_XY));

              if ((bouncePos[2] - COLLECT_REST_DROP) < groundHeight && groundAngle < COLLECT_REST_MAX_SLOPE) {
                actor->subMode = COLLECT_REST;
                CopyVector(&actor->posX, bouncePos);
                actor->posZ = groundHeight;

                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((bounceNormal[0] * bounceNormal[0]) + (bounceNormal[2] * bounceNormal[2])),
                      bounceNormal[1]);
                  st->yaw = -ArcTan2_8bit(bounceNormal[2], bounceNormal[0]);

                  if (st->pitch != 0 || st->yaw != 0) {
                    actor->rotZ = 0;
                  }
                }

                EncodeHitNormalToActorGroundCode(actor);
              } else {
                st->bounces++;
              }
            }

            if (st->bounces != 0) {
              g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
              g_nSfxOverrideVolL = COLLECT_BOUNCE_VOLUME >> (3 - st->bounces);
              g_nSfxOverrideVolR = COLLECT_BOUNCE_VOLUME >> (3 - st->bounces);

              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);

              if (ReflectVectorOffNormal(st->vel, bounceNormal, st->vel)) {
                st->bounces--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            bouncePos[2] -= COLLECT_RADIUS;
            CopyVector(pos, bouncePos);
            FindGroundHeightBelow(bouncePos, 0x10000);
            EncodeHitNormalToActorGroundCode(actor);
          }
        }

        if (dist < SPARX_FETCH_RANGE) {
          int zDist = (((int *)&SPY.posX)[2] - SPYRO_RADIUS) - actor->posZ;

          if (ABS2(zDist) < SPARX_FETCH_DZ) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0 && st->bounces < 3) {

                SparxState *cs = g_pSparxActor->state;

                if (cs->target == 0) {
                  if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                     g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ])) {
                    g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
                    g_nSfxOverrideVolR = SPARX_BUZZ_VOLUME;
                    g_nSfxOverrideVolL = SPARX_BUZZ_VOLUME;
                    PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);
                  }

                  g_pSparxActor->subMode = SPARX_SUB_FETCH;
                  cs->target = actor;
                }
              }
            }
          }
        }

        break;
      }
      case COLLECT_HOMING: {
        if (st->timer >= COLLECT_HOMING_DONE) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(homingStep, &spy->posX, st->vel);
          RShiftVector3(homingStep, 5);

          if (VectorLength(homingStep, VEC_LEN_XYZ) > COLLECT_HOMING_SNAP_STEP) {
            CopyVector(&actor->posX, &spy->posX);
            st->timer = COLLECT_HOMING_DONE;
          } else {
            ScaleVector3Sat(homingStep, homingStep, st->timer);
            AddVector(&actor->posX, st->vel, homingStep);
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
      if ((st->timer > COLLECT_ARM_FRAMES && dist < COLLECT_TOUCH_RANGE && (-384 < (SPY.posZ - actor->posZ)) &&
           ((SPY.posZ - actor->posZ) < 512)) ||
          (actor->subMode == COLLECT_HOMING && st->timer > COLLECT_HOMING_COLLECT_TICK)) {

        if (g_pSparxActor != 0) {
          SparxState *cs = g_pSparxActor->state;

          if (cs->target == actor) {
            cs->target = 0;
          }
        }

        if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, 8);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, 0x10);

          if (g_nExtraLives < MAX_EXTRA_LIVES) {
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
        svF[1] = SPARX_WING_SPAN;
        svF[2] = 0;
        RotateVectorByMatrix(actor->mtx, svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);

        svF[0] = 0;
        svF[1] = -SPARX_WING_SPAN;
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
          st->glow->size += SPARX_GLOW_GROW;
          if (st->glow->size > SPARX_GLOW_MAX_SIZE) {
            st->glow->size = SPARX_GLOW_MAX_SIZE;
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

      if (st->target != 0 && ApproxDist2D(&actor->posX, &SPY.posX) > SPARX_TARGET_LEASH) {
        st->timer = 0;
      }

      if (actor->subMode == SPARX_SUB_PARKED)
        break;

      if (st->target != 0) {
        switch (actor->subMode) {
        case SPARX_SUB_CHASE: {
          Actor *target = st->target;
          ButterflyState *bp = target->state;
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
            if (TICK_DOWN(st->timer) != 0 ||
                (st->timer < SPARX_CATCH_EARLY_FRAMES &&
                 AbsAngleDelta8((actor->rotZ + (bp->circleDir * 110)) & 0xFF,
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

          SubtractVector(sparxToTarget, &st->target->posX, &actor->posX);
          dist = VectorLength(sparxToTarget, VEC_LEN_XYZ);

          if (dist < SPARX_CATCH_RANGE) {
            cp->timer = 0;
            cp->pitch = GetRandomU32() & 0xE;
            cp->yaw = GetRandomU32() & 0xE;
            cp->roll = GetRandomU32() & 0xE;

            CopyVector(cp->vel, &st->target->posX);
            st->target->subMode = COLLECT_HOMING;

            if (st->target->type == ACTOR_TYPE_EXTRA_LIFE)
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorExtraLife);
            else if (st->target->type == ACTOR_TYPE_SPARX_FOOD)
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorSparxFood);
            else
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_adwActorBurstColors[st->target->type]);

            st->target = 0;
            if (!GetSoundVoiceStatusByOwner(actor, AS_PTRU8(g_pLevelSampleBankHeader)[SFX_GEM_BOUNCE])) {
              g_nSfxOverrideFlags = SFX_OVERRIDE_VOLUME;
              g_nSfxOverrideVolR = SPARX_HANDOVER_VOLUME;
              g_nSfxOverrideVolL = SPARX_HANDOVER_VOLUME;
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);
            }
          } else {
            int dcheck = SPARX_FETCH_STEP_FAR;

            if (dcheck < dist)
              ScaleVector3ByRatio(sparxToTarget, dist, SPARX_FETCH_STEP_FAR);
            else
              ScaleVector3ByRatio(sparxToTarget, dist, SPARX_FETCH_STEP_NEAR);

            AddVector(&actor->posX, &actor->posX, sparxToTarget);
            TurnActorYawTowardAngle(actor, ArcTan2(sparxToTarget[0], sparxToTarget[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            pitchAngle = ArcTan2(VectorLength(sparxToTarget, VEC_LEN_XY), sparxToTarget[2], ATAN_ANGLE8);
            actor->rotY = pitchAngle;
            actor->rotY = StepAngle8Toward(pitchAngle, 0, 0x30);
          }
          break;
        }
        }
      } else {

        actor->cullRadius = 0x10;
        if (st->timer <= 0) {

          svF[0] = -SPARX_HOVER_BEHIND;
          svF[1] = (GetRandomU32() & 0x310) - 0x188;
          if (CAM.state == CAM_MODE_CHARGE) {
            svF[2] = (GetRandomU32() & 0x7F) + SPARX_CHARGE_HOVER_MIN_Z;
          } else {
            svF[2] = (GetRandomU32() & 0x1FF) - 0xC8;
          }
          st->drift[0] = svF[0];
          st->drift[1] = svF[1];
          st->drift[2] = svF[2];
          st->timer = GetRandomU32() & 0x7B;
        } else {
          st->timer -= g_nFrameStep;
          if (CAM.state == CAM_MODE_CHARGE) {
            svF[0] = st->drift[0] + (SPY.motionSpeed >> 2);
            svF[1] = st->drift[1];
            svF[2] = st->drift[2];
            if (svF[2] < SPARX_CHARGE_HOVER_MIN_Z)
              svF[2] = SPARX_CHARGE_HOVER_MIN_Z;
          } else {
            if (CAM.state == (CAM_HELD | CAM_MODE_FREE_LOOK))
              svF[0] = st->drift[0] - SPARX_FREE_LOOK_PULLBACK;
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

          if (svF[2] > SPARX_MAX_PITCH)
            svF[2] = SPARX_MAX_PITCH;
          if (svF[2] < -SPARX_MAX_PITCH)
            svF[2] = -SPARX_MAX_PITCH;
          actor->rotX = 0;
          actor->rotY = svF[2];
          TurnActorYawTowardAngle(actor, SPY.bodyRotZ, 4, 0, YAW_HOLD_IN_BAND);
        }

        switch (SPY.sparxHealth) {
        case SPARX_GREEN:
          if (actor->nextAnim != SPARX_ANIM_GREEN) {
            if (actor->anim != SPARX_ANIM_GREEN) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_GREEN);
            }
          }
          break;
        case SPARX_BLUE:
          if (actor->nextAnim != SPARX_ANIM_BLUE) {
            if (actor->anim != SPARX_ANIM_BLUE) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_BLUE);
            }
          }

          break;
        case SPARX_GOLD:
          if (actor->nextAnim != SPARX_ANIM_GOLD) {
            if (actor->anim != SPARX_ANIM_GOLD) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, SPARX_ANIM_GOLD);
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
        st->zSpeed = 0xC8;
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
        PlayActorMeshVoice(actor, FTARGET_VOICE_BURST, 0);
        actor->mode = FTARGET_STRUCK;
        ANIM_RESET(actor, FTARGET_STRUCK);
        break;
      }

      switch (actor->mode) {
      case FTARGET_ACTIVE: {
        int d;

        if (st->legStep == 0xFF) {
          st->path->cur++;
          if (st->path->cur >= st->path->count) {
            st->path->cur = 0;
          }
          st->legStep = 0;
          ZeroVector(st->legVec);
        }
        d = actor->rotZ;
        if (g_nSpyroSubstate < SPYRO_SUB_SCRIPTED &&
            TweenActorAlongPath(actor, st->path, st->legVec, &st->legStep, 0xA, TWEEN_FLY) == TWEEN_WRAPPED) {
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
          st->path->cur = 0;
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
          if (actor->nextAnim != FTARGET_SPENT) {
            actor->frameProgress = 0x10;
            actor->frameRate = 0x10;
            actor->nextAnim = FTARGET_SPENT;
            actor->nextFrame = 0;
            OnActorAnimQueued(actor);
          }
          continue;
        }
        /* fall through */
      case FTARGET_SPENT:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC,
                          0xC) == MOVED_LANDED) {
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

      if (ApproxDist2D(&actor->posX, &spy->posX) < KEY_TOUCH_RANGE) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < KEY_TOUCH_DZ
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < KEY_TOUCH_DZ) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (0x8080 + 0x1000000 * i));
          }

          g_nLevelKeyState = KEYSTATE_CARRIED;
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->gateChunk = GATE_CHUNK_ALWAYS;
          actor->subMode = KEY_TAKEN;
        }
      }

      if (st->attach != ROT_BILLBOARD_NONE) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->attach].pos);
        AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                      &actor->posX);
        if (g_aRotBillboards[st->attach].life < 5) {
          st->attach = ROT_BILLBOARD_NONE;
        }
      } else if (st->timer >= ROT_BILLBOARD_GLINT_TICK) {
        ang = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);
        if (ang >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < ROT_BILLBOARD_ATTACH_RANGE) {
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
              
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(&g_anCollisionHitNormal, 0x1000);

              dot = (st->vel[0] * g_anCollisionHitNormal + st->vel[1] * g_nCollisionHitNormalY +
                     st->vel[2] * g_nCollisionHitNormalZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(&g_anCollisionHitNormal, 0x1000, -dot);
                st->vel[0] += g_anCollisionHitNormal;
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

    /* Flight-level chest: flaming or charging it frees its reward and
     * scatters shards; otherwise it hums its ambient sound. */
    case ACTOR_TYPE_FLIGHT_CHEST: {
      FlightChestState *st = actor->state;
      int i;

      if (actor->flags & ACTOR_HITS_SPYRO) {
        Actor *reward;

        PlayActorMeshVoice(actor, FCHEST_VOICE_BURST, 0);
        TallyFlightTarget(st->tallyClass);
        reward = g_pfnLevelSpawnActor(TIME_BONUS_TYPE(st->reward), actor);
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
        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x20);
        DespawnActorRecord(actor);
        break;
      }

      {
        Model **models = g_apActorMeshTable;

        if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable_12B->m_Sounds[FCHEST_VOICE_HUM]) == 0 && g_nGamestate == GS_GAMEPLAY) {
          PlaySoundEffect(models[actor->type]->m_Sounds[FCHEST_VOICE_HUM], actor, SFX_MODE_3D,
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

        for (i = 0; i < st->path->count; i++) {
          int d = ApproxDist2D(&actor->posX, (int *)((unsigned char *)st->path + ((i << 4) + 8)));

          if (d < best) {
            best = d;
            st->path->cur = i;
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
        st->zSpeed = 0;
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
        PlayActorMeshVoice(actor, FTARGET_VOICE_BURST, 0);
        actor->mode = BANKER_STRUCK;
        ANIM_RESET(actor, BANKER_STRUCK);
        break;
      }

      switch (actor->mode) {
      case BANKER_FLY: {
        int d = actor->rotZ;

        if (g_nActiveLevelId == LEVEL_SUNNY_FLIGHT || g_nActiveLevelId == LEVEL_WILD_FLIGHT) {
          TweenActorAlongPath(actor, st->path, st->legVec, &st->legStep, 0xC, TWEEN_FLY);
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
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC,
                          0xC) == MOVED_LANDED) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = BANKER_FLEE;
          ANIM_RESET(actor, BANKER_FLEE);
        }
        break;
      case BANKER_FLEE:
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC,
                          0xC) == MOVED_LANDED) {
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
        actor->cullRadius = ACTOR_CULL_ALWAYS;
        actor->otBias = 0x20;
        actor->mode = TIMEBONUS_TO_HUD;
        PlaySoundEffect(g_pLevelSampleBankHeader[SFX_FLIGHT_COLLECT], actor, SFX_MODE_PANNED, 0);
        break;
      case TIMEBONUS_TO_HUD: {
        int d;

        bonusStep[0] = 0x1A4;
        bonusStep[1] = 0x30;
        bonusStep[2] = 0x157C;
        SubtractVector(bonusStep, bonusStep, &actor->posX);
        d = VectorLength(bonusStep, VEC_LEN_XYZ);
        if (d < 0x20) {
          actor->mode = TIMEBONUS_ARRIVED;
          actor->posX = 0x1A4;
          actor->posY = 0x30;
          actor->posZ = 0x157C;
          *st = 0x1E;
        } else {
          if (d > 0x80) {
            ScaleVector3ByRatio(bonusStep, d, 0x80);
          }
          AddVector(&actor->posX, &actor->posX, bonusStep);
        }
        break;
      }
      case TIMEBONUS_ARRIVED:
        if (TICK_DOWN(*st) != 0) {
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

      if (ApproxDist2D(&actor->posX, &spy->posX) < FLIGHT_TARGET_NEAR_RANGE) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < FLIGHT_TARGET_NEAR_DZ : SPY.posZ - height < FLIGHT_TARGET_NEAR_DZ) {
          SubtractVector(targetFromSpyro, &actor->posX, &spy->posX);
          svW[2] = (targetFromSpyro[2] - 0xC0) * 4;
          targetFromSpyro[2] = (svW[2] - targetFromSpyro[2]) >> 2;
          if (VectorLength(targetFromSpyro, VEC_LEN_XYZ) < FLIGHT_TARGET_HIT_RANGE &&
              PlaneDistanceAbs(&spy->posX, st) < FLIGHT_TARGET_PLANE_DIST) {
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
              RotateVectorByMatrix(actor->mtx, &g_anActorShardOffsets[i * 3], targetFromSpyro);
              AddVector(&shard->posX, &actor->posX, targetFromSpyro);
              RShiftVector3(targetFromSpyro, 5);
              svW[0] = (GetRandomU32() & 0x7E) - 0x3F;
              svW[1] = (GetRandomU32() & 0x7E) - 0x3F;
              svW[2] = (GetRandomU32() & 0x7E) - 0x3F;
              AddVector(&ss->posX, targetFromSpyro, svW);
              AddVector(&shard->posX, &shard->posX, &ss->posX);
              ss->spinX = (GetRandomU32() & 2) - 1;
              ss->spinY = (GetRandomU32() & 2) - 1;
              ss->spinZ = (GetRandomU32() & 2) - 1;
              ss->life = 0x3C;
            }

            PlayActorMeshVoice(actor, FTARGET_VOICE_BURST, 0);
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
          st->digit[i]->cullRadius = ACTOR_CULL_ALWAYS;
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
            st->digit[i]->cullRadius = ACTOR_CULL_ALWAYS;
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
          g_aHudIconActorRecords[i].type = st->types[i];
          InitActorRenderDefaults(&g_aHudIconActorRecords[i]);
          HideActorRenderRecord(&g_aHudIconActorRecords[i]);
          g_aHudIconActorRecords[i].posY = y;
          y += 0x14;
          g_aHudIconActorRecords[i].otBias = 0x20;
          g_aHudIconActorRecords[i].cullRadius = ACTOR_CULL_ALWAYS;
          g_aHudIconActorRecords[i].posX = 0x3C;
          g_aHudIconActorRecords[i].posZ = 0x1000;
          g_aHudIconActorRecords[i].tintMode = i + 3;
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
            g_pfnLevelEndFlightRun();
          }
        }
        g_nFlightRunClock += g_nFrameStep;
        if (g_nFlightRunClock > 0x8C9F) {
          g_nFlightRunClock = 0x8C9F;
        }
      } else {
        ChangeSpyroState(SPYRO_ST_FLY);
        g_nSpyroSubstate = SPYRO_SUB_FLY_RUN;
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_NO_CLIP);
      }

      if (g_anFlightClearTimes[g_nActiveWorldIndex] != 0) {
        t = g_nFlightRunClock;
        i = t / 3600;
        if (i != 0) {
          st->digit[6]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
          st->digit[4]->cullRadius = ACTOR_CULL_ALWAYS;
          st->digit[5]->cullRadius = ACTOR_CULL_ALWAYS;
          st->digit[6]->cullRadius = ACTOR_CULL_ALWAYS;
        } else {
          st->digit[4]->cullRadius = ACTOR_CULL_HIDDEN;
          st->digit[5]->cullRadius = ACTOR_CULL_HIDDEN;
          st->digit[6]->cullRadius = ACTOR_CULL_HIDDEN;
        }
        i = t * 10 / 6 % 10;
        st->digit[7]->type = ACTOR_TYPE_GEM_DIGIT_0 + i;
        st->digit[7]->cullRadius = ACTOR_CULL_ALWAYS;
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
        st->digit[0]->cullRadius = ACTOR_CULL_ALWAYS;
      } else {
        st->digit[0]->cullRadius = ACTOR_CULL_HIDDEN;
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
        if (st->count != 0 && TICK_DOWN(st->timer) != 0) {
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
          g_pfnLevelEndFlightRun();
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
                st->icon[i]->cullRadius = ACTOR_CULL_ALWAYS;
                st->icon[i]->posX = x;
                st->icon[i]->tintMode = 0;
                st->icon[i]->rotZ = 0;
                st->icon[i]->mode = TALLY_ICON_UNLIT;
              } else {
                st->icon[i]->cullRadius = ACTOR_CULL_ALWAYS;
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
            st->icon[i]->cullRadius = ACTOR_CULL_HIDDEN;
          }
          if (st->spent[i] != 0) {
            st->spent[i]->cullRadius = ACTOR_CULL_HIDDEN;
          }
        }
        for (i = 0; i < 8; i++) {
          if (st->digit[i] != 0) {
            st->digit[i]->cullRadius = ACTOR_CULL_HIDDEN;
          }
          if (st->slot[i] != 0) {
            st->slot[i]->cullRadius = ACTOR_CULL_HIDDEN;
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

        st->vel[2] -= DEBRIS_GRAVITY;
        if (st->vel[2] < -DEBRIS_MAX_FALL) {
          st->vel[2] = -DEBRIS_MAX_FALL;
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

      if (actor->drawn != 0 && TICK_DOWN(ss->life) == 0) {
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
      NameLetterState *st = actor->state;

      if (st->sign->mode == ORBIT_OWNER_FAR || st->sign->subMode != actor->mode) {
        DespawnActorRecord(actor);
      } else if (st->sign->type == ACTOR_TYPE_LEVEL_EXIT) {
        if (ApproxDist2D(&st->sign->posX, &spy->posX) < LEVELEXIT_ENTER_RANGE) {
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
          anglespyro = ArcTan2(spy->posX - st->sign->posX,
                                     SPY.posY - st->sign->posY, ATAN_ANGLE8);
          vecA[0] = g_anCosineLut[anglespyro] * 3 >> 4;
          vecA[1] = g_anSineLut[anglespyro] * 3 >> 4;
          vecA[2] = 0;
          vecB[0] = g_anCosineLut[anglespyro + 64 & 0xFF] >> 5;
          vecB[1] = g_anSineLut[(anglespyro + 64) & 0xFF] >> 5;
          vecB[2] = 0;
          anglespyro = (anglespyro + 0x80) & 0xFF;
          actor->rotZ = anglespyro + (g_anCosineLut[actor->subMode] * 3 >> 9);
          ScaleVector3Sat(vecC, vecB, st->count - 1);
          LShiftVector3(vecB, 1);
          anglespyro = 2;
          anglespyro = (st->count - 1) * anglespyro;
          vecC[2] += g_anCosineLut[anglespyro] * 3 >> 3;
          vecD[0] = vecA[0] * g_anCosineLut[anglespyro];
          vecD[1] = vecA[1] * g_anCosineLut[anglespyro];
          vecD[2] = 0;
          anglespyro = ((anglespyro - st->index * 4)) & 0xFF;
          ScaleVector3Sat(vecB, vecB, st->index);
          SubtractVector(vecC, vecC, vecB);
          actor->posX = vecA[0] * g_anCosineLut[anglespyro];
          actor->posY = vecA[1] * g_anCosineLut[anglespyro];
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, vecD);
          RShiftVector3(&actor->posX, 10);
          AddVector(&actor->posX, &actor->posX, vecA);
          AddVector(&actor->posX, &actor->posX, &st->sign->posX);
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
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)trail);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
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
