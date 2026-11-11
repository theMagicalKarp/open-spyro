/* Level24_UpdateActors -- per-frame actor update for level 24 (Dream
 * Weavers home). 0x8007E3C0, 43,620 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. The
 * level-24 classes are the cannon (91) and its comets (104), the guards
 * (92/130) and the talking NPC (191); the rest are shared with the other
 * level heads.
 *
 * Load-bearing source forms:
 *   - No loop-top carriers: every hoisted constant spills, the preheader is
 *     empty and jump2 turns the loop entry into a `j` to the bottom test.
 *   - Case 91 state 10: the comet's state is written timer, steps, count,
 *     dvz, speed, spark. The steps read must precede every cs store after
 *     the timer (it aliases them), and the count store ahead of dvz keeps
 *     the speed constant out of the slot the call's a2 = 800 needs, so the
 *     second rotZ index stays in a2.
 */

#include "actor.h"
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
#include "cd_stream.h"
#include "actor_cutscene.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. The
 * clones that need a loop-top carrier for 0x10 declare `int sixteen`; here
 * it is the plain constant, so arm bodies read the same in every level. */
#define ANIM_RATE16 sixteen
#define sixteen 0x10

extern CometTrailPoint *g_apMotionTrailSlices[]; /* trails */

#define spy (&SPY)
#define one 1

