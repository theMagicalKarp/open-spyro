/* Level2_UpdateActors -- per-frame actor update for level 2 (Dark Hollow).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (level 0's
 * Level0_UpdateActors, level 4's Level4_UpdateActors, level 30's
 * Level30_UpdateActors); the level-2-only classes are the patrolling guard
 * pair (115), the hit reactors (129, 157), the post guard (165), the path
 * patroller (166), the locked chest (174) and the wandering
 * critter (412).
 *
 * Load-bearing source forms (matched 2026-09-28):
 *   - Loop-top constant carriers `sixteen`, `oneq` (a `short`) and `one`, as
 *     in level 4 (see Level4_UpdateActors).
 *   - loop.c lowers its hoisting threshold by 3 for every constant it moves
 *     out of the loop, so each constant's fate depends on the ones before it.
 *     Here 2, 13 and the two table addresses are hoisted (and spilled: they
 *     come back as `li/lui t1`), while 3 and 4 must stay inline. Case 115
 *     uses ENTER_POSE_B, whose empty loop gives the `nextAnim = n` store its own
 *     copy of the constant; without it the 3 and 4 groups live long enough
 *     to be hoisted.
 *   - Stack layout: `idx[2]` (BLKmode, so it gets its slot at declaration)
 *     and the case 166/174 vectors svR/keyOffset sit between sparxToTarget and svK; case 421
 *     borrows case 76's first vector.
 *   - `do { } while (0);` barriers at the head of cases 165/166 and in 412's
 *     hit block keep cse from folding `spy` into the absolute address.
 *   - Case 165 has two `mode = 3; continue;` tails (jump.c merges them);
 *     one shared tail changes the register choice for the `cur` store.
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
#include "cd_stream.h"
#include "hud_counter.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. */
#define ANIM_RATE16 sixteen

