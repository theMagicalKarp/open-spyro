/* Level25_UpdateActors -- per-frame actor update for level 25 (Dark
 * Passage).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 9,
 * 16, 26 and 31); the level-25 classes are the grabber and its lookout (50,
 * 51), the sentry pair (53, 54), the falling rock (56), the lantern and its
 * flame (122, 124), the flame-lit switch (129), the flying ember (155) and
 * the cannon (402).
 *
 * Load-bearing source forms (matched 2026-09-30):
 *   - Loop-top carriers `oneq` (short) then `one`, each used once (case 9);
 *     every other 1 is a literal, so int sites and compares use the hoisted
 *     fp while byte stores stay inline `li`. 16 is a literal, not a carrier.
 *   - Scratch arrays at function level in frame order (the shared slots
 *     sparkVel and s168..s576, reused across arms).
 *   - Case 402 reads its two gain columns as `g_anSineLut[t * 2 - 6]` and
 *     `[t * 2 - 5]`; two separate symbols compile identically but lose the
 *     reload spill slot that makes the frame 0x2D0.
 *   - Case 50 holds its scratch vector in a pointer local (s0) and reaches
 *     its grab/partner arms by goto, after the drop block.
 *   - The timer and speed selects in cases 51, 53 and 54 are if/else
 *     around the store; the range tests in cases 51 and 53 are written
 *     positively (ABS2 form).
 *   - Case 110's first state is written like case 331's (`< 0xA01 -> break;
 *     mode = 1; continue`) so the two call tails cross-jump.
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
#include "hud_counter.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. The
 * clones that need a loop-top carrier for 0x10 declare `int sixteen`; here
 * it is the plain constant, so arm bodies read the same in every level. */
#define ANIM_RATE16 sixteen
#define sixteen 0x10

#define spy (&SPY)