void Level24_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  char *base;
  int dt;

  /* Scratch vectors, in the original's slot order: shared ones named by sp
   * offset (gNNN), single-use ones by role with the offset noted. */
  int g056[3];
  int eggArcBase[3];
  int eggArcOffset[3];
  int g104[3];
  int eggViewPos[3];
  int g136[3];
  int g152[3];
  int lobTarget[3]; /* sp 168 */
  int lobFrames[2]; /* sp 184 */
  int targetPos[3]; /* sp 192 */
  volatile int g208v;
  int arcEnd[3]; /* sp 216 */
  int triVerts[3][3]; /* sp 232 */
  int bouncePos[3];
  int bounceNormal[3];
  int homingStep[3];
  int sparxToTarget[3];
  int gripA[3];
  int gripB[3];
  int g368[3];
  int sparkPosVel[6];
  int g408[3];
  int sparkleTwinkle[3];
  int shardTwinkle[3];
  int jetPos[6];
  int g480[3];
  int g496[3];
  int podSparkVel[3];
  int letterSide[3];
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
  base = (char *)&g_nSpyroWorldPosZ;

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
    /* Portal name text: shown while the camera faces the portal. */
    case ACTOR_TYPE_PORTAL_NAME: {
      int shouldSpawnText;
      int cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == PORTALNAME_FAR;

        actor->mode = one;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ,
            ArcTan2(CAM.posX - actor->posX, CAM.posY - actor->posY, ATAN_ANGLE8));

        if (cameraAngleDiff <= 56) {
          if (actor->subMode == NAMESIDE_BACK) {
            shouldSpawnText = 1;
          }
          actor->subMode = NAMESIDE_FRONT;
        } else if (cameraAngleDiff > 71) {
          if (actor->subMode == NAMESIDE_FRONT) {
            shouldSpawnText = 1;
          }
          actor->subMode = NAMESIDE_BACK;
        } else {
          actor->mode = PORTALNAME_FAR;
        }

        if (actor->mode && shouldSpawnText) {
          SpawnNameLetterActors(actor, NAME_LETTERS_LEVEL);
        }
      } else {
        actor->mode = PORTALNAME_FAR;
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
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
        } else {
          actor->updateDist = UPDATE_DIST(0x8000);
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

            st->pos[2] += GROUND_PROBE_LIFT;

            if (st->dropToSpyro != 0) {
              SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, FOLLOWER_DROP_PROBE_DEPTH) != 0) {
              st->pos[2] -= GROUND_PROBE_LIFT;
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
          st->release = FOLLOWER_RELEASE_STAGGER;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < ACTOR_MODE_DORMANT && parent->type == ACTOR_TYPE_FOLLOWER &&
                ((FollowState *)parent->state)->parent == st->parent) {
              st->release += FOLLOWER_RELEASE_STAGGER;
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

      if (SPY.sparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, &SPY.posX) < BUTTERFLY_CALL_RANGE) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < BUTTERFLY_CALL_DZ) {
          if (g_pSparxActor == 0) {
            if (SPY.sparxHealth >= SPARX_GONE) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, SPARX_VOICE_BUZZ, 0);

              SPY.sparxHealth = SPARX_GREEN;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            SparxState *cs = g_pSparxActor->state;

            if (cs->target == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, ATAN_ANGLE8)) <
                  0) {
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

      if (actor->cullRadius & ACTOR_CULL_ALWAYS_BIT) {
        break;
      }

      switch (actor->mode) {
      case BUTTERFLY_FLUTTER: {
        if (TICK_DOWN(st->offscreenTimer) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TICK_DOWN(st->headingTimer) != 0) {
            int r = RandomSignedInRange(BUTTERFLY_VEER_MIN, BUTTERFLY_VEER_MAX);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->headingTimer = RandomIntInRange(BUTTERFLY_VEER_MIN_FRAMES, BUTTERFLY_VEER_MAX_FRAMES);
          }

          if (TICK_DOWN(st->climbTimer) != 0) {
            st->vz = RandomSignedInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < BUTTERFLY_FLUTTER_MIN_HEIGHT) {
            st->vz = RandomIntInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
          }

          if (BUTTERFLY_FLUTTER_MAX_HEIGHT < refHeight && refHeight < BUTTERFLY_DROP_HEIGHT) {
            st->vz = -RandomIntInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, YAW_STEP_IN_BAND);
          moved = MoveActorAlongHeading(actor, BUTTERFLY_SPEED, 0, BUTTERFLY_RADIUS, MOVE_LIFT);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(BUTTERFLY_BOUNCE_MIN, BUTTERFLY_BOUNCE_MAX));
            st->headingTimer = RandomIntInRange(BUTTERFLY_BOUNCE_MIN_FRAMES, BUTTERFLY_BOUNCE_MAX_FRAMES);
          }

          if (ApproxDist2D(&actor->posX, st->home) > BUTTERFLY_LEASH) {
            int angle = ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, ATAN_ANGLE8);

            st->heading = angle;
            st->headingTimer = BUTTERFLY_LEASH_TURN_FRAMES;
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
                      8, BUTTERFLY_CIRCLE_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);

        if (g_pSparxActor == 0 || g_pSparxActor->subMode == SPARX_SUB_PARKED) {
          actor->mode = BUTTERFLY_FLUTTER;
        } else {
          int refHeight;

          if (TICK_DOWN(st->climbTimer)) {
            st->vz = RandomSignedInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < BUTTERFLY_CIRCLE_MIN_HEIGHT) {
            st->vz = RandomIntInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
          }

          if (BUTTERFLY_CIRCLE_MAX_HEIGHT < refHeight && refHeight < BUTTERFLY_DROP_HEIGHT) {
            st->vz = -RandomIntInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
          }
        }
        break;
      }
      }

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
        actor->mode += 1;
        break;

      case EGG_SPIRAL_UP: {
        st->phase += g_nFrameStep;

        if (st->phase < EGG_SPIRAL_FRAMES) {
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

          CopyVector(g056, &SPY.posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * EGG_APEX_DIAG_4;
          y = cs * EGG_APEX_DIAG_4;
          g056[0] += (y + x) * 4 >> 12;
          g056[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * EGG_APEX_DIAG_4) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * (unsigned)EGG_APEX_DIAG_4)) *
                        4 >>
                    12;
          g056[2] += EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->apex, g056);
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

        if (st->phase < EGG_FLIGHT_FRAMES) {
          SVectorToVectorShl2(eggArcBase, st->centre);
          SVectorToVectorShl2(eggArcOffset, st->apex);
          SubtractVector(eggArcOffset, eggArcOffset, eggArcBase);
          ScaleVector3Sat(eggArcOffset, eggArcOffset, st->phase);
          RShiftVector3(eggArcOffset, 6);
          AddVector(&actor->posX, eggArcBase, eggArcOffset);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(g104, &SPY.posX);
          g104[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[2] += EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->centre, g104);
          st->radius = EGG_SPIRAL_RADIUS;
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

        if (st->phase < EGG_SPIRAL_FRAMES) {
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
        SubtractVector(eggViewPos, &actor->posX, &CAM.posX);
        WorldToCameraRotate(eggViewPos, eggViewPos);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, eggViewPos, 0);
        g_nCinematicActive = 0;
        ZeroVector(&g_nSpyroSwingEulerTargetX);
        CommitActorKillGems(0, st->owner);
        g_nTotalEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Comet spark: replays its animation subMode times, then shares the
     * one-shot effects' exit. */
    case ACTOR_TYPE_COMET_SPARK:
      if (actor->subMode != 0) {
        actor->subMode--;
        actor->frame = 0;
        actor->nextFrame = 1;
        actor->frameProgress = 0;
      }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 91: a cannon. Watches for Spyro to stand in its sights and
     * blasts him, or lobs a comet (104) at whatever its trigger names. */
    case ACTOR_TYPE_COMET_CANNON: {
      CometCannonState *st = actor->state;
      Actor *p = &g_pActorListBase[st->link];

      RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      if (actor->nextAnim >= 3 && actor->nextFrame >= 0xE) {
        ANIM_GO(actor, CCANNON_WATCH);
      }
      if (st->init == 0) {
        st->init = one;
        st->heading = actor->rotZ << 4;
      }
      if (st->camFlag != 0) {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_LOCK_ON);
        g_pScriptedCameraActor = actor;
      }

      switch (actor->mode) {
      case CCANNON_WATCH:
        if (actor->subMode != 0 && p->mode < 0x7F) {
          st->target = actor->subMode;
          actor->subMode = 0;
          st->idx = (st->idx + 1) % 3;
          actor->mode = CCANNON_LOB;
          continue;
        }
        if (p->mode >= ACTOR_MODE_DORMANT) {
          int range = CCANNON_LOCK_RANGE;
          int a = 0x20;
          int b = 0x50;

          if (st->camFlag != 0) {
            range = CCANNON_LOCK_RANGE_HELD;
            a = 0x2C;
            b = 0x41;
          }
          TICK_DOWN(st->timer);
          if (actor->flags & ACTOR_HIT_FLAME) {
            int h;

            actor->flags = 0;
            g104[0] = -0x3E8;
            g104[1] = 0;
            g104[2] = 0x172;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
            RotateVectorByMatrix(actor->mtx, g104, g104);
            AddVector(g104, g104, &actor->posX);
            h = ArcTan2(g104[0] - g_anSpyroWorldPos, g104[1] - g_nSpyroWorldPosY, ATAN_ANGLE8);
            if (st->timer == 0 && AbsAngleDelta8(h, g_bSpyroPersistentEulerZ) < 0x30) {
              actor->mode = CCANNON_ARM;
              continue;
            }
          }
          st->camFlag = 0;
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < range) {
            int d1 = WrapAngleDelta8(actor->rotZ, *(unsigned char *)&g_bSpyroPersistentEulerZ);
            int ad = ABS(d1);

            if (0x40 - a < ad && ad < a + 0x40) {
              int d2 = WrapAngleDelta8(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                                   g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8),
                                     actor->rotZ);

              if (b < ABS(d2) &&
                  ((d1 < 0 && d2 < 0) || (d1 > 0 && d2 > 0))) {
                st->camFlag = one;
              }
            }
          }
        } else {
          st->heading = actor->rotZ << 4;
        }
        actor->flags = 0;
        break;

      case CCANNON_ARM:
        st->timer = CCANNON_FUSE_FRAMES;
        actor->mode = CCANNON_FIRE;
        continue;

      case CCANNON_FUSE:
        if (st->timer >= 0xB) {
          int i;

          actor->flags = 0;
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 3, g056);
          g136[0] = -0x10;
          g136[1] = 0;
          g136[2] = -0xE;
          RotateVectorByMatrix(actor->mtx, g136, g136);
          i = 0;
          do {
            i++;
            g136[0] += (GetRandomU32() & 0xF) - 8;
            g136[1] += (GetRandomU32() & 0xF) - 8;
            g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, g056, (int)g136);
          } while (i < 5);
        }
        if (TICK_DOWN(st->timer) != 0) {
          StopSoundVoicesByOwner(actor, VOICE_STOP_LOOPING);
          actor->mode = CCANNON_FIRE;
          continue;
        }
        break;

      case CCANNON_FIRE: {
        Actor *o;
        CometState *cs;
        Actor *best;
        Actor *q;
        int bestScore;

        PlayActorMeshVoice(actor, CCANNON_VOICE_FIRE, 0);
        st->idx = (st->idx + 1) % 3;
        o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET, actor);
        cs = o->state;
        CopyVector(&o->posX, &actor->posX);
        o->rotZ = actor->rotZ;
        MoveActorAlongHeading(o, CCANNON_MUZZLE_FWD, 0, 0, 0);
        o->posZ += CCANNON_MUZZLE_UP;
        cs->speed = CCANNON_COMET_SPEED;
        cs->dvz = -6;
        cs->vz = CCANNON_COMET_VZ;
        cs->timer = CCANNON_COMET_LIFE;
        cs->steps = 0;
        cs->count = 0;
        cs->spark = 0;
        cs->life = 0;
        cs->lifeOn = 0;
        best = 0;
        cs->trail = (CometTrailPoint *)((char *)st + (st->idx * 0xFDC + 0x28));
        o->cullRadius = ACTOR_CULL_HIDDEN;
        o->updateDist = ACTOR_UPDATE_ALWAYS;
        bestScore = 0xA410;
        actor->flags = 0;
        for (q = g_pActorListBase; q < g_pActorPoolDynBase; q++) {
          int ang;
          int d;

          if (q == actor || q->mode >= ACTOR_MODE_DORMANT || q->type == ACTOR_TYPE_DRAGON_STATUE ||
              q->type == ACTOR_TYPE_SAVE_HOST || q->collision == 0 || q->collision >= 0) {
            continue;
          }
          SubtractVector(g152, &q->posX, &actor->posX);
          d = ABS(g152[0]) + ABS(g152[1]);
          if (d > CCANNON_SEEK_QUICK) {
            continue;
          }
          ang = (ArcTan2(q->posX - actor->posX, q->posY - actor->posY,
                               ATAN_ANGLE8) -
                 actor->rotZ) &
                0xFF;
          if (ang > 0x80) {
            ang -= 0x100;
          }
          ang = ABS(ang);
          if (ang >= CCANNON_SEEK_CONE) {
            continue;
          }
          if (d <= CCANNON_SEEK_EXACT) {
            d = VectorLength(g152, VEC_LEN_XY);
          } else {
            d = ApproxDist2D(&q->posX, &actor->posX);
          }
          if (d > CCANNON_SEEK_RANGE) {
            continue;
          }
          if (d > CCANNON_SEEK_FAR) {
            d -= CCANNON_SEEK_FAR;
          } else if (d < CCANNON_SEEK_NEAR) {
            d = CCANNON_SEEK_NEAR - d;
          } else {
            d = 0;
          }
          d += ang << 12;
          if (d < bestScore) {
            best = q;
            bestScore = d;
          }
        }
        if (best != 0) {
          CopyVector(lobTarget, &best->posX);
          if (best->type == ACTOR_TYPE_LUNGING_GUARD || best->type == ACTOR_TYPE_BIG_LUNGING_GUARD) {
            lobTarget[2] += CCANNON_GUARD_AIM_LIFT;
          }
          o->rotZ = ArcTan2(best->posX - actor->posX,
                                   best->posY - actor->posY, ATAN_ANGLE8);
          cs->dvz = -8;
          cs->speed = CCANNON_AIM_SPEED;
          {
            int t = ComputeLobVelocityZ(&o->posX, lobTarget, CCANNON_AIM_SPEED, cs->dvz, lobFrames);

            cs->life = lobFrames[0];
            cs->vz = t;
          }
        }
        ANIM_GO(actor, CCANNON_FIRE);
        actor->mode = CCANNON_WATCH;
        continue;
      }

      case CCANNON_LOB:
        if (actor->nextAnim == CCANNON_FIRE) {
          g_nActorAnimDone = 0;
          actor->anim = CCANNON_FIRE;
          actor->frameProgress = 0;
          actor->frame = 0;
          actor->nextFrame = 1;
        }
        ANIM_GO(actor, CCANNON_FIRE);
        if (actor->drawn == 0 ||
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                        g_pActorListBase[st->target].posY - actor->posY,
                                        ATAN_ANGLE8),
                          6, 0x14, one) != 0) {
          Actor *o;
          CometState *cs;
          int v;
          int i;

          CopyVector(targetPos, &g_pActorListBase[st->target].posX);
          o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET, actor);
          o->rotZ = ArcTan2(targetPos[0] - o->posX, targetPos[1] - o->posY, ATAN_ANGLE8);
          CopyVector(&o->posX, &actor->posX);
          targetPos[2] += CCANNON_GUARD_AIM_LIFT;
          cs = o->state;
          o->posX += (g_anCosineLut[o->rotZ] * 0x7D) >> 8;
          o->posY += (g_anSineLut[o->rotZ] * 0x7D) >> 8;
          o->posZ += CCANNON_LOB_UP;
          o->cullRadius = ACTOR_CULL_HIDDEN;
          o->updateDist = ACTOR_UPDATE_ALWAYS;
          cs->timer = CCANNON_LOB_LIFE;
          cs->steps = st->steps;
          cs->count = 0;
          cs->dvz = -7;
          cs->speed = CCANNON_LOB_SPEED;
          cs->spark = 0;
          cs->vz = ComputeLobVelocityZ(&o->posX, targetPos, CCANNON_LOB_SPEED, cs->dvz, (int *)&g208v);
          cs->trail = (CometTrailPoint *)((char *)st + (st->idx * 0xE38 + 0x28)); /* not FIRE's 0xFDC stride (CometCannonState.trails) */
          cs->lifeOn = one;
          v = cs->vz;
          cs->life = g208v;
          CopyVector(arcEnd, &o->posX);
          for (i = 0; i < *(int *)&g208v; i++) {
            arcEnd[0] += (cs->speed * g_anCosineLut[o->rotZ]) >> 12;
            arcEnd[1] += (cs->speed * g_anSineLut[o->rotZ]) >> 12;
            arcEnd[2] += v;
            v += cs->dvz;
          }
          SubtractVector(arcEnd, targetPos, arcEnd);
          AddVector(&o->posX, &o->posX, arcEnd);
          actor->mode = CCANNON_WATCH;
          continue;
        }
        continue;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 92: a guard. Wakes its partner when Spyro comes into range,
     * paces its path, lunges, and spins away when struck. */
    case ACTOR_TYPE_LUNGING_GUARD: {
      LungingGuardState *st = actor->state;
      int dist;

      SubtractVector(g104, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
      if (dist < EXACT_DIST_RANGE) {
        dist = VectorLength(g104, VEC_LEN_XY);
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != LGUARD_KNOCKED &&
          (st->kind != LGKIND_BIG || !(actor->flags & ACTOR_HIT_CHARGE))) {
        st->knockZSpeed = 0;
        st->knockSpeed = LGUARD_KNOCK_SPEED;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knockSpeed = LGUARD_CHARGE_KNOCK_SPEED;
        }
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8),
                                    g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        actor->flags = 0;
        st->spin = RandomSignedInRange(LGUARD_SPIN_MIN, LGUARD_SPIN_MAX);
        actor->subMode = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ANIM_GO(actor, st->base + 3);
        actor->mode = LGUARD_KNOCKED;
        continue;
      }
      if (st->init == 0) {
        st->init = one;
        if (st->kind == LGKIND_SMALL) {
          st->base = 4;
          ANIM_SET(actor, st->base);
        }
      }
      if (actor->mode < LGUARD_KNOCKED && (st->range != 0 || (actor->flags & ACTOR_HIT_BLAST))) {
        if (st->range > 0 && actor->mode == LGUARD_IDLE &&
            (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range ||
             IsPointInRotatedBox(&spy->posX, &st->vol) != 0) &&
            TICK_DOWN(st->reloadTimer) != 0) {
          st->range = -st->range;
          if (st->range == 0) {
            st->range = -1;
          }
          g_pActorListBase[st->partner].subMode = actor - g_pActorListBase;
          /* CometCannonState.steps, as a plain word store: the typed view
           * reschedules the whole function. */
          *(int *)g_pActorListBase[st->partner].state = st->cometSteps;
          st->reloadTimer = st->reload;
        } else if (actor->flags & ACTOR_HIT_BLAST) {
          actor->flags = 0;
          if (st->flags & LGUARD_F_REARM) {
            st->range = -st->range;
          } else {
            st->range = 0;
          }
          if (st->kind != LGKIND_SMALL) {
            actor->scale = ACTOR_SCALE_UNIT;
            actor->mode = LGUARD_TO_SMALL;
            continue;
          }
          BurstActorWithSound(actor, LGUARD_TO_BIG_SPARK_LIFE);
          actor->scale = LGUARD_TO_BIG_SCALE;
          st->base = 0;
          ANIM_SET(actor, st->base);
          actor->mode = LGUARD_TO_BIG;
          continue;
        } else if (st->range < 0) {
          TICK_DOWN(st->reloadTimer);
        }
      }
      actor->flags = 0;
      if (st->flags == LGUARD_F_ESCORT) {
        st->flags = 0;
        actor->mode = LGUARD_ESCORT;
        continue;
      }

      switch (actor->mode) {
      case LGUARD_IDLE:
        if ((st->flags & LGUARD_F_PACE) &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range2) {
          ANIM_GO(actor, st->base + 1);
          actor->mode = LGUARD_PACE;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < LGUARD_LUNGE_DZ
                               : -SPY_DZ(actor) < LGUARD_LUNGE_DZ)) {
          if (st->kind == LGKIND_SMALL) {
            if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_SMALL_LUNGE_RANGE) {
              actor->mode = LGUARD_LUNGE;
              continue;
            }
          } else if (st->kind == LGKIND_BIG &&
                     ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_BIG_LUNGE_RANGE) {
            actor->mode = LGUARD_LUNGE;
            continue;
          }
        }
        break;

      case LGUARD_PACE:
        st->flags &= ~LGUARD_F_PACE;
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_PACE_STOP_RANGE ||
            PivotActorAlongPath(actor, st->path, 0x100, LGUARD_PACE_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY, one) ==
                PATH_ARRIVED(0)) {
          ANIM_GO(actor, st->base);
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_LUNGE:
        if (st->kind == LGKIND_SMALL) {
          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          g_anSpyroRequestVelocity.x = (g_anCosineLut[actor->rotZ] * LGUARD_SMALL_BUMP_SPEED) >> 10;
          g_nSpyroRequestVelocityY = (g_anSineLut[actor->rotZ] * LGUARD_SMALL_BUMP_SPEED) >> 10;
          g_nSpyroRequestVelocityZ = LGUARD_SMALL_BUMP_VZ;
          if (g_nActorAnimDone != 0) {
            st->timer = LGUARD_SMALL_LUNGE_REST;
            ANIM_GO(actor, st->base);
            actor->mode = LGUARD_IDLE;
            continue;
          }
          break;
        }
        ANIM_GO(actor, st->base + 2);
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0 || dist > LGUARD_BIG_LUNGE_LEAVE) {
          st->timer = LGUARD_BIG_LUNGE_REST;
          ANIM_GO(actor, st->base);
          actor->mode = LGUARD_IDLE;
          continue;
        }
        if (dist < LGUARD_BIG_STRIKE_RANGE && actor->anim == actor->nextAnim &&
            actor->nextFrame < 0x12) {
          actor->frame = 0x15;
          actor->nextFrame = 0x16;
          actor->frameProgress = 0;
        }
        if (actor->nextFrame == 0x15) {
          actor->frame = 7;
          actor->nextFrame = 8;
          actor->frameProgress = 0;
        }
        break;

      case LGUARD_KNOCKED: {
        int r;

        actor->rotZ += st->spin;
        if (++actor->subMode >= 3) {
          if (st->spin > 0) {
            st->spin--;
            if (st->spin < 0) {
              st->spin = 0;
            }
          } else if (st->spin < 0) {
            st->spin++;
            if (st->spin > 0) {
              st->spin = 0;
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        r = DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, LGUARD_KNOCK_DRAG, LGUARD_KNOCK_GRAVITY);
        if (st->kind == LGKIND_SMALL) {
          if (r == MOVED_LANDED && (unsigned)(actor->nextFrame - 0xB) < 9) {
            g_nActorAnimDone = 0;
            actor->frameProgress = ANIM_RATE_TICKS(2);
            actor->frameRate = ANIM_RATE_TICKS(2);
            actor->anim = actor->nextAnim;
            actor->nextAnim = actor->nextAnim;
            actor->frame = actor->nextFrame;
            actor->nextFrame = 0x14;
            OnActorAnimQueued(actor);
          }
        } else if (r == MOVED_LANDED && (unsigned)(actor->nextFrame - 0xB) < 9) {
          g_nActorAnimDone = 0;
          actor->frameProgress = ANIM_RATE_TICKS(2);
          actor->frameRate = ANIM_RATE_TICKS(2);
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 0x16;
          OnActorAnimQueued(actor);
        }
        ANIM_RETIME(actor);
        break;
      }

      case LGUARD_TO_BIG:
        TICK_DOWN(st->reloadTimer);
        actor->scale -= g_nFrameStep * 2;
        if (actor->scale <= ACTOR_SCALE_UNIT) {
          actor->scale = ACTOR_SCALE_UNIT;
          st->kind = !st->kind;
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_TO_SMALL:
        TICK_DOWN(st->reloadTimer);
        actor->scale += g_nFrameStep * 2;
        if (actor->scale >= LGUARD_TO_SMALL_SCALE) {
          actor->scale = ACTOR_SCALE_UNIT;
          BurstActorWithSound(actor, LGUARD_TO_SMALL_SPARK_LIFE);
          st->base = 4;
          ANIM_SET(actor, st->base);
          st->kind = !st->kind;
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_ESCORT: {
        Actor *partner = &g_pActorListBase[st->partner];
        int h = ArcTan2(partner->posX - actor->posX, partner->posY - actor->posY, ATAN_ANGLE8);

        SteerActorAroundPoint(actor, &partner->posX, LGUARD_ESCORT_RADIUS,
                      WrapAddAngle8(ArcTan2(g_anSpyroWorldPos - partner->posX,
                                                  g_nSpyroWorldPosY - partner->posY, ATAN_ANGLE8),
                                    0x80),
                      4, LGUARD_ESCORT_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STRAFE);
        if (partner->mode != CCANNON_LOB) {
          if (TICK_DOWN(st->timer) != 0) {
            TurnActorYawTowardAngle(partner, h, 4, 0, YAW_HOLD_IN_BAND);
          }
          break;
        }
        st->timer = LGUARD_ESCORT_TURN_DELAY;
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 104: a comet. Flies its arc leaving a fading trail and drops a
     * spark (77) now and then; it is removed once the trail has faded. */
    case ACTOR_TYPE_COMET: {
      CometState *st = actor->state;
      int s = actor->mode;

      if (s < 3) {
        int n;
        int i;

        if (s < 0) {
          goto out104;
        }
        if (s == COMET_FLY) {
          st->spark += g_nFrameStep;
          if (st->spark >= 0xC) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET_SPARK, actor);

            CopyVector(&o->posX, &actor->posX);
            o->subMode = 7;
            o->rotY = ArcTan2(st->speed, st->vz, ATAN_ANGLE8);
            o->rotZ = actor->rotZ;
            o->cullRadius = CULL_RADIUS(0x5000);
            st->spark = 0;
          }
          do {
            actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
            actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
            actor->posZ += st->vz;
            st->vz += st->dvz;
            if (st->steps != 0) {
              st->steps--;
            }
            if (st->lifeOn != 0) {
              if (st->life == 0) {
                actor->mode = COMET_HIT;
              }
              st->life--;
            }
          } while (st->steps != 0);
          if (TICK_DOWN(st->timer) != 0) {
            actor->mode = COMET_HIT;
          }
          if (actor->posX < 0 || actor->posY < 0 || actor->posZ < 0) {
            actor->cullRadius = ACTOR_CULL_HIDDEN;
            actor->drawn = 0;
            actor->mode = COMET_HIT;
            continue;
          }
          DispatchActorContactAtSphere(&actor->posX, 0x258, 0, ACTOR_HIT_BLAST, actor, CONTACT_ALL);
          if (CollideSphereWithWorldAndActors(&actor->posX, 0x168, 0x168) != 0) {
            actor->mode = COMET_HIT;
          }
          if (actor->mode == COMET_HIT) {
            actor->cullRadius = ACTOR_CULL_HIDDEN;
            actor->drawn = 0;
          }
        }
        n = 0;
        if (actor->mode < COMET_FADE && g_nMotionTrailRibbonCount < 3 && st->count < 0x78) {
          int k = st->count;

          CopyVector(st->trail[k].pos, &actor->posX);
          st->trail[k].age = 0;
          st->count++;
        }
        for (i = 0; i < st->count; i++) {
          st->trail[i].age += g_nFrameStep;
          if (st->trail[i].age < 0x51) {
            n++;
          }
        }
        if (n == 0 && actor->mode != COMET_FLY) {
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nMotionTrailRibbonCount < 3) {
          int k = g_nMotionTrailRibbonCount;

          g_anMotionTrailSliceCounts[k] = st->count;
          g_nMotionTrailRibbonCount = k + 1;
          g_apMotionTrailSlices[k] = st->trail;
        }
        if (actor->mode == COMET_HIT) {
          actor->mode = COMET_FADE;
        }
      }
    out104:
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Explosion shard with a ground bounce (TumbleState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1:
    case ACTOR_TYPE_LIFE_CHEST_SHARD:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 1: {
      TumbleState *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {

      if (CollideSphereWithWorldAndActors(&actor->posX, DEBRIS_RADIUS, DEBRIS_RADIUS) != 0) {
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
        g056[0] = GetRandomU32() & 3;
        g056[1] = GetRandomU32() & 3;
        g056[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)g056);
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
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 2:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 2: {
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
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);

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
          CopyVector(g136, &actor->posX);
          g136[2] += GROUND_PROBE_LIFT;
          FindGroundHeightBelow(g136, GROUND_PROBE_FAR);
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

                st->lean =
                    -ArcTan2_8bit(IntegerSqrt((nrm[0] * nrm[0]) +
                                                 (g_nCollisionHitNormalZ * g_nCollisionHitNormalZ)),
                                   g_nCollisionHitNormalY);
                st->roll = -ArcTan2_8bit(g_nCollisionHitNormalZ, nrm[0]);
              }
            }
          } else if (st->mode == COLLREST_ON_TRI) {
            UnpackWorldCollisionTri(st->groundTri, triVerts[0]);
            AddVector(triVerts[0], triVerts[0], triVerts[1]);
            AddVector(triVerts[0], triVerts[0], triVerts[2]);
            actor->posX = triVerts[0][0] / 3;
            actor->posY = triVerts[0][1] / 3;
            actor->posZ = triVerts[0][2] / 3;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeHitNormalToActorGroundCode(actor);
          } else if (st->mode == COLLREST_FIND_TRI) {
            CopyVector(g152, &actor->posX);
            g152[2] += GROUND_PROBE_LIFT;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(g152, 0x1000) > 0) {
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
                  if (GetSoundVoiceStatusByOwner(
                          g_pSparxActor,
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) == 0) {
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
              int slope =
                  (signed char)ArcTan2_8bit(bounceNormal[2], VectorLength(bounceNormal, VEC_LEN_XY));

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
            bouncePos[2] -= COLLECT_RADIUS;
            CopyVector(at, bouncePos);
            FindGroundHeightBelow(bouncePos, GROUND_PROBE_FAR);
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
                  if (GetSoundVoiceStatusByOwner(
                          g_pSparxActor,
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) == 0) {
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
              actor->posZ += (short)(g_anSineLut[st->timer * 4] / 12);
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

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 25) +
                  (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
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

    /* Save-prompt host (SaveHostState). */
    case ACTOR_TYPE_SAVE_HOST: {
      SaveHostState *st = (SaveHostState *)actor->state;

      Actor *armer = &g_pActorListBase[st->armer];
      Actor *anchor = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY: {
        actor->collision = 0;
        if (ApproxDist2D(&anchor->posX, &SPY.posX) <= SAVEHOST_REARM_RANGE) {
          break;
        }
        actor->mode = SAVEHOST_WAIT_ARMED;
        continue;
      }

      case SAVEHOST_WAIT_ARMED: {
        if (armer->mode >= (unsigned)ACTOR_MODE_DORMANT) {
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = SAVEHOST_APPEAR_DELAY;
          st->baseZ = actor->posZ;
        }
        break;
      }

      case SAVEHOST_HIDDEN: {
        if (st->timer != 0) {
          st->timer--;
        } else {
          actor->collision = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < SAVEHOST_APPEAR_RANGE) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < SAVEHOST_APPEAR_DZ) {
              if (st->prompted == 0 || anchor->mode == CHECKPT_ARMED) {
                st->prompted = 0;
                actor->posZ = st->baseZ + SAVEHOST_POP_RAISE;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, ATAN_ANGLE8) -
                               128;
                actor->cullRadius = SAVEHOST_CULL_RADIUS;
                actor->mode = SAVEHOST_POP_IN;
                actor->scale = SAVEHOST_POP_SCALE;

                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, 0xC);

                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(&actor->posX, SAVEHOST_PROBE_DEPTH);
                EncodeHitNormalToActorGroundCode(actor);
                actor->groundCode |= ACTOR_GROUND_VALID;
              }
            }
          }
        }
        break;
      }

      case SAVEHOST_POP_IN: {
        if (actor->scale > ACTOR_SCALE_UNIT) {
          actor->scale -= SAVEHOST_SCALE_STEP;
          actor->rotZ = actor->rotZ + SAVEHOST_SPIN_STEP;
        } else {
          actor->mode = SAVEHOST_HOVER;
          st->timer = 0;
          st->wanderTimer = SAVEHOST_WANDER_DELAY;
        }
        break;
      }

      case SAVEHOST_HOVER: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if (CHECKPT_SETTLED(anchor) &&
            ApproxDist2D(&anchor->posX, &SPY.posX) < SAVEHOST_PROMPT_RANGE &&
            ABS2(anchor->posZ - anchor->floorOffset - SPY.posZ) < SAVEHOST_PROMPT_DZ) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > SAVEHOST_HOVER_LEASH ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > SAVEHOST_LEASH_DZ)) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          st->wanderTimer--;
          if (st->wanderTimer == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + SAVEHOST_WANDER_DIST;
            char finalAngle = (ArcTan2(SPY.posX - anchor->posX,
                                             SPY.posY - anchor->posY, ATAN_ANGLE8) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->wanderTo, &anchor->posX);

            actor->mode = SAVEHOST_WANDER;

            st->wanderTo[0] -= (LookupCosine(finalAngle * 0x10) * baseDistance) >> 12;
            st->wanderTo[1] -= (LookupSine(finalAngle * 0x10) * baseDistance) >> 12;

            st->faceYaw = ArcTan2(SPY.posX - st->wanderTo[0],
                                      SPY.posY - st->wanderTo[1], ATAN_ANGLE8);
            st->wanderTimer = 0;
          }
        }
        break;
      }
      case SAVEHOST_WANDER: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if (CHECKPT_SETTLED(anchor) &&
            ApproxDist2D(&anchor->posX, &SPY.posX) < SAVEHOST_PROMPT_RANGE &&
            ABS2(anchor->posZ - anchor->floorOffset - SPY.posZ) < SAVEHOST_PROMPT_DZ) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > SAVEHOST_WANDER_LEASH ||
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > SAVEHOST_LEASH_DZ) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->wanderTimer++;

          SubtractVector(g136, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (g136[0] * cosDelta) / totalWeight;
          actor->posY += ((g136[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, SAVEHOST_PROBE_DEPTH);
          EncodeHitNormalToActorGroundCode(actor);

          if (st->wanderTimer == SAVEHOST_WANDER_FRAMES) {
            actor->mode = SAVEHOST_HOVER;
            st->wanderTimer = (GetRandomU32() & 0x1F) + SAVEHOST_WANDER_DELAY;
          }
        }

        break;
      }

      case SAVEHOST_POP_OUT: {
        if (actor->scale < SAVEHOST_POP_SCALE) {
          actor->scale += SAVEHOST_SCALE_STEP;
          actor->rotZ = actor->rotZ + SAVEHOST_SPIN_STEP;
        } else {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->drawn = 0;
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = SAVEHOST_APPEAR_DELAY;

          g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, 0xC);

          actor->groundCode &= ~ACTOR_GROUND_VALID;
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

      if (g_anSparxHealthBlock[0] >= SPARX_BLUE) {
        g136[0] = 0;
        g136[1] = SPARX_WING_SPAN;
        g136[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g136, 0);
        g136[0] = 0;
        g136[1] = -SPARX_WING_SPAN;
        g136[2] = 0;
        ApplyActiveGteRotation(g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g136, 0);
      }

      if (g_anSparxHealthBlock[0] >= SPARX_GOLD) {
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
            st->glow->r = SPARX_GLOW_R;
            st->glow->g = SPARX_GLOW_G;
            st->glow->b = SPARX_GLOW_B;
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

      if (g_anSparxHealthBlock[0] <= SPARX_GONE) {
        if (st->target != 0 && st->target->type == ACTOR_TYPE_BUTTERFLY) {
          st->target->mode = BUTTERFLY_FLUTTER;
        }
        g_pSparxActor = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->target != 0 &&
          ApproxDist2D(&actor->posX, (int *)((char *)SPY_VIA(g_anSparxHealthBlock, SPY_SPARX_HEALTH, SPY_POS))) >
              SPARX_TARGET_LEASH) {
        st->timer = 0;
      }

      if (actor->subMode != SPARX_SUB_PARKED) {
        if (st->target != 0) {
          switch (actor->subMode) {
          case SPARX_SUB_CHASE: {
            /* Closing in on the butterfly. */
            ButterflyState *bp = st->target->state;
            int high = actor->nextAnim & 1;
            int dist;
            int reach;
            int limit;

            CopyVector(g152, &st->target->posX);

            if (high == 0) {
              g152[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              g152[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              g152[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              g152[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 22;
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

            SubtractVector(g152, g152, &actor->posX);
            dist = VectorLength(g152, VEC_LEN_XYZ);

            if (high == 0 &&
                (TICK_DOWN(st->timer) != 0 ||
                 (st->timer < SPARX_CATCH_EARLY_FRAMES &&
                  AbsAngleDelta8(
                      (actor->rotZ + (bp->circleDir * SPARX_CATCH_LEAD)) & 0xFF,
                      (((unsigned short)g_nCameraEulerYaw << 16 >> 20) + 0x80) &
                          0xFF) < 8))) {
              if (actor->nextAnim != actor->nextAnim + 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim + 1);
              }
            }

            reach = high != 0 ? SPARX_CHASE_STEP_HIGH : SPARX_CHASE_STEP;
            limit = reach + SPARX_CHASE_STEP_SLACK;
            if (limit < dist) {
              ScaleVector3ByRatio(g152, dist, limit);
            } else {
              ScaleVector3ByRatio(g152, dist, reach - SPARX_CHASE_STEP_SLACK);
            }
            AddVector(&actor->posX, &actor->posX, g152);
            TurnActorYawTowardAngle(actor, ArcTan2(g152[0], g152[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(g152, VEC_LEN_XY), g152[2], ATAN_ANGLE8);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case SPARX_SUB_EAT:
            g_anSparxHealthBlock[0] += 1;
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
          actor->cullRadius = CULL_RADIUS(0x1000);

          if (st->timer <= 0) {
            g136[0] = -SPARX_HOVER_BEHIND;
            g136[1] = (GetRandomU32() & 0x310) - 0x188;
            g136[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + SPARX_CHARGE_HOVER_MIN_Z
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = g136[0];
            st->drift[1] = g136[1];
            st->drift[2] = g136[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              g136[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
              if (g136[2] < SPARX_CHARGE_HOVER_MIN_Z) {
                g136[2] = SPARX_CHARGE_HOVER_MIN_Z;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                g136[0] = st->drift[0] - SPARX_FREE_LOOK_PULLBACK;
              } else {
                g136[0] = st->drift[0];
              }
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, g136, g136);
            AddVector(g136, g136, &SPY.posX);
            at = &actor->posX;
            SubtractVector(g136, g136, at);
            RShiftVector3(g136, 2);
            AddVector(at, at, g136);

            if (CollideSphereWithWorldAndActors(at, SPARX_COLLIDE_RADIUS, SPARX_COLLIDE_RADIUS) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (g136[2] > SPARX_MAX_PITCH) {
              g136[2] = SPARX_MAX_PITCH;
            }
            if (g136[2] < -SPARX_MAX_PITCH) {
              g136[2] = -SPARX_MAX_PITCH;
            }

            actor->rotX = 0;
            actor->rotY = g136[2];
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

    /* Type 126: a path walker. Turns to face Spyro when he is near, walks
     * its path, sings its tune while it rests, and can be carried and
     * thrown by its rider. */
    case ACTOR_TYPE_SINGING_PATH_WALKER: {
      PathWalkerState *st = actor->state;

      if (st->init == 0) {
        st->init = one;
        st->path->cur = st->startCur;
        if (st->mode == SINGER_ROLE_ABSENT) {
          DespawnActorRecord(actor);
          continue;
        }
        if (st->mode != SINGER_ROLE_PLAIN) {
          ENTER_POSE(actor, SINGER_REST);
        }
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) &&
          (actor->mode < SINGER_STRUCK || actor->mode == SINGER_RETURN)) {
        actor->flags = 0;
        if (st->mode == SINGER_ROLE_CARRIED) {
          ((int *)g_pActorListBase[st->rider].state)[18] = one;
          st->path->count -= SINGER_DROPPED_PATH_TRIM;
          st->mode = one;
        }
        actor->mode = SINGER_STRUCK;
        ANIM_RESET(actor, SINGER_STRUCK);
        continue;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      FindGroundHeightBelow(&actor->posX, GROUND_PROBE_FAR);
      EncodeHitNormalToActorGroundCode(actor);

      switch (actor->mode) {
      case SINGER_IDLE:
        if (st->mode == SINGER_ROLE_CARRIED) {
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < SINGER_WALK_RANGE) {
          st->next = SINGER_WALK;
          st->legStep[0] = 0xFF;
          ENTER_POSE(actor, SINGER_TURN);
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < SINGER_WATCH_RANGE) {
          st->next = SINGER_WATCH;
          ENTER_POSE(actor, SINGER_TURN);
        }
        continue;

      case SINGER_TURN:
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, st->next);
        }
        continue;

      case SINGER_REST:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (st->mode != SINGER_ROLE_CARRIED) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < SINGER_WALK_RANGE) {
            ENTER_POSE(actor, SINGER_WALK);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < SINGER_WATCH_RANGE &&
              g_nActorAnimDone != 0 && (GetRandomU32() & 4)) {
            ANIM_QUEUE(actor, SINGER_WATCH);
          }
          continue;
        }
        if (st->rideMode == one) {
          GetActorAttachPoint(&g_pActorListBase[st->rider], 1, gripA);
          GetActorAttachPoint(&g_pActorListBase[st->rider], 2, gripB);
          actor->posX = gripA[0] + ((gripB[0] - gripA[0]) >> 1);
          actor->posY = gripA[1] + ((gripB[1] - gripA[1]) >> 1);
          actor->posZ = gripA[2] + ((gripB[2] - gripA[2]) >> 1);
          actor->rotZ =
              ArcTan2(actor->posX - g_pActorListBase[st->rider].posX,
                            actor->posY - g_pActorListBase[st->rider].posY, ATAN_ANGLE8);
          continue;
        }
        if (st->rideMode == 2) {
          actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
          actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
          CopyVector(g368, &actor->posX);
          if (CollideSphereWithWorldAndActors(g368, SINGER_THROWN_RADIUS, SINGER_THROWN_RADIUS) != 0) {
            actor->mode = SINGER_LANDED;
            ANIM_RESET(actor, SINGER_LANDED);
            continue;
          }
          actor->posZ += st->vz;
          st->vz += st->dvz;
          ANIM_GO(actor, SINGER_ANIM_THROWN);
        }
        continue;

      case SINGER_WALK:
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < SINGER_WALK_RANGE) {
          if (TICK_DOWN(st->call) != 0) {
            st->call = (int)GetRandomU32() % 32 + SINGER_CALL_MIN;
            PlayActorMeshVoice(actor, (int)GetRandomU32() % 3 + 2, 0);
          }
          TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, sixteen, 0);
          continue;
        }
        if (TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, SINGER_FAST_TWEEN, 0) != 0) {
          ENTER_POSE(actor, SINGER_REST);
        }
        continue;

      case SINGER_WATCH:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < SINGER_WALK_RANGE) {
          ENTER_POSE(actor, SINGER_WALK);
        }
        if (g_nActorAnimDone != 0 && (GetRandomU32() & 4)) {
          ENTER_POSE(actor, SINGER_REST);
        }
        continue;

      case SINGER_LANDED:
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, SINGER_WALK);
          actor->mode = SINGER_RETURN;
        }
        continue;

      case SINGER_STRUCK:
        if ((unsigned)(FindGroundHeightBelow(&actor->posX, GROUND_PROBE_FAR) - 0x81) < 0x177F) {
          actor->posZ -= SINGER_STRUCK_SINK;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
        if (g_nActorAnimDone != 0) {
          st->timer = st->period;
          switch (st->mode) {
          case SINGER_ROLE_PLAIN:
          case SINGER_ROLE_DROPPED:
          case SINGER_ROLE_CARRIED:
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            break;
          case SINGER_ROLE_PAIRED:
            if (g_pActorListBase[st->partner].mode >= 6 &&
                ((PathWalkerState *)g_pActorListBase[st->partner].state)->resumePending == 0) {
              st->resumePending = one;
            } else {
              st->timer = SINGER_PAIRED_SING_TICKS;
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            }
            break;
          case SINGER_ROLE_TWO_GATE:
            if (CHUNK_ANIM_STEP(GetWorldChunkAnimState(SINGER_CHUNK_GATE_A)) > 0 ||
                !(GetWorldChunkAnimState(SINGER_CHUNK_GATE_A) & CHUNK_ANIM_HALTED)) {
              st->timer = SINGER_PAIRED_SING_TICKS;
              ModifyWorldChunkAnimFlags(SINGER_CHUNK_GATE_B, CHUNK_ANIM_RESUME, 0);
              st->chunkAnim = SINGER_CHUNK_GATE_B;
            } else {
              st->timer = SINGER_PAIRED_SING_TICKS;
              ModifyWorldChunkAnimFlags(SINGER_CHUNK_GATE_A, CHUNK_ANIM_RESUME, 0);
              st->chunkAnim = one;
            }
            break;
          }
          PlayActorMeshVoice(actor, SINGER_VOICE_SING, 0);
          ANIM_QUEUE(actor, SINGER_SING);
        }
        continue;

      case SINGER_SING:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
        if (st->next == SINGER_CUE_STOP) {
          ENTER_POSE(actor, SINGER_SETTLE);
        }
        if (st->resumePending != 0 && (GetWorldChunkAnimState(st->chunkAnim) & CHUNK_ANIM_HALTED)) {
          st->resumePending = 0;
          st->timer = SINGER_RESUME_SING_TICKS;
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = SINGER_SING_END_TICKS;
          PlayActorMeshVoice(actor, SINGER_VOICE_SING_END, 0);
          ENTER_POSE(actor, SINGER_SING_END);
        }
        continue;

      case SINGER_SING_END:
        if (st->next == SINGER_CUE_STOP) {
          ENTER_POSE(actor, SINGER_SETTLE);
        }
        switch (st->mode) {
        case SINGER_ROLE_PLAIN:
        case SINGER_ROLE_DROPPED:
        case SINGER_ROLE_CARRIED:
        case SINGER_ROLE_TWO_GATE:
          if (TICK_DOWN(st->timer) != 0) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            ENTER_POSE(actor, SINGER_SETTLE);
          }
          break;
        case SINGER_ROLE_PAIRED:
          if (TICK_DOWN(st->timer) != 0) {
            int s;

            if (g_pActorListBase[st->partner].mode >= 6) {
              ((PathWalkerState *)g_pActorListBase[st->partner].state)->next = SINGER_CUE_STOP;
            }
            s = GetWorldChunkAnimState(st->chunkAnim);
            if ((s & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(s) < SINGER_GATE_REWIND_BELOW) {
              SetWorldChunkAnimStep(st->chunkAnim, SINGER_GATE_REWIND_STEP, 0);
            }
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            ENTER_POSE(actor, SINGER_SETTLE);
          }
          break;
        }
        continue;

      case SINGER_SETTLE:
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        if (g_nActorAnimDone != 0) {
          st->next = SINGER_REST;
          actor->mode = SINGER_REST;
          actor->flags = 0;
          ANIM_RESET(actor, SINGER_REST);
          continue;
        }
        continue;

      case SINGER_RETURN:
        if (TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, sixteen, TWEEN_FLY) == TWEEN_WRAPPED) {
          st->rideMode = 0;
          actor->rotY = 0;
          ((int *)g_pActorListBase[st->rider].state)[16] = one;
          ENTER_POSE(actor, SINGER_REST);
        }
        continue;
      }
      continue;
    }

    /* Type 130: the big guard (as type 92, plus two followers it wakes and
     * a growl while it lunges). */
    case ACTOR_TYPE_BIG_LUNGING_GUARD: {
      BigLungingGuardState *st = actor->state;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode != LGUARD_KNOCKED &&
          (st->kind != LGKIND_BIG || !(actor->flags & ACTOR_HIT_CHARGE))) {
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        st->knockZSpeed = LGUARD_BLG_KNOCK_VZ;
        st->knockSpeed = LGUARD_BLG_KNOCK_SPEED;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knockSpeed = LGUARD_BLG_CHARGE_KNOCK_SPEED;
        }
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8),
                                    g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        actor->flags = 0;
        actor->probeDist = ACTOR_PROBE_NONE;
        st->spin = RandomSignedInRange(LGUARD_SPIN_MIN, LGUARD_SPIN_MAX);
        actor->subMode = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ANIM_GO(actor, st->base + 3);
        actor->mode = LGUARD_KNOCKED;
        continue;
      }
      if (st->init == 0) {
        st->init = one;
        if (st->flip != 0 && actor->dropKind == DROP_GEM_ROLL) {
          st->kind = !st->kind;
        }
        if (st->kind == LGKIND_SMALL) {
          st->base = 4;
          ANIM_SET(actor, st->base);
        }
      }
      if (actor->mode < LGUARD_KNOCKED && (st->range != 0 || (actor->flags & ACTOR_HIT_BLAST))) {
        if (st->range > 0 && (actor->mode == LGUARD_IDLE || actor->mode == LGUARD_LUNGE) &&
            (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range ||
             IsPointInRotatedBox(&spy->posX, &st->vol) != 0) &&
            TICK_DOWN(st->reloadTimer) != 0) {
          st->range = -st->range;
          if (st->range == 0) {
            st->range = -1;
          }
          g_pActorListBase[st->partner].subMode = actor - g_pActorListBase;
          /* CometCannonState.steps, as a plain word store: the typed view
           * reschedules the whole function. */
          *(int *)g_pActorListBase[st->partner].state = st->cometSteps;
          st->reloadTimer = st->reload;
          if (st->follow1 != -1) {
            ((BigLungingGuardState *)g_pActorListBase[st->follow1].state)->reloadTimer = LGUARD_FOLLOW1_RELOAD;
          }
          if (st->follow2 != -1) {
            ((BigLungingGuardState *)g_pActorListBase[st->follow2].state)->reloadTimer = LGUARD_FOLLOW2_RELOAD;
          }
        } else if (actor->flags & ACTOR_HIT_BLAST) {
          actor->flags = 0;
          if (st->flags & LGUARD_F_REARM) {
            st->range = -st->range;
          } else {
            st->range = 0;
          }
          if (st->kind == LGKIND_SMALL) {
            st->kind = !st->kind;
            BurstActorWithSound(actor, LGUARD_TO_BIG_SPARK_LIFE);
            actor->scale = LGUARD_TO_BIG_SCALE;
            st->base = 0;
            ANIM_SET(actor, st->base);
            actor->mode = LGUARD_TO_BIG;
            continue;
          }
          st->kind = !st->kind;
          actor->scale = ACTOR_SCALE_UNIT;
          actor->mode = LGUARD_TO_SMALL;
          continue;
        } else if (st->range < 0) {
          TICK_DOWN(st->reloadTimer);
        }
      }
      actor->flags = 0;

      switch (actor->mode) {
      case LGUARD_IDLE:
        if ((st->flags & LGUARD_F_PACE) &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range2) {
          ANIM_GO(actor, st->base + 1);
          actor->mode = LGUARD_PACE;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < LGUARD_LUNGE_DZ
                               : -SPY_DZ(actor) < LGUARD_LUNGE_DZ)) {
          if (st->kind == LGKIND_SMALL) {
            if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_BLG_SMALL_LUNGE_RANGE) {
              actor->mode = LGUARD_LUNGE;
              continue;
            }
          } else if (st->kind == LGKIND_BIG &&
                     ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_BIG_LUNGE_RANGE) {
            actor->mode = LGUARD_LUNGE;
            continue;
          }
        }
        break;

      case LGUARD_PACE:
        st->flags &= ~LGUARD_F_PACE;
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < LGUARD_PACE_STOP_RANGE ||
            PivotActorAlongPath(actor, st->path, 0x100, LGUARD_PACE_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
                PATH_ARRIVED(0)) {
          ANIM_GO(actor, st->base);
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_LUNGE:
        if (st->kind == LGKIND_SMALL) {
          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          g_anSpyroRequestVelocity.x = (g_anCosineLut[actor->rotZ] * LGUARD_SMALL_BUMP_SPEED) >> 10;
          g_nSpyroRequestVelocityY = (g_anSineLut[actor->rotZ] * LGUARD_SMALL_BUMP_SPEED) >> 10;
          g_nSpyroRequestVelocityZ = LGUARD_SMALL_BUMP_VZ;
          if (g_nActorAnimDone != 0) {
            st->timer = LGUARD_SMALL_LUNGE_REST;
            ANIM_GO(actor, st->base);
            actor->mode = LGUARD_IDLE;
            continue;
          }
          break;
        } else {
          int dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);

          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          if (g_nActorAnimDone != 0 || dist > LGUARD_BLG_LUNGE_LEAVE) {
            st->timer = LGUARD_BIG_LUNGE_REST;
            ANIM_GO(actor, st->base);
            actor->mode = LGUARD_IDLE;
            continue;
          }
          if (dist < LGUARD_BLG_STRIKE_RANGE && actor->anim == actor->nextAnim &&
              actor->nextFrame < 0xE) {
            actor->frame = 0x10;
            actor->nextFrame = 0x11;
            actor->frameProgress = 0;
          }
          if (actor->nextFrame == 0xF) {
            actor->frame = 5;
            actor->nextFrame = 6;
            actor->frameProgress = 0;
          }
          if (actor->nextFrame == 6 &&
              GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[LGUARD_VOICE_GROWL]) == 0) {
            PlayActorMeshVoice(actor, LGUARD_VOICE_GROWL, &st->sound);
          }
          break;
        }

      case LGUARD_KNOCKED: {
        int r;

        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        actor->rotZ += st->spin;
        if (++actor->subMode >= 3) {
          if (st->spin > 0) {
            st->spin--;
            if (st->spin < 0) {
              st->spin = 0;
            }
          } else if (st->spin < 0) {
            st->spin++;
            if (st->spin > 0) {
              st->spin = 0;
            }
          }
        }
        r = DriveActorTrackedMove(actor, &st->knockSpeed, st->heading, &st->knockZSpeed, LGUARD_KNOCK_DRAG, LGUARD_KNOCK_GRAVITY);
        if (r == MOVED_LANDED && actor->nextFrame < 0xA) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 0xA;
          OnActorAnimQueued(actor);
        }
        if (r == 0 && actor->nextFrame == 9) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 5;
          OnActorAnimQueued(actor);
        }
        ANIM_RETIME(actor);
        break;
      }

      case LGUARD_TO_BIG:
        TICK_DOWN(st->reloadTimer);
        actor->scale -= g_nFrameStep * 2;
        if (actor->scale <= ACTOR_SCALE_UNIT) {
          actor->scale = ACTOR_SCALE_UNIT;
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_TO_SMALL:
        TICK_DOWN(st->reloadTimer);
        actor->scale += g_nFrameStep * 2;
        if (actor->scale >= LGUARD_TO_SMALL_SCALE) {
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
          actor->scale = ACTOR_SCALE_UNIT;
          BurstActorWithSound(actor, LGUARD_TO_SMALL_SPARK_LIFE);
          st->base = 4;
          ANIM_SET(actor, st->base);
          actor->mode = LGUARD_IDLE;
          continue;
        }
        break;

      case LGUARD_ESCORT: {
        Actor *p;
        int h;

        p = &g_pActorListBase[st->partner];
        ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
        h = ArcTan2(p->posX - actor->posX, p->posY - actor->posY, ATAN_ANGLE8);
        SteerActorAroundPoint(actor, &p->posX, LGUARD_ESCORT_RADIUS,
                      WrapAddAngle8(ArcTan2(g_anSpyroWorldPos - p->posX,
                                                  g_nSpyroWorldPosY - p->posY, ATAN_ANGLE8),
                                    0x80),
                      4, LGUARD_ESCORT_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STRAFE);
        TurnActorYawTowardAngle(p, h, 4, 0, YAW_HOLD_IN_BAND);
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
      st->vel[2] -= DEBRIS_FALL_GRAVITY;
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];
      if (!(actor->subMode & 1)) {
        CopyVector(sparkPosVel, &actor->posX);
        sparkPosVel[3] = GetRandomU32() & 3;
        sparkPosVel[4] = GetRandomU32() & 3;
        sparkPosVel[5] = 0x14;
        g368[0] = 0x80;
        g368[1] = 0x70;
        g368[2] = 0x40;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_RGB, sparkPosVel, (int)g368);
      }
      break;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case ACTOR_TYPE_WANDERING_CRITTER: {
      CritterKnockState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != CRITTER_KNOCKED) {
        actor->flags = 0;
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        {
          if (actor->flags & ACTOR_HIT_FLAME) {
            st->knockSpeed = CRITTER_FLAME_KNOCK_SPEED;
          } else {
            st->knockSpeed = CRITTER_KNOCK_SPEED;
          }
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        actor->mode = CRITTER_KNOCKED;
        ANIM_RESET(actor, CRITTER_KNOCKED);
        continue;
      }

      switch (actor->mode) {
      case CRITTER_WANDER:
        UpdateActorWander(actor, &st->wander);
        break;

      case CRITTER_KNOCKED:
        if (st->knockSpeed >= CRITTER_KNOCK_MIN_SPEED) {
          st->knockSpeed -= CRITTER_KNOCK_DRAG;
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, 0, CRITTER_MOVE_RADIUS, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Key (KeyState): rocks in place and sparkles; touching it picks it up. */
    case ACTOR_TYPE_KEY: {
      KeyState *st = actor->state;

      if (actor->subMode == KEY_WAITING) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &spy->posX) < KEY_TOUCH_RANGE) {
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
            actor->scale = ACTOR_SCALE_HALF;
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

    /* Type 191: a talking NPC. Starts a conversation when Spyro walks up
     * facing it; the line depends on how far through the level he is. */
    case ACTOR_TYPE_HOME_TALKER + 4: {
      if (actor->mode == TALKER_REARM) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= TALKER_REARM_RANGE) {
          actor->mode = TALKER_READY;
        }
      } else if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
                  SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_CHARGE) &&
                 ApproxDist2D(&actor->posX, &spy->posX) < TALKER_TALK_RANGE &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - spy->posX,
                                             actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8)) <
                     TALKER_SPYRO_FACING_ANGLE &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(spy->posX - actor->posX,
                                             g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8)) <
                     TALKER_FACING_ANGLE) {
        actor->mode = TALKER_REARM;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ResetSpyroLinearMotion();
        ZeroVector(&spy->headLook);
        g_pfnLevelBeginActorCutscene(actor);
        g_abWorldBalloonistState[1] = BALLOONIST_OPEN;
        g_abWorldBalloonistState[2] = BALLOONIST_OPEN;
        g_abWorldBalloonistState[3] = BALLOONIST_OPEN;
        g_abWorldBalloonistState[4] = BALLOONIST_OPEN;

        if (g_nTotalTreasureCollected < 0x1770) {
          if (g_abWorldBalloonistState[5] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(4, BPAGE_LOCKED_FIRST);
            g_abWorldBalloonistState[5] = BALLOONIST_TALKED;
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(4, BPAGE_LOCKED_AGAIN);
          }
        } else if (g_abWorldBalloonistState[5] < BALLOONIST_OPEN) {
          if (g_abWorldBalloonistState[5] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(4, BPAGE_UNLOCK_FIRST);
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(4, BPAGE_UNLOCK_AGAIN);
          }
          g_abWorldBalloonistState[5] = BALLOONIST_OPEN;
        } else {
          g_nActorCutsceneDialogPage = BPAGE_ROSTER;
          g_nActorCutsceneDialogVariant = (int)GetRandomU32() % 3;
        }
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

      actor->soundRange = SOUND_RANGE(0x8000);
      PlayActorMeshVoice(actor, WOODCHEST_VOICE_BREAK, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < WOODCHEST_SHARD_PAIRS; i++) {
        if (ACTOR_POOL_LOW()) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 1, actor);
      }
      for (i = 0; i < WOODCHEST_CHIPS; i++) {
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

        if (phase < SHAKE_TICKS) {
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

        actor->soundRange = SOUND_RANGE(0x8000);
        PlayActorMeshVoice(actor, WOBBLECHEST_VOICE_BREAK, 0);

        for (i = 0; i < WOBBLECHEST_SHARD_PAIRS; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }

        for (i = 0; i < WOBBLECHEST_CHIPS; i++) {
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

    /* Crystal dragon statue (DragonStatueState): shakes, then counts the rescue when Spyro is close. */
    case ACTOR_TYPE_DRAGON_STATUE: {
      DragonStatueState *st = actor->state;
      int *at;

      if (actor->mode == STATUE_INIT) {
        if (st->partner != -1) {
          g_pActorListBase[st->partner].mode = CHECKPT_HELD;
        }
        actor->mode = STATUE_IDLE;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->mode == STATUE_IDLE) {
        st->phase += g_nFrameStep;

        if (st->phase > STATUE_CYCLE_TICKS) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if (st->phase >= STATUE_SHAKE_TICK) {
          int dx;
          int dy;

          if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[STATUE_VOICE_RATTLE]) == 0) {
            PlayActorMeshVoice(actor, STATUE_VOICE_RATTLE, 0);
          }

          actor->rotX = g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else if (st->phase >= STATUE_SETTLE_TICK) {
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = STATUE_SETTLE_TICK;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if ((st->phase >> 1) == 0x10 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
        } else if ((st->phase >> 1) == 0x30 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
        } else if ((st->phase >> 1) == 0x50 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
        }

        at = &actor->posX;
        if (ApproxDist2D(at, &spy->posX) < STATUE_NEAR_RANGE) {
          SubtractVector(g408, at, &spy->posX);
          g408[2] = (g408[2] * 3) >> 2;

          if (VectorLength(g408, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->facing;
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, facing);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIndex] += 1;
              g_nTotalDragonsRescued += 1;
              StubRewardHook(st->reward, 0);

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = CHECKPT_ARMED;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->cutsceneId == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIndex] += 1;
              g_nTotalDragonsRescued += 1;

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = CHECKPT_ARMED;
              }
              DespawnActorRecord(actor);
              continue;
            }

            actor->mode = STATUE_RESCUING;
            actor->rotX = st->rotX;
            actor->rotY = st->rotY;
            actor->posZ = st->restZ;
            BeginDragonRescue(actor);
            ZeroVector(&SPY.headLook);
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT);

            if (g_nSpyroAirborneFrames != 0) {
              g_nSpyroRequestState = SPYRO_ST_FALL;
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

              g_nSpyroRequestState = SPYRO_ST_STOP;
              CopyVector(look, &SPY.motionX);
              RShiftVector3(look, 6);
              g_nSpyroRequestVelocityZ = 0;

              if (SPY.requestVelX != 0 || g_nSpyroRequestVelocityY != 0) {
                ScaleVector3ByRatio(look, VectorLength(look, VEC_LEN_XY), 0x60);
                goto clear_c68;
              }
            }
          }
        }

      } else {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
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

          if (actor->posZ > DEBRIS_FLOOR_Z) {
            if (CollideSphereWithWorldAndActors(&actor->posX, DEBRIS_RADIUS, DEBRIS_RADIUS)) {
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
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case AMBIENT_MODE_PATH:
        switch (st->sub) {
        case AMBIENT_STEP_WAIT:
          if (TICK_DOWN(st->timer) != 0) {
            st->timer = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->sub = AMBIENT_STEP_TRAVEL;
          }
          break;

        case AMBIENT_STEP_TRAVEL: {
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
            st->sub = AMBIENT_STEP_WAIT;
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
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TICK_DOWN(st->timer) != 0) {
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

      case AMBIENT_MODE_PORTAL:
        voice = SFX_PORTAL_HUM;
        break;

      case AMBIENT_MODE_WHIRLWIND:
        voice = SFX_WHIRLWIND;
        break;
      }

      if (voice >= 0) {
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, SFX_MODE_3D, &actor->voiceRow);
      }

      break;
    }

    /* Whirlwind (WhirlwindState): lifts Spyro to its top (SPYRO_EV_LIFT). */
    case ACTOR_TYPE_WHIRLWIND: {
      WhirlwindState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (WhirlwindState *)actor->state;
      if (st->flags & WHIRL_F_GATED) {
        if (g_pActorListBase[st->idx].type == ACTOR_TYPE_KEY) {
          if (g_pActorListBase[st->idx].subMode == KEY_WAITING)
            break;
          if (ApproxDist2D(&actor->posX, &SPY.posX) > WHIRL_KEY_RANGE)
            break;
        } else if (g_pActorListBase[st->idx].type == ACTOR_TYPE_DRAGON_STATUE ||
                   g_pActorListBase[st->idx].type == ACTOR_TYPE_GEM_1) {
          if (g_pActorListBase[st->idx].mode < ACTOR_MODE_DORMANT)
            break;
        } else {
          break;
        }
      }
      flags = st->flags;
      if (flags & WHIRL_F_IDLE) {
        if (st->timer != 0) {
          if (TICK_DOWN(st->timer) != 0)
            st->flags = (st->flags & ~WHIRL_F_IDLE) | WHIRL_F_WOKEN;
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < WHIRL_WAKE_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < WHIRL_WAKE_DZ) {
          st->timer = WHIRL_WAKE_DELAY;
        }
        break;
      }
      if (flags & WHIRL_F_MARK)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, SFX_MODE_3D, &actor->voiceRow);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_PLUME, actor, 0);
      SubtractVector(g408, &SPY.posX, &actor->posX);
      distance = VectorLength(g408, VEC_LEN_XY);
      if (distance < st->radius && g408[2] >= -WHIRL_BELOW_REACH && g408[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + WHIRL_TOP_MARGIN < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(SPY.firstContactPoint, &actor->posX);
        if (st->flags & WHIRL_F_TARGET_CAM) {
          spyro->requestMask |= SPYRO_RQ_TARGET_CAMERA;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamFollowBehind;
          SPY.pCamAnchor = &SPY.posX;
          SPY.pCamTarget = &g_anCameraActiveTargetParams;
          spyro->requestMask |= SPYRO_RQ_CAMERA;
          g_anCameraActiveTargetParams.w[CAMP_YAW] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
        }
        st->timer = WHIRL_IDLE_DELAY;
        break;
      }
      if ((st->flags & ~WHIRL_F_TARGET_CAM) == WHIRL_F_WOKEN && st->timer != 0 &&
          TICK_DOWN(st->timer) != 0) {
        st->flags = (st->flags & ~WHIRL_F_WOKEN) | WHIRL_F_IDLE;
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

        if (CollideSphereWithWorldAndActors(&actor->posX, DEBRIS_RADIUS, DEBRIS_RADIUS) != 0) {
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
          actor->cullRadius = ACTOR_CULL_MAX;
          if (st->timer < BOULDER_BLAST_TICK) {
            st->timer = BOULDER_BLAST_TICK;
          }
        } else if (actor->flags & ACTOR_HIT_FLAME) {
          actor->subMode = BOULDER_CRACKING;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          actor->cullRadius = ACTOR_CULL_MAX;
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
          jetPos[0] = g_aSmokeJetEmitters[i >> 1].pos[0];
          jetPos[1] = g_aSmokeJetEmitters[i >> 1].pos[1];
          jetPos[2] = g_aSmokeJetEmitters[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, jetPos, jetPos);
          AddVector(jetPos, jetPos, &actor->posX);
          jetPos[3] = g_aSmokeJetEmitters[i >> 1].vel[0];
          jetPos[4] = g_aSmokeJetEmitters[i >> 1].vel[1];
          jetPos[5] = g_aSmokeJetEmitters[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, jetPos + 3, jetPos + 3);
          jetPos[3] += (GetRandomU32() & 0xF) - 8;
          jetPos[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, jetPos, (int)(jetPos + 3));
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          jetPos[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          jetPos[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          jetPos[2] = (GetRandomU32() & 0x7F) + 0xC0;
          jetPos[3] = jetPos[0] >> 4;
          jetPos[4] = jetPos[4] >> 4;
          jetPos[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, jetPos, jetPos);
          AddVector(jetPos, jetPos, &actor->posX);
          RotateVectorByMatrix(actor->mtx, jetPos + 3, jetPos + 3);
          g480[0] = (GetRandomU32() & 7) + 0x10;
          g480[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, jetPos, (int)g480);
          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, CBOULDER_VOICE_CRACK, 0);
          }
        }
        if (st->timer > BOULDER_CRACK_FRAMES - SHAKE_TICKS) {
          actor->rotX = g_aShakeRotOffsets[(BOULDER_CRACK_FRAMES - st->timer) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(BOULDER_CRACK_FRAMES - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)g_aShakeRotOffsets[(BOULDER_CRACK_FRAMES - st->timer) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(BOULDER_CRACK_FRAMES - st->timer) >> 1].b)) *
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
            c->subMode = (GetRandomU32() & 1) + SPARK_DEBRIS_LIFE;
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
            if (ACTOR_POOL_LOW()) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 1, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 3) + CHUNK_DEBRIS_LIFE;
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
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, SPYRO_EV_KNOCKBACK_2 | SPYRO_EV_CHARGE_HIT) != 0) {
          st->pushed = 1;
          g_anSpyroRequestVelocity.x = SPY.posX - actor->posX;
          g_nSpyroRequestVelocityZ = 0;
          g_nSpyroRequestVelocityY = SPY.posY - actor->posY;
          if (g_anSpyroRequestVelocity.x != 0 || g_nSpyroRequestVelocityY != 0) {
            int len = VectorLength(&g_anSpyroRequestVelocity.x, VEC_LEN_XYZ);

            ScaleVector3ByRatio(&g_anSpyroRequestVelocity.x, len,
                          (BOULDER_PUSH_RANGE - ApproxDist2D(&actor->posX, &SPY.posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = BOULDER_PUSH_VZ;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_BLAST), actor, CONTACT_STOP_AT_SOLID);
        if (++st->timer >= BOULDER_BURST_TICKS) {
          MarkActorLiveKill(actor);
          DespawnActorRecord(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }

    /* Respawner (RespawnState): puts its type's dormant actors back once Spyro is far away. */
    case ACTOR_TYPE_RESPAWNER: {
      RespawnState *st = actor->state;

      CopyVector(&actor->posX, &SPY.posX);

      if (TICK_DOWN(st->timer) != 0) {
        Actor *scan;

        st->timer = st->period;

        for (scan = g_pActorListBase; scan < g_pActorPoolDynBase; scan++) {
          if (scan->type == st->type && scan->mode >= ACTOR_MODE_DORMANT &&
              ApproxDist2D(&SPY.posX, scan->state) > RESPAWN_RANGE) {
            CopyVector(&scan->posX, scan->state);
            InsertActorIntoSpatialGrid(scan);
            scan->flags = 0;
            scan->mode = ACTOR_SPAWN_MODE;
            scan->killGroup &= ACTOR_KILL_GROUP_MASK;
            ANIM_RESET(scan, ACTOR_SPAWN_ANIM);
          }
        }
      }

      break;
    }

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case ACTOR_TYPE_SEED_POD: {
      SeedPodState *st = actor->state;

      actor->soundRange = SOUND_RANGE(0x8000);
      switch (actor->mode) {
      case SEEDPOD_CLOSED:
        if (actor->subMode == SEEDPOD_SUB_UNTINTED) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
          actor->subMode = SEEDPOD_SUB_TINTED;
          actor->updateDist = SEEDPOD_IDLE_UPDATE_DIST;
        }
        if (actor->flags & ACTOR_HITS_SPYRO) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = SEEDPOD_OPEN_UPDATE_DIST;
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

        st->prize->cullRadius = SEEDPOD_PRIZE_CULL;
        CopyVector(&st->prize->posX, &actor->posX);
        st->prize->posZ += LookupSine((st->timer << 11) / 90) / 3 + SEEDPOD_PRIZE_HOVER;
        st->prize->rotZ += g_nFrameStep * 2;
        if (st->timer < SEEDPOD_SHAKE_TICKS) {
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
        if (st->timer < SEEDPOD_SPARK_TICKS &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          ZeroVector(g480);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)g480);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < SEEDPOD_PRIZE_RANGE &&
             (unsigned int)(SPY.posZ - st->prize->posZ + SEEDPOD_PRIZE_DZ_BELOW) < SEEDPOD_PRIZE_DZ_SPAN) ||
            g_nSpyroState == SPYRO_ST_SUPERCHARGE || g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
            (g_nSpyroState == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, SEEDPOD_VOICE_BURST, 0);
          for (i = 0; i < SEEDPOD_SHARD_PAIRS; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < SEEDPOD_CHIPS; i++) {
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
          for (i = 0; i < SEEDPOD_SHARD_PAIRS; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < SEEDPOD_CHIPS; i++) {
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
        if (st->timer >= SEEDPOD_OPEN_TICKS) {
          int j;

          for (j = 0; j < SEEDPOD_RING_SPARKS; j++) {
            CopyVector(g496, &st->prize->posX);
            g496[0] += g_anCosineLut[j * 43] >> 6;
            g496[1] += g_anSineLut[j * 43] >> 6;
            g496[2] += 0x28;
            podSparkVel[0] = g_anCosineLut[j * 43] >> 7;
            podSparkVel[1] = g_anSineLut[j * 43] >> 7;
            podSparkVel[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, g496, (int)podSparkVel);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= ACTOR_KILL_GROUP_MASK;
          actor->updateDist = SEEDPOD_IDLE_UPDATE_DIST;
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

    /* Checkpoint: Spyro reaching it sets the respawn anchor
     * (SetSpawnAnchorFromActor); it then plays its anim for 0x30 frames. */
    case ACTOR_TYPE_CHECKPOINT:
      switch (actor->mode) {
      case CHECKPT_REARM:
        if (ApproxDist2D(&actor->posX, &SPY.posX) <= CHECKPT_REARM_RANGE) {
          break;
        }
        actor->mode = CHECKPT_ARMED;
        continue;

      case CHECKPT_ARMED: {
        int height;
        int drop;

        if (ApproxDist2D(&actor->posX, &spy->posX) >= CHECKPT_TOUCH_RANGE) {
          break;
        }

        height = actor->posZ - actor->floorOffset;
        drop = height - SPY.posZ;

        if (drop > 0 ? drop < CHECKPT_TOUCH_DZ : (SPY.posZ - height) < CHECKPT_TOUCH_DZ) {
          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = CHECKPT_FLASH;
          actor->anim = CHECKPT_ANIM_FLASH;
          actor->subMode = 0;
          continue;
        }
        break;
      }

      case CHECKPT_FLASH: {
        int d = g_nFrameStep;
        int tick = actor->subMode + d;

        actor->subMode = tick;
        if ((unsigned char)tick < CHECKPT_FLASH_FRAMES) {
          break;
        }
        actor->mode = CHECKPT_REARM;
        actor->anim = CHECKPT_ANIM_REST;
        continue;
      }
      }
      break;

    /* Portal guide (PortalState). */
    case ACTOR_TYPE_PORTAL_GUIDE: {
      PortalState *st = actor->state;
      PortalCamPath *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case PORTALGUIDE_ENTER: {

        SPY.requestMask =
            (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_NO_CLIP |
             SPYRO_RQ_FORCE_STATE | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_VEL_Z |
             SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_X);
        SPY.camPathReverse = actor->subMode;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = PORTALGUIDE_PATH_MAX_STEP;
        SPY.requestState = SPYRO_ST_JUMP;
        SPY.otBinBias = SPYRO_OT_BIAS_PINNED;

        p = st->desc;
        AddVector(g480, p->point[0].pos, p->point[1].pos);
        RShiftVector3(g480, 1);

        SubtractVector(&SPY.requestVelX, g480, &SPY.posX);
        AddVector(&SPY.requestVelX, &SPY.requestVelX, st->offset);

        mag = VectorLength(&SPY.requestVelX, VEC_LEN_XYZ);
        if (mag > PORTALGUIDE_JUMP_SPEED) {
          ScaleVector3ByRatio(&SPY.requestVelX, mag, PORTALGUIDE_JUMP_SPEED);
        } else {
          if (actor->subMode != PORTAL_SIDE_FORWARD) {
            progressed = (st->desc->index < (st->desc->count - 1));
          } else {
            progressed = st->desc->index;
          }

          if (progressed) {
            actor->mode = PORTALGUIDE_GLIDE;
          }
        }

        if (CollideSphereWithWorldAndActors(&CAM.posX, PORTALGUIDE_CAM_PROBE_RADIUS, PORTALGUIDE_CAM_PROBE_RADIUS) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = PORTALGUIDE_IDLE;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          SPY.requestState = SPYRO_ST_GLIDE;
          g_nCdStreamState = LSTREAM_PRESERVE;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nSkipGamestateDraw = 1;
          SPY.requestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.bodyYaw;

          SPY.bodyYaw += g_nPortalSkyYawSpin;
          SPY.bodyRotZ = SPY.bodyYaw >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nTransitionSkyPitchTilt = g_nPortalSkyPitchTilt;
          if (st->cameraPreset >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->cameraPreset];
          } else {
            CAM.preset = 0;
          }
        } else {
          SPY.requestMask |= SPYRO_RQ_PORTAL_PATH_CAMERA;
        }
        break;
      }
      case PORTALGUIDE_GLIDE: {
        SPY.requestMask = SPYRO_RQ_PENDING | SPYRO_RQ_NO_CLIP | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_FORCE_STATE;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = PORTALGUIDE_PATH_MAX_STEP;
        SPY.requestState = SPYRO_ST_GLIDE;
        SPY.otBinBias = SPYRO_OT_BIAS_PINNED;

        if (CollideSphereWithWorldAndActors(&CAM.posX, PORTALGUIDE_CAM_PROBE_RADIUS, PORTALGUIDE_CAM_PROBE_RADIUS) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = PORTALGUIDE_IDLE;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          g_nCdStreamState = LSTREAM_PRESERVE;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nSkipGamestateDraw = 1;
          SPY.requestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.bodyYaw;

          SPY.bodyYaw += g_nPortalSkyYawSpin;
          SPY.bodyRotZ = SPY.bodyYaw >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nTransitionSkyPitchTilt = g_nPortalSkyPitchTilt;
          if (st->cameraPreset >= 0) {
            CAM.preset = &g_aCameraXformPresets[st->cameraPreset];
          } else {
            CAM.preset = 0;
          }
        } else {
          SPY.requestMask |= SPYRO_RQ_PORTAL_PATH_CAMERA;
        }
        break;
      }
      }
      break;
    }

    /* Balloon: bobs about its rest height. */
    case ACTOR_TYPE_BALLOON: {
      int *base = actor->state;

      if (actor->mode == BALLOON_INIT) {
        *base = actor->posZ;
        actor->mode = BALLOON_BOB;
        break;
      }

      actor->posZ = *base + (g_anSineLut[actor->subMode] >> 4);
      actor->subMode += g_nFrameStep;
      break;
    }

    /* Life chest (LifeChestState). */
    case ACTOR_TYPE_LIFE_CHEST: {
      LifeChestState *st = actor->state;
      Actor *prize;
      int i;
      int a;
      int b;

      if (st->restMode == COLLREST_SNAP) {
        SnapActorToGround(actor);
        EncodeHitNormalToActorGroundCode(actor);
      }

      if (actor->mode == LIFECHEST_CLOSED) {
        st->phase += g_nFrameStep;

        if (st->phase > LIFECHEST_OPEN_TICK) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->mode = LIFECHEST_OPEN;
          actor->frameProgress = ANIM_RATE_TICKS(8);
          actor->frameRate = ANIM_RATE_TICKS(8);
          actor->nextAnim = LIFECHEST_OPEN;

          st->passenger = g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_RIDER, actor);
          st->passenger->rotX = actor->rotX;
          st->passenger->rotY = actor->rotY;
          st->passenger->rotZ = actor->rotZ;
          st->passenger->frameRate = ANIM_RATE_TICKS(4);
          st->passenger->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
          st->passenger->cullRadius = ACTOR_CULL_HIDDEN;
          st->passenger->drawn = 0;
          st->passenger->otBias = 5;

          PlayActorMeshVoice(actor, LIFECHEST_VOICE_OPEN, 0);
        } else if (st->phase >= LIFECHEST_SHAKE_TICK) {
          actor->rotX = g_aShakeRotOffsets[LIFECHEST_SHAKE_STEP(st)].a + st->savedRotX;
          actor->rotY = g_aShakeRotOffsets[LIFECHEST_SHAKE_STEP(st)].b + st->savedRotY;
          a = (signed char)g_aShakeRotOffsets[LIFECHEST_SHAKE_STEP(st)].a;
          b = (signed char)g_aShakeRotOffsets[LIFECHEST_SHAKE_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->savedPosZ + (a + b) * 6;

          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, LIFECHEST_VOICE_RATTLE, 0);
          }
        } else if (st->phase >= LIFECHEST_SETTLE_TICK) {
          /* Just before the shake: remember the rest pose. */
          st->savedRotX = actor->rotX;
          st->savedRotY = actor->rotY;
          st->savedPosZ = actor->posZ;
          if (st->restMode == COLLREST_SNAP) {
            st->phase = 0;
          }
        }
      } else {
        if (actor->anim == LIFECHEST_OPEN) {
          st->phase += g_nFrameStep;
          st->passenger->cullRadius = CULL_RADIUS(0x1000);
          st->passenger->updateDist = ACTOR_UPDATE_ALWAYS;
        }

        if (st->phase >= LIFECHEST_OPEN_FRAMES) {
          DespawnActorRecord(st->passenger);
          st->phase = GetRandomU32() & 0xFF;
          actor->frameProgress = ANIM_RATE_TICKS(8);
          actor->frameRate = ANIM_RATE_TICKS(8);
          actor->mode = LIFECHEST_CLOSED;
          actor->nextAnim = LIFECHEST_CLOSED;
          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, LIFECHEST_VOICE_CLOSE, 0);
          }
        }
      }

      if ((actor->flags & ACTOR_HITS_SPYRO) == 0) {
        break;
      }

      prize = SpawnActorDeathDrop(actor, DROP_IN_PLACE, 0, 0);
      if (st->restMode == COLLREST_SNAP) {
        ((CollectibleState *)prize->state)->mode = st->restMode;
      }

      actor->soundRange = SOUND_RANGE(0x8000);
      PlayActorMeshVoice(actor, LIFECHEST_VOICE_BREAK, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < LIFECHEST_SHARD_PAIRS; i++) {
        if (ACTOR_POOL_LOW()) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD + 1, actor);
      }

      for (i = 0; i < LIFECHEST_CHIPS; i++) {
        if (ACTOR_POOL_LOW()) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD + 2, actor);
      }

      for (i = 0; i < LIFECHEST_SPARK_RING; i++) {
        g496[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        g496[1] = (g_anSineLut[i * 32] >> 8) * 3;
        g496[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)g496);
      }

      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x18);

      if (actor->mode == LIFECHEST_OPEN) {
        DespawnActorRecord(st->passenger);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[LIFECHEST_VOICE_BURST], actor, SFX_MODE_3D,
                    (void *)(long)actor->voiceRow);
      DespawnActorRecord(actor);
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
          g496[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g496[1] = (g_anSineLut[bearing] * 3) >> 4;
          g496[2] = 0;
          letterSide[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterSide, st->count - 1);
          LShiftVector3(letterSide, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = g496[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = g496[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterSide, letterSide, st->index);
          SubtractVector(letterSlot, letterSlot, letterSide);
          swing = &g_anCosineLut[bearing];
          actor->posX = g496[0] * *swing;
          actor->posY = g496[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g496);
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