void Level2_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int sixteen;
  int one;
  short oneq;
  char *base;
  int dt;

  /* Scratch vectors shared by several cases. */
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
  int idx[2];
  int svR[3];
  int keyOffset[3];
  int svK[3];
  int shardTwinkle[3];
  int svN[3];
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
    sixteen = 0x10;
    oneq = 1;
    one = 1;
    spy = (SpyroObj *)(base - 8);

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

      if (actor->mode != LEVELEXIT_FAR ? dist < LEVELEXIT_HIDE_RANGE : dist < LEVELEXIT_SHOW_RANGE) {
        if (actor->mode == LEVELEXIT_FAR) {
          actor->posZ += LEVELEXIT_NAME_HEIGHT;
          SpawnNameLetterActors(actor, NAME_LETTERS_RETURN_HOME);
          actor->posZ -= LEVELEXIT_NAME_HEIGHT;
        }
        actor->mode = oneq;
      } else {
        actor->mode = LEVELEXIT_FAR;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < LEVELEXIT_SPARKLE_RANGE) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL, &actor->posX, tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL_SMEAR, &actor->posX, tick);
        }
      }

      if (dist < LEVELEXIT_ENTER_RANGE) {
        int top = g_nSpyroWorldPosZ;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - LEVELEXIT_LIFT_BOTTOM) < LEVELEXIT_LIFT_TOP - LEVELEXIT_LIFT_BOTTOM) {
          int depth = LEVELEXIT_DEFAULT_DEPTH;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == SPYRO_ST_LIFT || top + LEVELEXIT_TOP_MARGIN < z + depth) {
            if (g_abHudCounterDisplayState == HUD_CTR_SHOWN) {
              RefreshHudLevelGemCounter();
            }
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_TARGET_CAMERA);
            g_pScriptedCameraActor = actor;
            g_dwSpyroTriggerEventFlags |= SPYRO_EV_LIFT;
            g_nSpyroRequestTargetZ = ACTOR_RAW(actor, int, posZ) + LEVELEXIT_LIFT_TOP;
            CopyVector(SPY_HELD_FIELD(base, int, firstContactPoint), &actor->posX);

            if (depth < drop) {
              int yaw = g_nCameraEulerPitch & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -LEVELEXIT_LEAVE_PITCH) {
                g_abLevelExited[g_nLevelIndex] = 1;
                g_nCdStreamState = LSTREAM_PRESERVE;
                g_nEnteredPortalIdx = -1;
                g_nPortalTransitionActive = one;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = GS_LEVEL_LOAD;
                g_nSkipGamestateDraw = 1;
                g_nReturnPortalLevelId = g_nActiveLevelId;
                g_nNextLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(RESET_SPYRO_SOFT);
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
                      8, 0x82, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);

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
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
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

          CopyVector(svA, &spy->posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * EGG_APEX_DIAG_4;
          y = cs * EGG_APEX_DIAG_4;
          svA[0] += (y + x) * 4 >> 12;
          svA[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * EGG_APEX_DIAG_4) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * (unsigned)EGG_APEX_DIAG_4)) *
                        4 >>
                    12;
          svA[2] += EGG_APEX_HEIGHT;
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

        if (st->phase < EGG_FLIGHT_FRAMES) {
          SVectorToVectorShl2(eggArcBase, st->centre);
          SVectorToVectorShl2(eggArcOffset, st->apex);
          SubtractVector(eggArcOffset, eggArcOffset, eggArcBase);
          ScaleVector3Sat(eggArcOffset, eggArcOffset, st->phase);
          RShiftVector3(eggArcOffset, 6);
          AddVector(&actor->posX, eggArcBase, eggArcOffset);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(svD, &spy->posX);
          svD[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[2] += EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->centre, svD);
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

    /* Explosion shard with a ground bounce (TumbleState). */
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1:
    case ACTOR_TYPE_LIFE_CHEST_SHARD:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 1: {
      TumbleState *st = actor->state;

      if (st->life <= 0 || actor->drawn == 0) {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
        DespawnActorRecord(actor);
        continue;
      }

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
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)svD);
      }

      st->life -= 1;
      break;
    }

    /* Falling chest fragments (TumbleState). */
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
          CopyVector(svA, &actor->posX);
          svA[2] += GROUND_PROBE_LIFT;
          FindGroundHeightBelow(svA, GROUND_PROBE_FAR);
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
            svF[2] += GROUND_PROBE_LIFT;
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
      SaveHostState *st = actor->state;
      Actor *armer = &g_pActorListBase[st->armer];
      Actor *anchor = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY:
        actor->collision = 0;
        if (ApproxDist2D(&anchor->posX, &SPY.posX) <= SAVEHOST_REARM_RANGE) {
          break;
        }
        actor->mode = SAVEHOST_WAIT_ARMED;
        continue;

      case SAVEHOST_WAIT_ARMED:
        if (armer->mode >= ACTOR_MODE_DORMANT) {
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = SAVEHOST_APPEAR_DELAY;
          st->baseZ = actor->posZ;
        }
        break;

      case SAVEHOST_HIDDEN:
        if (st->timer != 0) {
          st->timer -= 1;
        } else {
          actor->collision = 0;
          if (ApproxDist2D(&actor->posX, &spy->posX) < SAVEHOST_APPEAR_RANGE) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop < SAVEHOST_APPEAR_DZ : (SPY.posZ - height) < SAVEHOST_APPEAR_DZ) {
              if (st->prompted == 0 || anchor->mode == CHECKPT_ARMED) {
                int *at = &actor->posX;

                st->prompted = 0;
                actor->posZ = st->baseZ + SAVEHOST_POP_RAISE;
                actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                             spy->posY - actor->posY, ATAN_ANGLE8) -
                               0x80;
                actor->cullRadius = SAVEHOST_CULL_RADIUS;
                actor->mode = SAVEHOST_POP_IN;
                actor->scale = SAVEHOST_POP_SCALE;
                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, at, 0xC);
                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(at, SAVEHOST_PROBE_DEPTH);
                EncodeHitNormalToActorGroundCode(actor);
                actor->groundCode |= ACTOR_GROUND_VALID;
              }
            }
          }
        }
        break;

      case SAVEHOST_POP_IN:
        if (actor->scale > ACTOR_SCALE_UNIT) {
          actor->scale -= SAVEHOST_SCALE_STEP;
          actor->rotZ += SAVEHOST_SPIN_STEP;
        } else {
          actor->mode = SAVEHOST_HOVER;
          st->timer = 0;
          st->wanderTimer = SAVEHOST_WANDER_DELAY;
        }
        break;

      case SAVEHOST_HOVER: {
        int height;
        int drop;
        int close;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if (CHECKPT_SETTLED(anchor) &&
            ApproxDist2D(&anchor->posX, &spy->posX) < SAVEHOST_PROMPT_RANGE &&
            (height = anchor->posZ - anchor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < SAVEHOST_PROMPT_DZ : (SPY.posZ - height) < SAVEHOST_PROMPT_DZ) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = SAVEHOST_POP_OUT;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) > SAVEHOST_HOVER_LEASH ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             close = drop > 0 ? drop <= SAVEHOST_LEASH_DZ : (SPY.posZ - height) <= SAVEHOST_LEASH_DZ,
             close == 0)) {
          actor->mode = SAVEHOST_POP_OUT;
          break;
        }

        st->wanderTimer -= 1;
        if (st->wanderTimer == 0) {
          int reach = (GetRandomU32() & 0x3F) + SAVEHOST_WANDER_DIST;
          int bearing = ArcTan2(spy->posX - anchor->posX,
                                      spy->posY - anchor->posY, ATAN_ANGLE8);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          EBB_BREAK();
          CopyVector(st->wanderTo, &anchor->posX);
          aim &= 0xFF;
          aim *= 0x10;
          actor->mode = SAVEHOST_WANDER;
          st->wanderTo[0] -= (LookupCosine(aim) * reach) >> 12;
          st->wanderTo[1] -= (LookupSine(aim) * reach) >> 12;
          st->faceYaw = ArcTan2(spy->posX - st->wanderTo[0],
                                  spy->posY - st->wanderTo[1], ATAN_ANGLE8);
          st->wanderTimer = 0;
        }
        break;
      }

      case SAVEHOST_WANDER: {
        int height;
        int drop;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if (CHECKPT_SETTLED(anchor) &&
            ApproxDist2D(&anchor->posX, &spy->posX) < SAVEHOST_PROMPT_RANGE &&
            (height = anchor->posZ - anchor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < SAVEHOST_PROMPT_DZ : (SPY.posZ - height) < SAVEHOST_PROMPT_DZ) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = SAVEHOST_POP_OUT;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) > SAVEHOST_WANDER_LEASH ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop > SAVEHOST_LEASH_DZ : (SPY.posZ - height) > SAVEHOST_LEASH_DZ)) {
          actor->mode = SAVEHOST_POP_OUT;
          break;
        }
        {
          int *here = &actor->posX;
          int turn;
          int w0;
          int wSpan;
          int wDen;

          st->wanderTimer += 1;
          SubtractVector(svF, st->wanderTo, here);
          turn = SignedAngleDelta8(st->faceYaw, actor->rotZ);
          w0 = LookupCosine((st->wanderTimer - 1) << 7);
          wSpan = w0 - LookupCosine(st->wanderTimer << 7);
          wDen = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;
          actor->posX += (svF[0] * wSpan) / wDen;
          actor->posY += (svF[1] * wSpan) / wDen;
          actor->rotZ += (turn * wSpan) / wDen;
          FindGroundHeightBelow(here, SAVEHOST_PROBE_DEPTH);
          EncodeHitNormalToActorGroundCode(actor);

          if (st->wanderTimer == SAVEHOST_WANDER_FRAMES) {
            actor->mode = SAVEHOST_HOVER;
            st->wanderTimer = (GetRandomU32() & 0x1F) + SAVEHOST_WANDER_DELAY;
          }
        }
        break;
      }

      case SAVEHOST_POP_OUT:
        if (actor->scale < SAVEHOST_POP_SCALE) {
          actor->scale += SAVEHOST_SCALE_STEP;
          actor->rotZ += SAVEHOST_SPIN_STEP;
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

      break;
    }

    /* Type 115: patrolling guard (GuardPairState). Charges Spyro when he is
     * near and level with it, and cues its partner when it faces it. */
    case ACTOR_TYPE_GUARD_PAIR: {
      GuardPairState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != GPAIR_KNOCKED) {
        st->speed = GPAIR_KNOCK_SPEED;
        st->knockHeading = SPY.bodyRotZ;
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE_B(actor, GPAIR_KNOCKED);
      }

      switch (actor->mode) {
      case GPAIR_IDLE: {
        int dz;

        TrackEntityYawTowardSpyro(actor, 3, 0x10, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &spy->posX) < GPAIR_ROUSE_RANGE &&
            ((dz = actor->posZ - SPY.posZ) > 0 ? dz < GPAIR_LEVEL_DZ
                                               : SPY.posZ - actor->posZ < GPAIR_LEVEL_DZ)) {
          st->speed = GPAIR_ROUSE_SPEED;
          ENTER_POSE_B(actor, GPAIR_ROUSE);
        }

        if (st->partner != -1 && TICK_DOWN(st->timer)) {
          Actor *mate = &g_pActorListBase[st->partner];

          if ((unsigned int)(WrapAngleDelta8(
                                 ArcTan2(mate->posX - actor->posX,
                                               mate->posY - actor->posY, ATAN_ANGLE8),
                                 actor->rotZ) +
                             0x20) < 7) {
            actor->subMode = GPCUE_RAISE;
            actor->mode = GPAIR_CUE;
            continue;
          }
        }
        break;
      }
      case GPAIR_ROUSE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE_B(actor, GPAIR_WATCH);
        }
        break;
      case GPAIR_WATCH: {
        int dist = ApproxDist2D(&actor->posX, &spy->posX);
        int dz;

        TrackEntityYawTowardSpyro(actor, 3, 0, YAW_HOLD_IN_BAND);
        if (dist < GPAIR_SWING_RANGE &&
            ((dz = actor->posZ - SPY.posZ) > 0 ? dz < GPAIR_LEVEL_DZ
                                               : SPY.posZ - actor->posZ < GPAIR_LEVEL_DZ)) {
          st->speed = GPAIR_ROUSE_SPEED;
          st->gloatCount = 0;
          ENTER_POSE_B(actor, GPAIR_SWING);
        }
        if (dist > GPAIR_LOWER_RANGE) {
          ENTER_POSE_B(actor, GPAIR_LOWER);
        }
        break;
      }
      case GPAIR_SWING:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (SPY.state == SPYRO_ST_HURT_PINNED) {
          st->gloatCount = 2;
        }
        if (g_nActorAnimDone != 0) {
          if (st->gloatCount != 0) {
            ENTER_POSE_B(actor, GPAIR_GLOAT);
          }
          ENTER_POSE_B(actor, GPAIR_WATCH);
        }
        break;
      case GPAIR_LOWER:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE_B(actor, GPAIR_IDLE);
        }
        break;
      case GPAIR_KNOCKED:
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->speed, GPAIR_KNOCK_CONTACT, GPAIR_MOVE_RADIUS, 0);
          st->speed -= GPAIR_KNOCK_DRAG;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, GPAIR_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      case GPAIR_GLOAT:
        if (g_nActorAnimDone != 0 && TICK_DOWN(st->gloatCount)) {
          ENTER_POSE_B(actor, GPAIR_IDLE);
        }
        break;
      case GPAIR_CUE:
        switch (actor->subMode) {
        case GPCUE_RAISE:
          ANIM_GO(actor, GPAIR_ROUSE);
          if (g_nActorAnimDone != 0) {
            actor->subMode++;
          }
          break;
        case GPCUE_SIGNAL:
          ANIM_GO(actor, GPAIR_ANIM_SIGNAL);
          if (g_nActorAnimNewFrame != 0 && actor->frame == GPAIR_SIGNAL_FRAME) {
            g_pActorListBase[st->partner].mode = 0xD;
            g_pActorListBase[st->partner].anim = 0xD;
            g_pActorListBase[st->partner].nextAnim = 0xD;
            g_pActorListBase[st->partner].frame = 0;
            g_pActorListBase[st->partner].nextFrame = 0;
            g_pActorListBase[st->partner].frameProgress = 0;
          }
          if (g_nActorAnimDone != 0) {
            actor->subMode++;
          }
          break;
        case GPCUE_LOWER:
          ANIM_GO(actor, GPAIR_LOWER);
          if (g_nActorAnimDone != 0) {
            st->timer = GPAIR_CUE_GAP;
            ENTER_POSE_B(actor, GPAIR_IDLE);
          }
          break;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Sparx (SparxState): hovers beside Spyro, glows by health, catches
     * butterflies (health + 1) and fetches nearby pickups. Despawns at 0. */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = actor->state;

      if (g_anSparxHealthBlock[0] >= SPARX_BLUE) {
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
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              svA[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              svA[1] -=
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

            if (CollideSphereWithWorldAndActors(at, SPARX_COLLIDE_RADIUS, SPARX_COLLIDE_RADIUS) != 0) {
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

    /* Charge-reactive prop: a charge plays its reaction animation once. */
    case ACTOR_TYPE_CHARGE_REACT_PROP:
      switch (actor->mode) {
      case CREACT_IDLE:
        if (actor->flags & ACTOR_HIT_CHARGE) {
          ENTER_POSE(actor, CREACT_REACT);
        }
        break;
      case CREACT_REACT:
        if (g_nActorAnimDone != 0) {
          actor->flags = 0;
          ENTER_POSE(actor, CREACT_IDLE);
        }
        break;
      }
      break;

    /* Type 157: starts its sound and animation when hit, then settles. */
    case ACTOR_TYPE_FLAME_CHUNK_TRIGGER:
      if ((actor->flags & ACTOR_HIT_FLAME) && actor->mode == FLAMETRIG_ARMED) {
        ModifyWorldChunkAnimFlags(*(int *)actor->state, CHUNK_ANIM_RESUME, 0);
        actor->mode = FLAMETRIG_FIRED;
        ANIM_RESET(actor, FLAMETRIG_FIRED);
      } else if (actor->mode == FLAMETRIG_FIRED && g_nActorAnimDone != 0) {
        ENTER_POSE(actor, FLAMETRIG_SPENT);
      }
      break;

    /* Type 165: post guard (PostState). Waits at one of two posts, steps
     * out to the post nearer Spyro, hops at him with its partner, and is
     * knocked away spinning when hit. */
    case ACTOR_TYPE_POST_GUARD: {
      PostState *st = actor->state;

      TickActorFlameGlow(actor);
      EBB_BREAK();
      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != POSTGUARD_KNOCKED &&
          (actor->flags != ACTOR_HIT_FLAME || st->flameProof == 0)) {
        st->knockZSpeed = POSTGUARD_KNOCK_VZ;
        st->knockSpeed = POSTGUARD_KNOCK_SPEED;
        st->timer = POSTGUARD_KNOCK_FRAMES;
        if (actor->flags == ACTOR_HIT_CHARGE) {
          st->knockZSpeed = POSTGUARD_CHARGE_KNOCK_VZ;
          st->knockSpeed = POSTGUARD_CHARGE_KNOCK_SPEED;
          st->timer = POSTGUARD_CHARGE_KNOCK_FRAMES;
        }
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->spinX = sixteen - (GetRandomU32() & 0x1F);
        st->spinY = sixteen - (GetRandomU32() & 0x1F);
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, POSTGUARD_KNOCKED);
      }

      actor->flags = 0;
      switch (actor->mode) {
      case POSTGUARD_IDLE:
        switch (st->mode) {
        case POSTGUARD_POST_PICK:
          if (ApproxDist2D(&spy->posX, &st->path->nodes[0].x) < POSTGUARD_POST_RANGE) {
            if (AbsAngleDelta8(ArcTan2(spy->posX - actor->posX,
                                            SPY.posY - actor->posY, ATAN_ANGLE8),
                              ArcTan2(st->path->nodes[0].x - actor->posX,
                                            st->path->nodes[0].y - actor->posY,
                                            ATAN_ANGLE8)) >= POSTGUARD_POST_CONE ||
                ApproxDist2D(&actor->posX, &spy->posX) >
                    ApproxDist2D(&actor->posX, &st->path->nodes[0].x)) {
              st->path->cur = 0;
              actor->mode = POSTGUARD_TO_POST;
              continue;
            }
          } else if (ApproxDist2D(&spy->posX, &st->path->nodes[1].x) < POSTGUARD_POST_RANGE) {
            if (AbsAngleDelta8(ArcTan2(spy->posX - actor->posX,
                                            SPY.posY - actor->posY, ATAN_ANGLE8),
                              ArcTan2(st->path->nodes[1].x - actor->posX,
                                            st->path->nodes[1].y - actor->posY,
                                            ATAN_ANGLE8)) >= POSTGUARD_POST_CONE ||
                ApproxDist2D(&actor->posX, &spy->posX) >
                    ApproxDist2D(&actor->posX, &st->path->nodes[1].x)) {
              st->path->cur = 1;
              actor->mode = POSTGUARD_TO_POST;
              continue;
            }
          }
          break;
        case POSTGUARD_POST_PATROL:
          ANIM_GO(actor, POSTGUARD_ANIM_RUN);
          PivotActorAlongPath(actor, st->path, 0x100, POSTGUARD_PATROL_SPEED, 0xC8, 5, 0xA, actor->group,
                        MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < POSTGUARD_PATROL_ALERT_RANGE) {
            SetActorGroupState(actor, POSTGUARD_ALERTED);
          }
          break;
        case POSTGUARD_POST_WATCH:
          TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
          if (g_nActorAnimDone != 0) {
            ANIM_GO(actor, POSTGUARD_ANIM_STAND);
          }
          if (ApproxDist2D(&actor->posX, &SPY.posX) < POSTGUARD_WATCH_RANGE) {
            actor->subMode = 0;
            st->timer = RandomIntInRange(POSTGUARD_REST_MIN_FRAMES, POSTGUARD_REST_MAX_FRAMES);
            actor->mode = POSTGUARD_HOP;
            continue;
          }
          break;
        case POSTGUARD_POST_AMBUSH:
          switch (st->phase) {
          case POSTGUARD_AMBUSH_WAIT:
            TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
            if (ApproxDist2D(&actor->posX, &spy->posX) < POSTGUARD_AMBUSH_RANGE &&
                actor->posZ - actor->floorOffset - SPY.posZ < 0 && SPY.airborneFrames == 0) {
              PostState *other = g_pActorListBase[st->partner].state;

              st->phase = POSTGUARD_AMBUSH_DELAY;
              other->timer = RandomIntInRange(0, 0xF);
              other->phase = POSTGUARD_AMBUSH_DELAY;
            }
            break;
          case POSTGUARD_AMBUSH_DELAY:
            if (TICK_DOWN(st->timer)) {
              st->phase++;
            }
            break;
          case POSTGUARD_AMBUSH_ROUSE:
            ANIM_GO(actor, POSTGUARD_ANIM_SHOUT);
            if (g_nActorAnimDone != 0) {
              st->phase++;
            }
            break;
          case POSTGUARD_AMBUSH_RUN:
            ANIM_GO(actor, POSTGUARD_ANIM_RUN);
            if (PivotActorAlongPath(actor, st->path, 0x100, POSTGUARD_AMBUSH_SPEED, 0, 8, 0x32, PIVOT_TURN_ANY,
                              MOVE_LIFT | MOVE_FOLLOW_FLOOR) == PATH_ARRIVED(0)) {
              st->phase++;
            }
            break;
          case POSTGUARD_AMBUSH_FACE:
            if (TrackEntityYawTowardSpyro(actor, 6, 0xA, YAW_STEP_IN_BAND)) {
              actor->mode = POSTGUARD_HOP;
              continue;
            }
            break;
          }
          break;
        }
        break;

      case POSTGUARD_HOP:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        switch (st->sub) {
        case POSTGUARD_HOP_REST:
          ANIM_GO(actor, POSTGUARD_ANIM_REST);
          st->flameProof = 1;
          if (TICK_DOWN(st->timer) && g_nActorAnimDone != 0) {
            st->sub = POSTGUARD_HOP_READY;
            st->flameProof = 0;
            ANIM_GO(actor, POSTGUARD_ANIM_READY);
          }
          break;
        case POSTGUARD_HOP_READY:
          if (TICK_DOWN(st->timer) &&
              ApproxDist2D(&actor->posX, &spy->posX) < POSTGUARD_LUNGE_RANGE) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if ((drop > 0 ? drop < POSTGUARD_LUNGE_DZ : (SPY.posZ - height) < POSTGUARD_LUNGE_DZ) &&
                actor->anim == POSTGUARD_ANIM_READY) {
              st->timer = POSTGUARD_LUNGE_REST;
              ANIM_GO(actor, POSTGUARD_ANIM_LUNGE);
            }
          }
          if (g_nActorAnimDone != 0) {
            if (actor->anim == POSTGUARD_ANIM_LUNGE) {
              ANIM_GO(actor, POSTGUARD_ANIM_READY);
            } else {
              st->flameProof = 1;
              st->sub = POSTGUARD_HOP_REST;
              st->timer = RandomIntInRange(POSTGUARD_REST_MIN_FRAMES, POSTGUARD_REST_MAX_FRAMES);
              ANIM_BLEND(actor, POSTGUARD_ANIM_REST);
            }
          }
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) > POSTGUARD_HOP_LEAVE_RANGE) {
          actor->mode = POSTGUARD_IDLE;
          continue;
        }
        break;

      case POSTGUARD_KNOCKED:
        actor->rotX += st->spinX;
        actor->rotY += st->spinY;
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, POSTGUARD_KNOCK_DRAG, POSTGUARD_KNOCK_GRAVITY);
        if (g_nActorAnimDone != 0 && actor->nextAnim == POSTGUARD_ANIM_KNOCKED && actor->anim != POSTGUARD_ANIM_KNOCKED_END) {
          g_nActorAnimDone = 0;
          ANIM_RESET(actor, POSTGUARD_ANIM_KNOCKED_END);
        }
        if (TICK_DOWN(st->timer)) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, POSTGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case POSTGUARD_TO_POST:
        ANIM_GO(actor, POSTGUARD_ANIM_RUN);
        if (TurnActorYawTowardAngle(
                actor,
                ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8),
                6, 0x14, one)) {
          MoveActorAlongHeading(actor, POSTGUARD_TO_POST_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x) < POSTGUARD_POST_ARRIVE ||
            ApproxDist2D(&actor->posX, &SPY.posX) < POSTGUARD_POST_STOP_RANGE) {
          ANIM_BLEND(actor, POSTGUARD_ANIM_REST);
          actor->mode = POSTGUARD_HOP;
          continue;
        }
        break;

      case POSTGUARD_ALERTED:
        if (TrackEntityYawTowardSpyro(actor, 7, 0x14, YAW_STEP_IN_BAND)) {
          actor->mode = POSTGUARD_SHOUT;
          continue;
        }
        break;

      case POSTGUARD_SHOUT:
        ANIM_GO(actor, POSTGUARD_ANIM_SHOUT);
        if (g_nActorAnimDone != 0) {
          st->sub = POSTGUARD_HOP_REST;
          ANIM_BLEND(actor, POSTGUARD_ANIM_REST);
          actor->mode = POSTGUARD_HOP;
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 166: path patroller (PatrolState). Walks its path back and forth,
     * and charges Spyro down the path when he is ahead of it. */
    case ACTOR_TYPE_PATH_CHARGER: {
      PatrolState *st = actor->state;

      TickActorFlameGlow(actor);
      EBB_BREAK();
      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != PCHARGER_KNOCKED &&
          AbsAngleDelta8(actor->rotZ,
                        ArcTan2(spy->posX - actor->posX,
                                      spy->posY - actor->posY, ATAN_ANGLE8)) >= PCHARGER_GUARDED_ARC) {
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, PCHARGER_KNOCKED);
      }

      actor->flags = 0;
      switch (actor->mode) {
      case PCHARGER_PATROL: {
        int turned;
        int ang = ArcTan2(
            st->path->nodes[st->path->cur].x - actor->posX,
            st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);

        turned = 0;

        if (AbsAngleDelta8(ang, actor->rotZ) < PCHARGER_WALK_FACING) {
          TurnActorYawTowardAngle(actor, ang, 5, 0, YAW_HOLD_IN_BAND);
          MoveActorAlongAngle(actor, ang, PCHARGER_WALK_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (st->afterCharge != 0 ||
              ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < PCHARGER_NODE_RANGE) {
            PathData *path = st->path;

            if (path->dir == 1) {
              if (path->cur + 1 == path->count) {
                path->dir = -path->dir;
                st->path->cur = st->path->count - 2;
                turned = 1;
              } else {
                path->cur = path->dir + path->cur;
              }
            } else if (path->cur == 0) {
              turned = 1;
              path->dir = -path->dir;
              st->path->cur = 1;
            } else {
              path->cur = path->dir + path->cur;
            }
          }
        } else {
          TurnActorYawTowardAngle(actor, ang, 5, 0, YAW_HOLD_IN_BAND);
        }

        if (turned) {
          if (st->mode == PCTURN_TAUNTED) {
            st->near = 1;
          } else {
            st->near = 0;
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < PCHARGER_TAUNT_RANGE &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          SPY.posY - actor->posY, ATAN_ANGLE8)) < PCHARGER_TAUNT_FACING &&
              (st->mode == PCTURN_FIRST || RandomIntInRange(0, 2) == 0)) {
            st->mode = PCTURN_TAUNTED;
            ENTER_POSE(actor, PCHARGER_TAUNT);
          }
          st->mode = PCTURN_PLAIN;
        }

        st->afterCharge = 0;
        if (TICK_DOWN(st->timer) &&
            actor->posZ - actor->floorOffset - SPY.posZ > -PCHARGER_CHARGE_DROP) {
          int dSpy = ApproxDist2D(&actor->posX, &((SpyroObj *)(base - 8))->posX);
          int dPost = ApproxDist2D(&actor->posX,
                                    &st->path->nodes[st->path->cur].x);
          int ok;

          if (st->near) {
            ok = dSpy < dPost + PCHARGER_NEAR_CHARGE_LEAD && dSpy < PCHARGER_NEAR_CHARGE_RANGE;
          } else {
            ok = dSpy < dPost + PCHARGER_CHARGE_LEAD && dSpy < PCHARGER_CHARGE_RANGE;
          }

          if (ok &&
              AbsAngleDelta8(
                  ArcTan2(spy->posX - actor->posX, spy->posY - actor->posY,
                                ATAN_ANGLE8),
                  ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                st->path->nodes[st->path->cur].y - actor->posY,
                                ATAN_ANGLE8)) < PCHARGER_CHARGE_FACING) {
            actor->subMode = 0;
            ENTER_POSE(actor, PCHARGER_CHARGE);
          }
        }
        break;
      }

      case PCHARGER_KNOCKED:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, PCHARGER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case PCHARGER_CHARGE:
        st->afterCharge = 1;
        if (actor->subMode == 0) {
          int dist = ApproxDist2D(&actor->posX, &spy->posX);
          int ang;

          if (!TrackEntityYawTowardSpyro(actor, 5, 0x14, YAW_STEP_IN_BAND)) {
            continue;
          }
          if (MoveActorAlongHeading(actor, PCHARGER_CHARGE_SPEED, 0, PCHARGER_MOVE_RADIUS, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
            st->timer = PCHARGER_COOLDOWN_TICKS;
            ENTER_POSE(actor, PCHARGER_PATROL);
          }
          if (dist < PCHARGER_BUTT_RANGE) {
            actor->subMode++;
          }
          if (FindNearestPathPointToActor(actor, st->path, idx) > PCHARGER_BUTT_RANGE) {
            ang = ArcTan2(st->path->nodes[idx[0]].x - actor->posX,
                                st->path->nodes[idx[0]].y - actor->posY, ATAN_ANGLE8);
            if (AbsAngleDelta8(
                    ang, ArcTan2(st->path->nodes[one - idx[0]].x - actor->posX,
                                       st->path->nodes[one - idx[0]].y - actor->posY,
                                       ATAN_ANGLE8)) < PCHARGER_CHARGE_FACING &&
                AbsAngleDelta8(ang, ArcTan2(spy->posX - actor->posX,
                                                 SPY.posY - actor->posY, ATAN_ANGLE8)) <
                    PCHARGER_CHARGE_FACING) {
              st->timer = PCHARGER_COOLDOWN_TICKS;
              ENTER_POSE(actor, PCHARGER_PATROL);
            }
          }
        } else {
          ANIM_GO(actor, PCHARGER_ANIM_BUTT);
          if (g_nActorAnimNewFrame != 0 && actor->frame == PCHARGER_BUTT_FRAME) {
            PathData *p = st->path;
            int prev = p->count - 1;

            SubtractVector(svR, &p->nodes[p->cur].x,
                          &p->nodes[(p->cur + prev) % p->count].x);
            ScaleVector3ByRatio(svR, VectorLength(svR, VEC_LEN_XY), PCHARGER_BUTT_PUSH);
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FORCE_STATE);
            g_nSpyroRequestVelocityZ = PCHARGER_BUTT_PUSH_VZ;
            g_nSpyroRequestState = SPYRO_ST_FALL;
            g_anSpyroRequestVelocity.x = svR[0];
            g_nSpyroRequestVelocityY = svR[1];
            if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
              g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
            }
          }
          if (g_nActorAnimDone != 0) {
            ENTER_POSE(actor, PCHARGER_PATROL);
          }
        }
        break;

      case PCHARGER_TAUNT:
      case PCHARGER_TAUNT_ALT:
        TrackEntityYawTowardSpyro(actor, 5, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, PCHARGER_PATROL);
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

    /* Type 174: the locked chest (LockedChestState). Shakes when hit; with
     * the key held and Spyro facing it up close, the key flies into its lock
     * and the chest bursts and drops its reward. */
    case ACTOR_TYPE_LOCKED_CHEST: {
      LockedChestState *st = actor->state;
      Actor *key = &g_pActorListBase[st->idx];

      if (actor->flags & ACTOR_HIT_SUPER) {
        actor->subMode = LOCKCHEST_OPENING;
        st->shake = LOCKCHEST_SKIP_TICK;
      }

      if (actor->subMode < LOCKCHEST_OPENING) {
        if (st->shake != 0) {
          st->shake += g_nFrameStep;
          if (st->shake < SHAKE_TICKS) {
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
        } else if (actor->flags & ACTOR_HITS_SPYRO) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      switch (actor->subMode) {
      case LOCKCHEST_NO_KEY:
        if (g_nLevelKeyState == KEYSTATE_NONE || g_nLevelKeyState == KEYSTATE_REWARD_TAKEN) {
          g_nLevelKeyState = KEYSTATE_NONE;
          actor->updateDist = sixteen;
          key->updateDist = 0x40;
          key->cullRadius = 0x18;
        } else {
          g_nLevelKeyState = KEYSTATE_CARRIED;
          actor->subMode = LOCKCHEST_KEY_READY;
          key->scale = 0x40;
          key->groundCode = 0;
          key->gateChunk = GATE_CHUNK_ALWAYS;
          key->subMode = KEY_TAKEN;
        }
        break;

      case LOCKCHEST_KEY_READY: {
        int height;
        int drop;

        if (g_nLevelKeyState == KEYSTATE_CARRIED &&
            (SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
             SPY.state == SPYRO_ST_RUN) &&
            ApproxDist2D(&actor->posX, &spy->posX) < LOCKCHEST_KEY_RANGE) {
          height = actor->posZ - actor->floorOffset;
          drop = height - SPY.posZ;
          if ((drop > 0 ? drop < LOCKCHEST_KEY_DZ : (SPY.posZ - height) < LOCKCHEST_KEY_DZ) &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, ATAN_ANGLE8)) < LOCKCHEST_FACING_ANGLE &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8)) < LOCKCHEST_FACING_ANGLE) {
            spy->requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
            ResetSpyroLinearMotion();
            ZeroVector(&spy->headLook);
            actor->subMode = LOCKCHEST_OPENING;
            if (st->shake > 0) {
              st->shake = 0;
              actor->rotX = st->rotX;
              actor->rotY = st->rotY;
              actor->posZ = st->restZ;
            }
            key->rotX = 0;
            key->rotY = 0;
            key->rotZ = spy->bodyRotZ + 0x80;
          }
        }
        break;
      }

      case LOCKCHEST_OPENING:
        if (g_dwPadPressed & PAD_CROSS) {
          st->shake = LOCKCHEST_SKIP_TICK;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        key->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nCinematicActive = 1;

        if (st->shake < LOCKCHEST_INSERT_TICK) {
          svR[0] = LOCKCHEST_LOCK_FWD;
          svR[1] = 0;
          svR[2] = LOCKCHEST_LOCK_UP;
          RotateVectorByMatrix(actor->mtx, svR, svR);
          AddVector(svR, svR, &actor->posX);
          SubtractVector(svR, svR, &spy->posX);
          svR[2] -= 0x100;
          RShiftVector3(svR, 5);
          ScaleVector3Sat(svR, svR, st->shake);
          DivideVector3(svR, svR, 6);
          AddVector(svR, svR, &spy->posX);
          svR[2] += 0x100;
          svR[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, svR);
          g_nSpyroSwingEulerTargetY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

          if (st->shake < LOCKCHEST_KEY_SPIN_TICK) {
            int rot;

            key->rotY = -st->shake;
            rot = g_bSpyroPersistentEulerZ + 0x80;
            key->rotZ = rot + st->shake * 4;
          } else if (st->shake < LOCKCHEST_KEY_TURN_TICK) {
            int rot;

            key->rotY = 0xC0;
            rot = g_bSpyroPersistentEulerZ + 0x80;

            key->rotZ = rot + (st->shake - LOCKCHEST_KEY_SPIN_TICK) * 3;
          } else {
            int turn = AbsAngleDelta8(actor->rotZ,
                                     (unsigned char)(g_bSpyroPersistentEulerZ + 0x40)) *
                       (st->shake - LOCKCHEST_KEY_TURN_TICK);
            int rot = g_bSpyroPersistentEulerZ + 0x40;

            key->rotZ = rot + turn / 64;
          }
        } else if (st->shake < LOCKCHEST_BURST_TICK) {
          key->rotZ = actor->rotZ;
          keyOffset[0] = LOCKCHEST_LOCK_FWD - (st->shake - LOCKCHEST_INSERT_TICK) * 8;
          keyOffset[1] = 0;
          keyOffset[2] = LOCKCHEST_LOCK_UP;
          RotateVectorByMatrix(actor->mtx, keyOffset, keyOffset);
          AddVector(&key->posX, keyOffset, &actor->posX);
        }

        if (st->shake >= LOCKCHEST_BURST_TICK ||
            !(SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
              SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          int i;

          key->cullRadius = ACTOR_CULL_HIDDEN;
          key->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
          key->drawn = 0;
          g_nCinematicActive = 0;
          actor->soundRange = SOUND_RANGE(0x8000);
          PlayActorMeshVoice(actor, LOCKEDCHEST_VOICE_OPEN, 0);

          for (i = 0; i < 4; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
          }

          for (i = 0; i < 10; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          }

          g_pfnLevelEmitParticles(0x20, EMIT_STREAK_BURST, &actor->posX, 0x18);
          g_pLockedChestReward = SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          DespawnActorRecord(actor);
          g_nLevelKeyState = KEYSTATE_CHEST_OPENED;
          g_nSpyroSwingEulerTargetZ = 0;
          g_nSpyroSwingEulerTargetY[0] = 0;
          g_nSpyroSwingEulerTargetX = 0;
        }

        RefreshActorDerivedState(key, ACTOR_REFRESH_MATRIX);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, key, 0x8080);
        } else {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, key, 0x5008080);
        }
        break;
      }

      st->scorchBand = StepActorScorchTint(actor, st->scorchBand);
      actor->flags = 0;
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
          SubtractVector(svK, at, &spy->posX);
          svK[2] = (svK[2] * 3) >> 2;

          if (VectorLength(svK, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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

        svK[0] = (GetRandomU32() & 0xFE) - 0x7F;
        svK[1] = (GetRandomU32() & 0xFE) - 0x7F;
        svK[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(svK, svK, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, svK, 1);
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Type 412: wandering critter; knocked back when hit, then recovers. */
    case ACTOR_TYPE_HOPPING_CRITTER: {
      HopState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != GRAZER_KNOCKED) {
        EBB_BREAK();
        actor->flags = 0;
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->knockSpeed = GRAZER_HOP_FLAME_KNOCK_SPEED;
        } else {
          st->knockSpeed = GRAZER_HOP_KNOCK_SPEED;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, GRAZER_KNOCKED);
      }

      SnapActorToGround(actor);
      EncodeHitNormalToActorGroundCode(actor);

      switch (actor->mode) {
      case GRAZER_WANDER:
        if (TICK_DOWN(st->timer)) {
          ENTER_POSE(actor, GRAZER_PAUSE);
        }
        UpdateActorWander(actor, &st->wander);
        break;
      case GRAZER_PAUSE:
        SnapActorToGround(actor);
        if (g_nActorAnimDone != 0) {
          st->timer = RandomIntInRange(GRAZER_WANDER_MIN, GRAZER_WANDER_MAX);
          ENTER_POSE(actor, GRAZER_WANDER);
        }
        break;
      case GRAZER_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, 0, GRAZER_KNOCK_DRAG, 0);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, GRAZER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = LIFECHEST_OPEN;

          st->passenger = g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_RIDER, actor);
          st->passenger->rotX = actor->rotX;
          st->passenger->rotY = actor->rotY;
          st->passenger->rotZ = actor->rotZ;
          st->passenger->frameRate = 0x10;
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
          st->passenger->cullRadius = 0x10;
          st->passenger->updateDist = ACTOR_UPDATE_ALWAYS;
        }

        if (st->phase >= LIFECHEST_OPEN_FRAMES) {
          DespawnActorRecord(st->passenger);
          st->phase = GetRandomU32() & 0xFF;
          actor->frameProgress = 8;
          actor->frameRate = 8;
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
        ((CollectibleState *)prize->state)->mode = one;
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
        svN[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        svN[1] = (g_anSineLut[i * 32] >> 8) * 3;
        svN[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)svN);
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
          svN[0] = (g_anCosineLut[bearing] * 3) >> 4;
          svN[1] = (g_anSineLut[bearing] * 3) >> 4;
          svN[2] = 0;
          letterSide[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterSide, st->count - 1);
          LShiftVector3(letterSide, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = svN[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = svN[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterSide, letterSide, st->index);
          SubtractVector(letterSlot, letterSlot, letterSide);
          swing = &g_anCosineLut[bearing];
          actor->posX = svN[0] * *swing;
          actor->posY = svN[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, svN);
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