void Level25_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  short oneq;
  int one;
  char *base;
  int dt;

  /* Scratch vectors, in frame order. */
  int svA[3];
  int eggArcBase[3];
  int eggArcOffset[3];
  int svD[3];
  int eggViewPos[3];
  int prevPos[3];
  int sparkVel[3];
  int s168[3];
  int groundProbe[3];
  int acc[3][3];
  int bouncePos[3];
  int bounceNormal[3];
  int homingStep[3];
  int sparxTargetPos[3];
  int sparxToTarget[3];
  int attachA[3];
  int attachB[3];
  int fx[6];
  int col[3];
  int svW[3];
  int sparkLead[3];
  int v250[3];
  int sparkleTwinkle[3];
  int shardTwinkle[3];
  int ps[6];
  int s496[3];
  int s512[3];
  int podSparkVel[3];
  int smokeVel[3];
  int ballToNode[3];
  int s576[3];
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
        char *base = (char *)&g_nSpyroWorldPosZ;
        int top = *(int *)base;
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
        SteerActorAroundPoint(actor, &SPY.posX, BUTTERFLY_CIRCLE_RADIUS,
                      WrapAddAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, ATAN_ANGLE8),
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

          CopyVector(svA, &SPY.posX);
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
          CopyVector(svD, &SPY.posX);
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

    /* Type 50: grabber. Seizes Spyro when he wanders close in front of it,
     * holds him under a scripted camera, then walks back to its post. */
    case ACTOR_TYPE_POST_GRABBER: {
      PostGrabberState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != PGRAB_KNOCKED && actor->mode != PGRAB_GRAB) {
        if (*st->slot == actor) {
          L25_DROP(st);
        }
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, PGRAB_KNOCKED);
      }
      actor->flags = 0;
      if (st->link >= 0) {
        if (actor->mode != PGRAB_STAND_DOWN && actor->mode != PGRAB_KNOCKED &&
            g_pActorListBase[st->link].subMode == (LANTERN_F_LIT | LANTERN_F_SETTLED)) {
          if (*st->slot == actor) {
            L25_DROP(st);
          }
          ENTER_POSE(actor, PGRAB_STAND_DOWN);
        }
      } else {
        actor->type = ACTOR_TYPE_GRABBER_LOOKOUT;
        actor->mode = LOOKOUT_WAIT;
        ANIM_RESET(actor, PGRAB_WAIT);
        continue;
      }

      switch (actor->mode) {
      case PGRAB_WAIT:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (*st->slot == actor && TICK_DOWN(st->hold)) {
          L25_DROP(st);
        }
        if (TICK_DOWN(st->timer) == 0) {
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= PGRAB_NOTICE_RANGE) {
          break;
        }
        if (ABS(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ + SPYRO_RADIUS) >= PGRAB_GRAB_DZ) {
          break;
        }
        if (g_nSpyroAirborneFrames != 0 || g_nSpyroState == SPYRO_ST_CHARGE || g_nSpyroState == SPYRO_ST_CHARGE_AIR) {
          break;
        }
        if (*st->slot != 0) {
          break;
        }
        if (AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8)) >= PGRAB_FACING) {
          break;
        }
        actor->subMode = 0;
        *st->slot = actor;
        st->hold = PGRAB_HOLD_TICKS;
        st->timer = PGRAB_ENGAGE_TICKS;
        ENTER_POSE(actor, PGRAB_CHASE);

      case PGRAB_GRAB:
        if (g_nActorAnimDone != 0) {
          actor->subMode = 1;
          L25_UNLINK(st);
          st->timer = PGRAB_RETURN_TICKS;
          ENTER_POSE(actor, PGRAB_CHASE);
        }
        if (g_nSpyroDrawSuppressed != 0 && actor->nextFrame >= PGRAB_RELEASE_FRAME) {
          SubtractVector((int *)&g_anSpyroRequestVelocity, st->grip, &actor->posX);
          RShiftVector3((int *)&g_anSpyroRequestVelocity, 4);
          g_dwSpyroTriggerEventFlags = SPYRO_EV_KNOCKBACK_2;
          g_nSpyroDrawSuppressed = 0;
          actor->subMode = 1;
          break;
        }
        if (actor->subMode != 0) {
          break;
        }
        if (actor->nextFrame >= PGRAB_PULL_FRAME) {
          if (g_nSpyroDrawSuppressed == 0) {
            st->grip[0] = PGRAB_GRIP_FWD;
            st->grip[1] = 0;
            st->grip[2] = PGRAB_GRIP_UP;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
            RotateVectorByMatrix(actor->mtx, st->grip, st->grip);
            AddVector(st->grip, &actor->posX, st->grip);
          }
          {
            int *v = svD;

            SubtractVector(v, st->grip, &SPY.posX);
            ScaleVector3Sat(v, v, ((actor->nextFrame - PGRAB_PULL_FRAME) << 12) / PGRAB_PULL_FRAMES);
            RShiftVector3(v, 0xC);
            AddVector(&SPY.requestTargetX, &SPY.posX, v);
          }
          g_nSpyroRequestState = SPYRO_ST_IDLE;
          g_nSpyroDrawSuppressed = 1;
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_NO_CLIP);
        } else {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT);
          g_nSpyroRequestState = SPYRO_ST_SCRIPTED_HOLD;
        }
        break;

      case PGRAB_CHASE:
        if (actor->subMode != 0) {
          int heading;
          int dist;

          if (*st->slot == actor && TICK_DOWN(st->hold)) {
            L25_DROP(st);
          }
          heading = ArcTan2(st->homeX - actor->posX,
                                  st->homeY - actor->posY, ATAN_ANGLE8);
          dist = ApproxDist2D(&actor->posX, &st->homeX);
          if (TICK_DOWN(st->timer) || dist < PGRAB_HOME_RANGE) {
            st->timer = PGRAB_ENGAGE_TICKS;
            ENTER_POSE(actor, PGRAB_WAIT);
          }
          if (dist > PGRAB_HOME_SPEED) {
            dist = PGRAB_HOME_SPEED;
          }
          if (TurnActorYawTowardAngle(actor, heading, 4, 0x18, YAW_STEP_IN_BAND) == 0) {
            break;
          }
          MoveActorAlongHeading(actor, dist, PGRAB_MOVE_RADIUS, PGRAB_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE);
          break;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        MoveActorAlongAngle(actor,
                      ArcTan2(SPY.posX - actor->posX,
                                    SPY.posY - actor->posY, ATAN_ANGLE8),
                      PGRAB_CHASE_SPEED, PGRAB_MOVE_RADIUS, PGRAB_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE);
        if (TICK_DOWN(st->timer) == 0) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= PGRAB_GRAB_RANGE) {
            break;
          }
          if (ABS(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ + SPYRO_RADIUS) >= PGRAB_GRAB_DZ) {
            break;
          }
          actor->subMode = 0;
          if (st->link < 0) {
            goto pose1;
          }
          if (g_pActorListBase[st->link].subMode & LANTERN_F_SETTLED) {
            goto grab;
          }
        }
        L25_DROP(st);
        actor->subMode = 1;
        st->timer = PGRAB_RETURN_TICKS;
        break;
      grab:
        g_pActorListBase[st->link].subMode |= LANTERN_F_HELD;
        ENTER_POSE(actor, PGRAB_GRAB);
      pose1:
        ENTER_POSE(actor, PGRAB_GRAB);

      case PGRAB_STAND_DOWN:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->type = ACTOR_TYPE_GRABBER_LOOKOUT;
        actor->subMode = 1;
        st->timer = PGRAB_RETURN_TICKS;
        actor->mode = LOOKOUT_CHASE;
        ANIM_RESET(actor, PGRAB_GRAB);
        continue;

      case PGRAB_EMERGE: {
        int reach = (actor->nextFrame << 7) + PGRAB_EMERGE_REACH;

        if (reach > PGRAB_MOVE_RADIUS) {
          reach = PGRAB_MOVE_RADIUS;
        }
        DispatchActorContactAtSphere(&actor->posX, reach, 0, 0, actor, CONTACT_PUSH_OUT);
        if (g_nActorAnimDone == 0) {
          break;
        }
        st->timer = 0;
        ANIM_QUEUE(actor, PGRAB_WAIT);
      }

      case PGRAB_KNOCKED:
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        BurstActorWithSound(actor, PGRAB_BURST_SPARK_LIFE);
        DespawnActorRecord(actor);
        continue;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 51: lookout. Shares the grabber's slot; bolts when struck and
     * turns into type 50 once its partner is roused. */
    case ACTOR_TYPE_GRABBER_LOOKOUT: {
      PostGrabberState *st = actor->state;

      if (actor->flags & ACTOR_HITS_SPYRO) {
        if (actor->mode != LOOKOUT_KNOCKED) {
          if (*st->slot == actor) {
            *st->slot = 0;
          }
          if (actor->flags & ACTOR_HIT_FLAME) {
            st->fleeSpeed = LOOKOUT_FLAME_KNOCK_SPEED;
          } else {
            st->fleeSpeed = LOOKOUT_KNOCK_SPEED;
          }
          st->fleeZSpeed = 0;
          st->fleeDir = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                                    actor->posY - SPY.posY, ATAN_ANGLE8),
                                      g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorLiveKill(actor);
          actor->mode = LOOKOUT_KNOCKED;
          actor->flags = 0;
          if (actor->nextAnim != LOOKOUT_KNOCKED) {
            ANIM_ADVANCE(actor, LOOKOUT_KNOCKED);
          }
          continue;
        }
      } else if (actor->mode != LOOKOUT_KNOCKED && st->link >= 0 &&
                 g_pActorListBase[st->link].subMode == LANTERN_F_SETTLED) {
        if (*st->slot == actor) {
          *st->slot = 0;
        }
        actor->type = ACTOR_TYPE_POST_GRABBER;
        actor->mode = PGRAB_EMERGE;
        ANIM_RESET(actor, PGRAB_EMERGE);
        continue;
      }

      switch (actor->mode) {
      case LOOKOUT_WAIT:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) &&
            ApproxDist2D(&actor->posX, &SPY.posX) < LOOKOUT_NOTICE_RANGE &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < LOOKOUT_NOTICE_DZ &&
            g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR && *st->slot == 0 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8)) < PGRAB_FACING) {
          actor->subMode = 0;
          *st->slot = actor;
          st->timer = PGRAB_ENGAGE_TICKS;
          ENTER_POSE(actor, LOOKOUT_CHASE);
        }
        break;

      case LOOKOUT_CHASE:
        if (actor->subMode != 0) {
          int heading;
          int dist;

          heading = ArcTan2(st->homeX - actor->posX,
                                  st->homeY - actor->posY, ATAN_ANGLE8);
          dist = ApproxDist2D(&actor->posX, &st->homeX);
          if (TICK_DOWN(st->timer) || dist < PGRAB_HOME_RANGE) {
            st->timer = PGRAB_ENGAGE_TICKS;
            ENTER_POSE(actor, LOOKOUT_WAIT);
          }
          if (dist > PGRAB_HOME_SPEED) {
            dist = PGRAB_HOME_SPEED;
          }
          if (TurnActorYawTowardAngle(actor, heading, 4, 0x18, YAW_STEP_IN_BAND) == 0) {
            break;
          }
          MoveActorAlongHeading(actor, dist, LOOKOUT_MOVE_RADIUS, LOOKOUT_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE);
          break;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        MoveActorAlongAngle(actor,
                      ArcTan2(SPY.posX - actor->posX,
                                    SPY.posY - actor->posY, ATAN_ANGLE8),
                      LOOKOUT_CHASE_SPEED, LOOKOUT_MOVE_RADIUS, LOOKOUT_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE);
        if (TICK_DOWN(st->timer)) {
          *st->slot = 0;
          actor->subMode = 1;
          st->timer = PGRAB_RETURN_TICKS;
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < LOOKOUT_SNAP_RANGE &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < LOOKOUT_SNAP_DZ) {
          actor->mode = LOOKOUT_SNAP;
          actor->subMode = 0;
          if (actor->nextAnim != LOOKOUT_SNAP) {
            ANIM_ADVANCE(actor, LOOKOUT_SNAP);
          }
          continue;
        }
        break;

      case LOOKOUT_SNAP:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->subMode = 1;
        *st->slot = 0;
        st->timer = PGRAB_RETURN_TICKS;
        ENTER_POSE(actor, LOOKOUT_CHASE);

      case LOOKOUT_KNOCKED:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LOOKOUT_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        DriveActorTrackedMove(actor, &st->fleeSpeed, st->fleeDir, &st->fleeZSpeed, LOOKOUT_KNOCK_DRAG,
                      LOOKOUT_KNOCK_GRAVITY);
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
      break;
    }

    /* Type 52: bomb-lobbing lizard. Hides until Spyro is in range, then
     * lobs type 55 bombs at him; knocked away when hit. */
    case ACTOR_TYPE_BOMB_LOBBER: {
      BombLobberState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != BLOBBER_KNOCKED) {
        st->knockSpeed = BLOBBER_KNOCK_SPEED;
        st->knockHeading = SPY.bodyRotZ;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, BLOBBER_KNOCKED);
      }

      switch (actor->mode) {
      case BLOBBER_WATCH:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && actor->drawn != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < BLOBBER_LOB_RANGE) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < BLOBBER_LOB_DZ
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < BLOBBER_LOB_DZ) {
            ENTER_POSE(actor, BLOBBER_LOB);
          }
        }
        break;

      case BLOBBER_LOB:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, BLOBBER_WATCH);
        }
        if (actor->nextFrame >= BLOBBER_LOB_FRAME && st->timer == 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          g_pfnLevelEmitParticles(0x10, EMIT_ACTOR_RING_SPARK, g_pfnLevelSpawnActor(ACTOR_TYPE_LOBBED_BOMB, actor), 0x10);
          st->timer = st->reload;
        }
        break;

      case BLOBBER_KNOCKED:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, BLOBBER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        if (st->knockSpeed > 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, BLOBBER_KNOCK_CONTACT, BLOBBER_MOVE_RADIUS, MOVE_LIFT);
          st->knockSpeed -= BLOBBER_KNOCK_DRAG;
        }
        break;
      }
      break;
    }

    /* Type 53: sentry. Watches for Spyro in front of it and raises the
     * alarm; turns into type 54 once its partner is roused. */
    case ACTOR_TYPE_ALARM_SENTRY: {
      AlarmSentryState *st = actor->state;

      TickActorFlameGlow(actor);
      actor->flags = 0;
      if (st->link >= 0) {
        if (actor->mode != ASENTRY_ROUSE && g_pActorListBase[st->link].subMode == (LANTERN_F_LIT | LANTERN_F_SETTLED)) {
          ENTER_POSE(actor, ASENTRY_ROUSE);
        }
      } else {
        actor->type = ACTOR_TYPE_ROUSED_SENTRY;
        actor->mode = RSENTRY_WATCH;
        ANIM_RESET(actor, ASENTRY_WATCH);
        continue;
      }

      switch (actor->mode) {
      case ASENTRY_WATCH:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (st->timer != 0) {
          if (actor->subMode < 2 && g_nSpyroState == SPYRO_ST_HURT) {
            st->timer += 0x3C;
            actor->subMode = 2;
          }
          if (TICK_DOWN(st->timer) == 0) {
            break;
          }
          if (actor->drawn == 0) {
            break;
          }
          st->timer = 1;
          ENTER_POSE(actor, ASENTRY_FIRE);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x800 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8)) < 0x18) {
          if (actor->subMode) {
            st->timer = 0x2C;
          } else {
            st->timer = 0x1A;
          }
          actor->subMode = 1;
          break;
        }
        actor->subMode = 0;
        break;

      case ASENTRY_FIRE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, ASENTRY_WATCH);
        }
        if (actor->nextFrame < 5) {
          break;
        }
        if (st->timer == 0) {
          break;
        }
        {
          Actor *shot;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          shot = g_pfnLevelSpawnActor(ACTOR_TYPE_FLYING_EMBER, actor);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xC00) {
            CopyVector(&shot->posX, &SPY.posX);
          }
        }
        st->timer = 0;
        break;

      case ASENTRY_ROUSE:
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->type = ACTOR_TYPE_ROUSED_SENTRY;
        actor->mode = RSENTRY_WATCH;
        ANIM_RESET(actor, ASENTRY_WATCH);
        continue;

      case ASENTRY_SETTLE:
        if (g_nActorAnimDone == 0) {
          break;
        }
        ANIM_QUEUE(actor, ASENTRY_WATCH);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 54: roused sentry. Backs away when struck; otherwise watches and
     * fires like type 53. */
    case ACTOR_TYPE_ROUSED_SENTRY: {
      AlarmSentryState *st = actor->state;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode < RSENTRY_KNOCKED) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        st->speed = 0x190;
        st->zSpeed = 0x8C;
        st->dir = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                              actor->posY - SPY.posY, ATAN_ANGLE8),
                                g_bSpyroPersistentEulerZ, 0x28, 0x80);
        ENTER_POSE(actor, RSENTRY_KNOCKED);
      }
      actor->flags = 0;
      if (actor->mode < RSENTRY_KNOCKED && st->link >= 0 &&
          g_pActorListBase[st->link].subMode == LANTERN_F_SETTLED) {
        actor->type = ACTOR_TYPE_ALARM_SENTRY;
        actor->mode = ASENTRY_SETTLE;
        ANIM_RESET(actor, RSENTRY_FALL);
        continue;
      }

      switch (actor->mode) {
      case RSENTRY_WATCH:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (st->timer != 0) {
          if (actor->subMode < 2 && g_nSpyroState == SPYRO_ST_HURT) {
            st->timer += 0x3C;
            actor->subMode = 2;
          }
          if (TICK_DOWN(st->timer) == 0) {
            break;
          }
          if (actor->drawn == 0) {
            break;
          }
          st->timer = 1;
          ENTER_POSE(actor, RSENTRY_FIRE);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 &&
            ABS2(actor->posZ - actor->floorOffset - g_nSpyroWorldPosZ) < 0x800 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8)) < 0x18) {
          if (actor->subMode) {
            st->timer = 0x2C;
          } else {
            st->timer = 0x1A;
          }
          actor->subMode = 1;
          break;
        }
        actor->subMode = 0;
        break;

      case RSENTRY_FIRE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, RSENTRY_WATCH);
        }
        if (actor->nextFrame < 2) {
          break;
        }
        if (st->timer == 0) {
          break;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        g_pfnLevelSpawnActor(ACTOR_TYPE_FLYING_EMBER, actor);
        st->timer = 0;
        break;

      case RSENTRY_KNOCKED:
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, RSENTRY_FALL);
        }
        actor->rotY += 4;
        TurnActorYawTowardAngle(actor, (st->dir + 0x80) & 0xFF, 0x10, 0x20, YAW_STEP_IN_BAND);
        DriveActorTrackedMove(actor, &st->speed, st->dir, &st->zSpeed, 0xC, 0xC);
        break;

      case RSENTRY_FALL:
        if (DriveActorTrackedMove(actor, &st->speed, st->dir, &st->zSpeed, 0xC, 0x10) ==
            MOVED_LANDED) {
          ENTER_POSE(actor, RSENTRY_BURST);
        }
        TurnActorYawTowardAngle(actor, (st->dir + 0x80) & 0xFF, 0x10, 0x20, YAW_STEP_IN_BAND);
        break;

      case RSENTRY_BURST:
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, RSENTRY_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 55: lobbed bomb. Bursts on landing, on a timer or against
     * Spyro, then plays its burst and is removed. */
    case ACTOR_TYPE_LOBBED_BOMB: {
      int *life = actor->state; /* frames left; hits other actors once under 40 frames out */

      switch (actor->mode) {
      case LOBBOMB_FLY:
        if (actor->posX < WORLD_MIN_COORD || actor->posY < WORLD_MIN_COORD ||
            actor->posZ < WORLD_MIN_COORD) {
          actor->mode = LOBBOMB_BURST;
          break;
        }
        CopyVector(svA, &actor->posX);
        if (*life < 0x64) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, ACTOR_HIT_CHARGE, actor, CONTACT_STOP_AT_SOLID);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, SPYRO_EV_HIT | SPYRO_EV_CHARGE_HIT) != 0 ||
            TICK_DOWN(*life) != 0 ||
            TryMoveActorForward(actor, 0xE0, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(svA, &actor->posX) != 0) {
          actor->mode = LOBBOMB_BURST;
        }
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_RING_SPARK, actor, 8);
        break;

      case LOBBOMB_BURST:
        g_pfnLevelEmitParticles(0x10, EMIT_ACTOR_RING_SPARK, actor, 0x10);
        DespawnActorRecord(actor);
        continue;
      }
      break;
    }

    /* Type 56: falling rock. Drops until it lands, hits Spyro or times
     * out, then shatters. */
    case ACTOR_TYPE_FALLING_ROCK: {
      int *life = actor->state; /* frames left; hits other actors once under 40 frames out */

      switch (actor->mode) {
      case SHOT_FLY:
        CopyVector(prevPos, &actor->posX);
        if (*life < 0xC8) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, ACTOR_HIT_CHARGE, actor, CONTACT_STOP_AT_SOLID);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, SPYRO_EV_HIT | SPYRO_EV_CHARGE_HIT) || TICK_DOWN(*life) ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) ||
            CastRayWorldAndActors(prevPos, &actor->posX)) {
          actor->mode = SHOT_BURST;
          ANIM_RESET(actor, SHOT_BURST);
        }
        continue;

      case SHOT_BURST:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
        }
        continue;
      }
      continue;
    }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      /* Removed as soon as the animation finishes. */
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

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
        sparkVel[0] = GetRandomU32() & 3;
        sparkVel[1] = GetRandomU32() & 3;
        sparkVel[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)sparkVel);
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
          CopyVector(s168, &actor->posX);
          s168[2] += GROUND_PROBE_LIFT;
          FindGroundHeightBelow(s168, GROUND_PROBE_FAR);
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
            CopyVector(groundProbe, &actor->posX);
            groundProbe[2] += GROUND_PROBE_LIFT;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(groundProbe, 0x1000) > 0) {
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

          SubtractVector(s168, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (s168[0] * cosDelta) / totalWeight;
          actor->posY += ((s168[1] * cosDelta) / totalWeight);
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
        s168[0] = 0;
        s168[1] = SPARX_WING_SPAN;
        s168[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], s168, s168);
        AddVector(s168, s168, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, s168, 0);
        s168[0] = 0;
        s168[1] = -SPARX_WING_SPAN;
        s168[2] = 0;
        ApplyActiveGteRotation(s168, s168);
        AddVector(s168, s168, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, s168, 0);
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

            CopyVector(sparxTargetPos, &st->target->posX);

            if (high == 0) {
              sparxTargetPos[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              sparxTargetPos[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              sparxTargetPos[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              sparxTargetPos[1] -=
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

            SubtractVector(sparxTargetPos, sparxTargetPos, &actor->posX);
            dist = VectorLength(sparxTargetPos, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(sparxTargetPos, dist, limit);
            } else {
              ScaleVector3ByRatio(sparxTargetPos, dist, reach - SPARX_CHASE_STEP_SLACK);
            }
            AddVector(&actor->posX, &actor->posX, sparxTargetPos);
            TurnActorYawTowardAngle(actor, ArcTan2(sparxTargetPos[0], sparxTargetPos[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(sparxTargetPos, VEC_LEN_XY), sparxTargetPos[2], ATAN_ANGLE8);
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
            s168[0] = -SPARX_HOVER_BEHIND;
            s168[1] = (GetRandomU32() & 0x310) - 0x188;
            s168[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + SPARX_CHARGE_HOVER_MIN_Z
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = s168[0];
            st->drift[1] = s168[1];
            st->drift[2] = s168[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              s168[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              s168[1] = st->drift[1];
              s168[2] = st->drift[2];
              if (s168[2] < SPARX_CHARGE_HOVER_MIN_Z) {
                s168[2] = SPARX_CHARGE_HOVER_MIN_Z;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                s168[0] = st->drift[0] - SPARX_FREE_LOOK_PULLBACK;
              } else {
                s168[0] = st->drift[0];
              }
              s168[1] = st->drift[1];
              s168[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, s168, s168);
            AddVector(s168, s168, &SPY.posX);
            at = &actor->posX;
            SubtractVector(s168, s168, at);
            RShiftVector3(s168, 2);
            AddVector(at, at, s168);

            if (CollideSphereWithWorldAndActors(at, SPARX_COLLIDE_RADIUS, SPARX_COLLIDE_RADIUS) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (s168[2] > SPARX_MAX_PITCH) {
              s168[2] = SPARX_MAX_PITCH;
            }
            if (s168[2] < -SPARX_MAX_PITCH) {
              s168[2] = -SPARX_MAX_PITCH;
            }

            actor->rotX = 0;
            actor->rotY = s168[2];
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

    /* Type 122: lantern. Lit and snuffed by Spyro; the glow node grows and
     * fades with it. */
    case ACTOR_TYPE_LANTERN: {
      LanternState *st = actor->state;

      if (actor->flags & ACTOR_HITS_SPYRO) {
        actor->flags = 0;
        actor->subMode |= LANTERN_F_LIT | LANTERN_F_SETTLED;
        if (actor->mode < LANTERN_STRUCK) {
          g_pfnLevelSpawnActor(ACTOR_TYPE_LANTERN_FLAME, actor);
          actor->mode = LANTERN_STRUCK;
          ANIM_RESET(actor, LANTERN_STRUCK);
          continue;
        }
      }
      actor->flags = 0;
      if (actor->subMode & LANTERN_F_LIT) {
        if (st->fx == 0) {
          FxNode *g = AllocWorldSparkle();

          st->fx = g;
          if (g != 0) {
            g->anchor = st->pos;
            st->fx->size = 0x40;
            st->fx->offX = 0;
            st->fx->offY = 0;
            st->fx->offZ = 0;
            st->fx->r = 0xC0;
            st->fx->g = 0xC0;
            st->fx->b = 0x60;
            st->fx->sizeBias = -8;
          }
        } else {
          st->fx->size += 0x20;
          if (st->fx->size > 0x800) {
            st->fx->size = 0x800;
          }
          if (st->chunkAnim >= 0 && st->fx->size >= 0x300 &&
              CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim)) < 0xB) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
      } else if (st->fx != 0) {
        st->fx->size -= 0x40;
        if (st->fx->size <= 0x400 && st->chunkAnim >= 0 &&
            CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim)) > 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        if (st->fx->size <= 0) {
          ClearWord(st->fx);
          st->fx = 0;
        }
      }
      if (st->fx != 0) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, attachA);
        GetActorAttachPoint(actor, 1, attachB);
        AddVector(st->pos, attachA, attachB);
        RShiftVector3(st->pos, 1);
      }

      switch (actor->mode) {
      case LANTERN_IDLE:
        if (st->held == 0) {
          if (TICK_DOWN(st->timer) == 0) {
            break;
          }
          if (actor->subMode & LANTERN_F_HELD) {
            break;
          }
          st->timer = 0x78;
          actor->subMode &= ~LANTERN_F_SETTLED;
          if (actor->subMode & LANTERN_F_LIT) {
            if (st->chunkAnim >= 0) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            }
            ENTER_POSE(actor, LANTERN_SNUFF);
          }
          ENTER_POSE(actor, LANTERN_LIGHT);
        }
        actor->subMode |= LANTERN_F_SETTLED;
        break;

      case LANTERN_SNUFF:
        actor->subMode &= ~LANTERN_F_LIT;
        if (g_nActorAnimDone == 0) {
          break;
        }
        actor->subMode |= LANTERN_F_SETTLED;
        ENTER_POSE(actor, LANTERN_IDLE);

      case LANTERN_LIGHT:
        actor->subMode |= LANTERN_F_LIT;
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->chunkAnim >= 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        actor->subMode |= LANTERN_F_SETTLED;
        ENTER_POSE(actor, LANTERN_IDLE);

      case LANTERN_STRUCK:
        if (g_nActorAnimDone == 0) {
          break;
        }
        st->timer = 0x1C2;
        ANIM_QUEUE(actor, LANTERN_HOLD_LIT);

      case LANTERN_HOLD_LIT:
        if (TICK_DOWN(st->timer) == 0) {
          break;
        }
        {
          Actor *flame = g_pfnLevelSpawnActor(ACTOR_TYPE_LANTERN_FLAME, actor);

          actor->mode = LANTERN_DONE;
          flame->mode = LFLAME_CLOSE;
          ANIM_RESET(flame, LFLAME_CLOSE);
        }
        continue;

      case LANTERN_DONE:
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 124: lantern flame. Removes itself when its animation ends,
     * restarting the lantern's idle animation the second time. */
    case ACTOR_TYPE_LANTERN_FLAME: {
      Actor **st = actor->state;

      if (actor->mode == LFLAME_STRIKE) {
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
        }
        continue;
      }
      if (actor->mode == LFLAME_CLOSE && g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        (*st)->mode = LANTERN_IDLE;
        ANIM_RESET(*st, LANTERN_IDLE);
      }
      continue;
    }

    /* Charge-reactive prop: a charge plays its reaction animation once. */
    case ACTOR_TYPE_CHARGE_REACT_PROP:
      switch (actor->mode) {
      case CREACT_IDLE:
        if (!(actor->flags & ACTOR_HIT_CHARGE)) {
          continue;
        }
        ENTER_POSE(actor, CREACT_REACT);

      case CREACT_REACT:
        if (g_nActorAnimDone == 0) {
          continue;
        }
        actor->flags = 0;
        ENTER_POSE(actor, CREACT_IDLE);
      }
      continue;

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

    /* Type 155: flying ember. Rides the floor, trails sparks, and bursts
     * when it lands, hits Spyro, times out or leaves the level. */
    case ACTOR_TYPE_FLYING_EMBER: {
      int *life = actor->state; /* frames left; hits other actors once under 40 frames out */
      int i;

      switch (actor->mode) {
      case SHOT_FLY:
        if (actor->posX < WORLD_MIN_COORD || actor->posY < WORLD_MIN_COORD ||
            actor->posZ < WORLD_MIN_COORD) {
          actor->mode = SHOT_BURST;
          continue;
        }
        {
          int floorZ = FindGroundHeightBelow(&actor->posX, 0x2000);

          if (floorZ != 0) {
            int tilt = actor->posZ - floorZ - 0x200;

            tilt = -tilt >> 2;

            if (tilt > 0x20) {
              tilt = 0x20;
            }
            if (tilt < -0x20) {
              tilt = -0x20;
            }
            actor->rotY = tilt;
          }
        }
        CopyVector(svW, &actor->posX);
        if (*life < 0xC8) {
          DispatchActorContactAtSphere(&actor->posX, 0x64, 0, ACTOR_HIT_CHARGE, actor, CONTACT_STOP_AT_SOLID);
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, SPYRO_EV_HIT | SPYRO_EV_CHARGE_HIT) || TICK_DOWN(*life) ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) ||
            CastRayWorldAndActors(svW, &actor->posX)) {
          actor->mode = SHOT_BURST;
        }
        ZeroVector(svW);
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)svW);
        continue;

      case SHOT_BURST:
        for (i = 0; i < 16; i++) {
          svW[0] = RandomIntInRange(-0x5A, 0x5A);
          svW[1] = RandomIntInRange(-0x5A, 0x5A);
          svW[2] = RandomIntInRange(-0x19, 0x3C);
          CopyVector(sparkLead, svW);
          LShiftVector3(sparkLead, 2);
          AddVector(sparkLead, sparkLead, &actor->posX);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_YELLOW, sparkLead, (int)svW);
        }
        DespawnActorRecord(actor);
        continue;
      }
      continue;
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
          SubtractVector(v250, at, &spy->posX);
          v250[2] = (v250[2] * 3) >> 2;

          if (VectorLength(v250, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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
      SubtractVector(v250, &SPY.posX, &actor->posX);
      distance = VectorLength(v250, VEC_LEN_XY);
      if (distance < st->radius && v250[2] >= -WHIRL_BELOW_REACH && v250[2] < st->spanZ &&
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
          s496[0] = (GetRandomU32() & 7) + 0x10;
          s496[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, ps, (int)s496);
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
          ZeroVector(s496);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)s496);
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
            CopyVector(s512, &st->prize->posX);
            s512[0] += g_anCosineLut[j * 43] >> 6;
            s512[1] += g_anSineLut[j * 43] >> 6;
            s512[2] += 0x28;
            podSparkVel[0] = g_anCosineLut[j * 43] >> 7;
            podSparkVel[1] = g_anSineLut[j * 43] >> 7;
            podSparkVel[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, s512, (int)podSparkVel);
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

    /* Type 401: breakable rock. Shakes when struck; a charge shatters it
     * into rubble and dust and removes it. */
    case ACTOR_TYPE_BREAKABLE_ROCK: {
      BreakableRockState *st = actor->state;

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
      if (actor->flags & ACTOR_HIT_SUPER) {
        int i;

        actor->soundRange = SOUND_RANGE(0x8000);
        PlayActorMeshVoice(actor, BROCK_VOICE_BREAK, 0);
        for (i = 0; i < 4;) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          i++;
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }
        for (i = 0; i < 0xA;) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          i++;
        }
        g_pfnLevelEmitParticles(0x20, EMIT_STREAK_BURST, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->scorchBand = StepActorScorchTint(actor, st->scorchBand);
        if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE)) && st->shake == 0) {
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
    case ACTOR_TYPE_CANNON: {
      CannonState *st = actor->state;

      if (st->init == 1) {
        int pitch;

        SubtractVector(s496, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(s496, VEC_LEN_XY), s496[2], ATAN_ANGLE8);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < CANNON_LOW_PITCH_SPAN) {
          actor->rotY = CANNON_LOW_PITCH;
        }
        actor->rotZ = ArcTan2(s496[0], s496[1], ATAN_ANGLE8);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        st->init = 2;
      }

      switch (actor->subMode) {
      case CANNON_UNLIT:
        if (st->link >= 0 && g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) {
          DespawnActorRecord(actor);
          continue;
        }
        if (!(actor->flags & ACTOR_HIT_FLAME)) {
          continue;
        }
        actor->subMode = CANNON_FUSE;
        actor->updateDist = ACTOR_UPDATE_ALWAYS;
        actor->cullRadius = 0x7F;
        continue;

      case CANNON_FUSE:
        st->timer += g_nFrameStep;
        if (st->timer < CANNON_SMOKE_TICKS) {
          s512[0] = 0;
          s512[1] = 0;
          s512[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, s512, s512);
          AddVector(s512, s512, &actor->posX);
          smokeVel[0] = 0;
          smokeVel[1] = 6;
          smokeVel[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, smokeVel, smokeVel);
          smokeVel[0] += (GetRandomU32() & 0xF) - 8;
          smokeVel[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, s512, (int)smokeVel);
          if (actor->voiceRow != VOICE_ROW_NONE) {
            continue;
          }
          PlayActorMeshVoice(actor, CANNON_VOICE_FUSE, 0);
          continue;
        }
        if (st->timer < CANNON_FIRE_TICK &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          s512[0] = 0;
          s512[1] = 0;
          s512[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, s512, s512);
          AddVector(s512, s512, &actor->posX);
          smokeVel[0] = 0;
          smokeVel[1] = ((st->timer - CANNON_SMOKE_TICKS) >> 3) + 9;
          smokeVel[2] = -0x12 - ((st->timer - CANNON_SMOKE_TICKS) >> 2);
          RotateVectorByMatrix(actor->mtx, smokeVel, smokeVel);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, s512, (int)smokeVel);
          if (actor->rotX < 0x80) {
            actor->rotX = -((st->timer - CANNON_SMOKE_TICKS) >> 5);
          } else {
            actor->rotX = (st->timer - CANNON_SMOKE_TICKS) >> 5;
          }
          continue;
        }
        if (st->timer <= CANNON_FIRE_TICK) {
          continue;
        }
        actor->subMode = CANNON_FLIGHT;
        actor->cullRadius = 0x40;
        actor->updateDist = 0x40;
        st->path->cur = 1;
        st->timer = 0;
        ZeroVector(st->vel);
        PlayActorMeshVoice(actor, CANNON_VOICE_FIRE, 0);
        continue;

      case CANNON_FLIGHT:
        if (st->timer == CANNON_LEG_TICKS) {
          SubtractVector(ballToNode, PATH_PT(st->path, st->path->cur), &actor->posX);
          CopyVector(st->vel, ballToNode);
          st->path->cur++;
          if (st->path->cur == st->path->count) {
            Actor *target = &g_pActorListBase[st->link];

            if (st->link > 0) {
              target->updateDist = ACTOR_UPDATE_ALWAYS;
              target->flags |= ACTOR_HIT_SUPER;
            }
            DespawnActorRecord(actor);
          }
          st->timer = 0;
        } else {
          if (st->timer == 3) {
            SubtractVector(ballToNode, PATH_PT(st->path, st->path->cur), &actor->posX);
            RShiftVector3(ballToNode, 1);
          } else {
            SubtractVector(ballToNode, PATH_PT(st->path, st->path->cur),
                          (int *)((unsigned char *)st->path + ((st->path->cur << 4) - 8)));
            ScaleVector3Sat(ballToNode, ballToNode, g_anSineLut[st->timer * 2 - 6]);
            ScaleVector3Sat(s576, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(ballToNode, ballToNode, s576);
            RShiftVector3(ballToNode, 0xA);
          }
          st->timer++;
        }
        VectorLength(ballToNode, VEC_LEN_XYZ);
        actor->rotZ = ArcTan2_8bit(ballToNode[0], ballToNode[1]);
        actor->rotY = -ArcTan2_8bit(ballToNode[2], VectorLength(ballToNode, VEC_LEN_XY));
        AddVector(&actor->posX, &actor->posX, ballToNode);
        ZeroVector(ballToNode);
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)ballToNode);
        continue;
      }
      continue;
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
        s576[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        s576[1] = (g_anSineLut[i * 32] >> 8) * 3;
        s576[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)s576);
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
        if (ApproxDist2D(&st->sign->posX, &SPY.posX) < LEVELEXIT_ENTER_RANGE) {
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
          bearing = ArcTan2(SPY.posX - st->sign->posX,
                                  SPY.posY - st->sign->posY, ATAN_ANGLE8);
          s576[0] = (g_anCosineLut[bearing] * 3) >> 4;
          s576[1] = (g_anSineLut[bearing] * 3) >> 4;
          s576[2] = 0;
          letterSide[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterSide, st->count - 1);
          LShiftVector3(letterSide, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = s576[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = s576[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterSide, letterSide, st->index);
          SubtractVector(letterSlot, letterSlot, letterSide);
          swing = &g_anCosineLut[bearing];
          actor->posX = s576[0] * *swing;
          actor->posY = s576[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, s576);
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
