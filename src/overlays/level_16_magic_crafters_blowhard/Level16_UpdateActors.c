/* Level16_UpdateActors -- per-frame actor update for level 16 (Blowhard).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (level 0's
 * Level0_UpdateActors, level 2's Level2_UpdateActors, level 4's
 * Level4_UpdateActors, level 30's Level30_UpdateActors); the level-16
 * classes are the rim rider and its bomb and helper (37, 38, 39), the
 * sparking debris (152/153), the path guard (283), the cracking boulder
 * (312), the floor marker (328), the seed pod (329), the checkpoint (333),
 * the boss (495) and the humming bell (497).
 *
 * Load-bearing source forms (matched 2026-09-29):
 *   - No loop-top constant carriers. `one` would take stores of 1 as well as
 *     the compares; without it loop.c hoists 1 into fp for the compares only
 *     and the stores keep their own `li`. 16, 2, 3, 4 and the model and
 *     cosine table addresses are hoisted too, lose allocation and come back
 *     as `li/lui t1` (`sixteen` is a plain 0x10 here).
 *   - Every scratch array is declared at function level, in first-use order;
 *     only case 495's four vectors are block-local (they come last anyway).
 *   - Case 312 declares its debris child pointers once for both debris
 *     loops. Per-loop declarations make them local to one block, local-alloc
 *     then takes s0-s3 there and `actor` loses s3 to s4 function-wide.
 *   - Case 312's shake uses the literal `0x78 - timer` with the ABS2 form of
 *     the height bob (level 0's form); separate dx/dy locals put the index
 *     chain in the wrong register. Case 495's settle test also needs ABS2.
 *   - Case 312 case 1 stores subMode before updateDist/cullRadius (so the `li t1,2`
 *     reload leads the block and reorg shares it with the second branch),
 *     and the `(flags & 0xA0000)` test sits after the common tail (`goto`).
 *   - Case 312's jitter is `+= (rand & 0xF) - 8` and the sum is written
 *     back through `ps[3]`; `x - 8 + r` reassociates differently.
 *   - `tint0` is a word store (`*(int *)&actor->tint0`), written before the
 *     subMode store.
 *   - Case 38's spawn loop starts `for (i = 0, ang = rand & 0x3F; ...)`.
 *   - Case 497 splits its channel switch into `case 3: case 4:` and
 *     `case 5: case 7:` with identical bodies (jump.c merges them); the
 *     compare tree needs 5 as its own node. One `r` serves cases 1 and 2.
 *   - The boss counters are `extern int g_nLevelProgress[]` / `g_nLevelProgressCheckpoint[]`
 *     (held base for the increment and the first two stage reads).
 *   - Case 37's rim offset is `tbl[i] << 16 >> 19` (a plain `>> 3` is
 *     narrowed to lhu/sll/sra); case 9 holds `&g_nSpyroWorldPosZ` in a local.
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

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. */
#define ANIM_RATE16 sixteen

/* The ANIM/ENTER_POSE macros' frame-progress constant. */
#define sixteen 0x10

extern int g_nLevelProgress[]; /* hits taken */
extern int g_nLevelProgressCheckpoint[]; /* stage reached */

void Level16_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  int dt;

  /* Scratch vectors, in first-use order. */
  int svA[3];
  int eggArcBase[3];
  int eggArcOffset[3];
  int svD[3];
  int eggViewPos[3];
  int sparkVel[3];
  int svG[3];
  int bounceNormal[3];
  int acc[3][3];
  int homingStep[3];
  int hitNormal[3];
  int toSpyro[3];
  int sparxTargetPos[3];
  int sparxToTarget[3];
  int fx[6];
  int col[3];
  int svN[3];
  int sparkleTwinkle[3];
  int shardTwinkle[3];
  int ps[6];
  int svQ[3];
  int tri[3][3];
  int svR[3];
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

      if (actor->mode != LEVELEXIT_FAR ? dist < LEVELEXIT_HIDE_RANGE : dist < LEVELEXIT_SHOW_RANGE) {
        if (actor->mode == LEVELEXIT_FAR) {
          actor->posZ += LEVELEXIT_NAME_HEIGHT;
          SpawnNameLetterActors(actor, NAME_LETTERS_RETURN_HOME);
          actor->posZ -= LEVELEXIT_NAME_HEIGHT;
        }
        actor->mode = LEVELEXIT_NEAR;
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
                g_nPortalTransitionActive = 1;
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

    /* Type 37: a rider on its parent's rim. Drifts out toward where Spyro
     * was when it last finished an animation, and spawns its type 39 helper
     * when he comes close. */
    case ACTOR_TYPE_RIM_RIDER: {
      RimState *st = actor->state;

      CopyVector(svD, &actor->posX);
      svD[2] += GROUND_PROBE_LIFT;
      FindGroundHeightBelow(svD, GROUND_PROBE_FAR);
      EncodeHitNormalToActorGroundCode(actor);

      switch (actor->mode) {
      case RIMRIDER_EMERGE:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200 &&
            actor->frame >= 0xF) {
          ENTER_POSE(actor, RIMRIDER_SPAWN);
        }
        actor->posX =
            st->parent->posX + (g_anCosineLut[st->parent->rotZ] << 16 >> 19);
        actor->posY =
            st->parent->posY + (g_anSineLut[st->parent->rotZ] << 16 >> 19);
        TrackEntityYawTowardSpyro(actor, 8, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          st->x = SPY.posX >> 4;
          st->y = SPY.posY >> 4;
          actor->mode = RIMRIDER_DRIFT;
          ANIM_RESET(actor, RIMRIDER_DRIFT);
        }
        break;

      case RIMRIDER_DRIFT:
        switch (st->mode) {
        case RIMDRIFT_TETHERED: {
          int d = ApproxDist2D(&actor->posX, &st->parent->posX);

          if (d > st->range) {
            d = st->range;
          }
          if (d >= st->range) {
            ENTER_POSE(actor, RIMRIDER_STOP);
          }
          d += st->reach;
          actor->posX = st->parent->posX +
                        ((d * g_anCosineLut[st->parent->rotZ]) >> 12);
          actor->posY = st->parent->posY +
                        ((d * g_anSineLut[st->parent->rotZ]) >> 12);
          break;
        }

        case RIMDRIFT_CHASE_HELPER:
          if (st->child == 0) {
            st->child = g_pfnLevelSpawnActor(ACTOR_TYPE_RIM_RIDER_HELPER, actor);
            PlayActorMeshVoice(actor, RIMRIDER_VOICE_SPAWN_HELPER, 0);
          }
        case RIMDRIFT_CHASE: {
          int d;

          if (ApproxDist2D(&actor->posX, &st->parent->posX) > st->range) {
            ENTER_POSE(actor, RIMRIDER_STOP);
          }
          svD[0] = st->x << 4;
          svD[1] = st->y << 4;
          d = ApproxDist2D(&actor->posX, svD);
          if (d > 0x3000) {
            d = 0x3000;
          }
          if (st->reach >> 1 >= d) {
            ENTER_POSE(actor, RIMRIDER_STOP);
          }
          actor->posX += (st->reach * g_anCosineLut[actor->rotZ]) >> 12;
          actor->posY += (st->reach * g_anSineLut[actor->rotZ]) >> 12;
          break;
        }
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x200) {
          ENTER_POSE(actor, RIMRIDER_SPAWN);
        }
        break;

      case RIMRIDER_SINK:
        st->child = 0;
        if (g_nActorAnimDone != 0) {
          ((RimParent *)st->parent->state)->busy = 0;
          DespawnActorRecord(actor);
        }
        break;

      case RIMRIDER_STOP:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, RIMRIDER_SPAWN);
        }
        break;

      case RIMRIDER_SPAWN:
        st->child = g_pfnLevelSpawnActor(ACTOR_TYPE_RIM_RIDER_HELPER, actor);
        PlayActorMeshVoice(actor, RIMRIDER_VOICE_SPAWN_HELPER, 0);
        ENTER_POSE(actor, RIMRIDER_POP);

      case RIMRIDER_POP:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, RIMRIDER_SINK);
        }
        break;
      }
      break;
    }

    /* Split shot: launched at Spyro on the thrower's release frame, it
     * splits into four more of itself when it reaches him, and fizzles out
     * on a timer or against a wall. */
    case ACTOR_TYPE_SPLIT_SHOT: {
      int *timer = actor->state;

      switch (actor->mode) {
      case SPLITSHOT_GATHER:
      case SPLITSHOT_RAISE:
      case SPLITSHOT_FRAGMENT:
        if (g_nActorAnimDone != 0 && TICK_DOWN(*timer) != 0) {
          DespawnActorRecord(actor);
        }
        break;

      case SPLITSHOT_FLY:
        CopyVector(svA, &actor->posX);
        if (actor->posX < SPLITSHOT_MIN_COORD || actor->posY < SPLITSHOT_MIN_COORD ||
            actor->posZ < SPLITSHOT_MIN_COORD) {
          DespawnActorRecord(actor);
          continue;
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, SPLITSHOT_HIT_RADIUS, SPYRO_EV_HIT_TINT | SPYRO_EV_UNREAD_20000) != 0) {
          int i;
          int ang;

          for (i = 0, ang = GetRandomU32() & 0x3F; i < SPLITSHOT_FRAGMENTS; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor)->rotX = ang;
            ang += 0x40;
          }
          DespawnActorRecord(actor);
          continue;
        }
        if (TICK_DOWN(*timer) != 0 ||
            TryMoveActorForward(actor, SPLITSHOT_SPEED, 0, SPLITSHOT_RADIUS, 0) != 0 ||
            CastRayWorldAndActors(svA, &actor->posX) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if ((*timer & 7) == 0) {
          g_pfnLevelEmitParticles(1, EMIT_DRIFT_SPARKLE, &actor->posX, 0x10);
        }
        break;
      }
      break;
    }

    /* Type 39: the rim rider's helper. Follows its owner and faces the
     * camera until the owner lets it go. */
    case ACTOR_TYPE_RIM_RIDER_HELPER: {
      HelperState *st = actor->state;

      if (((RimState *)st->owner->state)->child == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      actor->posX = st->owner->posX;
      actor->posY = st->owner->posY;
      if (TICK_DOWN(st->beat) != 0) {
        PlayActorMeshVoice(actor, RIMHELPER_VOICE_BEAT, 0);
        st->beat = 99999;
      }
      if (actor->mode == 0) {
        actor->rotZ = ArcTan2(CAM.posX - actor->posX,
                                     CAM.posY - actor->posY, ATAN_ANGLE8);
        CopyVector(svA, &st->owner->posX);
        if (TICK_DOWN(st->timer) != 0 || RaiseSpyroTriggerInRadius(svA, 0x140, SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT) != 0) {
          ((RimState *)st->owner->state)->child = 0;
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Explosion shard with a ground bounce (TumbleState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1: {
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

        CopyVector(svG, &actor->posX);
        svG[2] += GROUND_PROBE_LIFT;
        FindGroundHeightBelow(svG, GROUND_PROBE_FAR);
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

            CopyVector(bounceNormal, &actor->posX);
            bounceNormal[2] += GROUND_PROBE_LIFT;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(bounceNormal, 0x1000) > 0) {
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

        ScaleVector3Sat(homingStep, st->vel, g_nFrameStep);
        RShiftVector3(homingStep, 1);

        speed = VectorLength(homingStep, VEC_LEN_XYZ);

        if (COLLECT_BOUNCE_MAX_STEP < speed) {
          ScaleVector3ByRatio(homingStep, speed, COLLECT_BOUNCE_MAX_STEP);
        }

        AddVector(homingStep, &actor->posX, homingStep);

        if (-COLLECT_BOUNCE_MAX_FALL < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * COLLECT_BOUNCE_GRAVITY;
        }

        if (homingStep[2] < 0) {
          if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          homingStep[2] += COLLECT_RADIUS;

          if (CollideSphereWithWorldAndActors(homingStep, COLLECT_RADIUS, COLLECT_RADIUS)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(hitNormal, &g_anCollisionHitNormal);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= COLLECT_RADIUS;

            if (st->bounces == 0) {
              int groundHeight = FindGroundHeightBelow(homingStep, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(hitNormal[2], VectorLength(hitNormal, VEC_LEN_XY));

              if ((homingStep[2] - COLLECT_REST_DROP) < groundHeight && groundAngle < COLLECT_REST_MAX_SLOPE) {
                actor->subMode = COLLECT_REST;
                CopyVector(&actor->posX, homingStep);
                actor->posZ = groundHeight;

                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((hitNormal[0] * hitNormal[0]) + (hitNormal[2] * hitNormal[2])),
                      hitNormal[1]);
                  st->yaw = -ArcTan2_8bit(hitNormal[2], hitNormal[0]);

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

              if (ReflectVectorOffNormal(st->vel, hitNormal, st->vel)) {
                st->bounces--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            homingStep[2] -= 240;
            CopyVector(&actor->posX, homingStep);
            FindGroundHeightBelow(homingStep, GROUND_PROBE_FAR);
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

          SubtractVector(toSpyro, &SPY.posX, st->vel);
          RShiftVector3(toSpyro, 5);

          if (VectorLength(toSpyro, VEC_LEN_XYZ) > COLLECT_HOMING_SNAP_STEP) {
            CopyVector(&actor->posX, &SPY.posX);
            st->timer = COLLECT_HOMING_DONE;
          } else {
            ScaleVector3Sat(toSpyro, toSpyro, st->timer);
            AddVector(&actor->posX, st->vel, toSpyro);
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

    /* Save-prompt host (SaveHostState). */
    case ACTOR_TYPE_SAVE_HOST: {
      SaveHostState *st = (SaveHostState *)actor->state;

      Actor *armer = &g_pActorListBase[st->armer];
      Actor *anchor = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY: {
        actor->collision = 0;
        if (ApproxDist2D(&anchor->posX, &SPY.posX) > SAVEHOST_REARM_RANGE) {
          actor->mode = SAVEHOST_WAIT_ARMED;
        }
        break;
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

          SubtractVector(svG, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (svG[0] * cosDelta) / totalWeight;
          actor->posY += ((svG[1] * cosDelta) / totalWeight);
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
        svG[0] = 0;
        svG[1] = SPARX_WING_SPAN;
        svG[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svG, svG);
        AddVector(svG, svG, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svG, 0);
        svG[0] = 0;
        svG[1] = -SPARX_WING_SPAN;
        svG[2] = 0;
        ApplyActiveGteRotation(svG, svG);
        AddVector(svG, svG, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svG, 0);
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
              ScaleVector3ByRatio(sparxTargetPos, dist, limit);
            } else {
              ScaleVector3ByRatio(sparxTargetPos, dist, reach - 5);
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
            svG[0] = -SPARX_HOVER_BEHIND;
            svG[1] = (GetRandomU32() & 0x310) - 0x188;
            svG[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + SPARX_CHARGE_HOVER_MIN_Z
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = svG[0];
            st->drift[1] = svG[1];
            st->drift[2] = svG[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              svG[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              svG[1] = st->drift[1];
              svG[2] = st->drift[2];
              if (svG[2] < SPARX_CHARGE_HOVER_MIN_Z) {
                svG[2] = SPARX_CHARGE_HOVER_MIN_Z;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                svG[0] = st->drift[0] - SPARX_FREE_LOOK_PULLBACK;
              } else {
                svG[0] = st->drift[0];
              }
              svG[1] = st->drift[1];
              svG[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, svG, svG);
            AddVector(svG, svG, &SPY.posX);
            at = &actor->posX;
            SubtractVector(svG, svG, at);
            RShiftVector3(svG, 2);
            AddVector(at, at, svG);

            if (CollideSphereWithWorldAndActors(at, SPARX_COLLIDE_RADIUS, SPARX_COLLIDE_RADIUS) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (svG[2] > SPARX_MAX_PITCH) {
              svG[2] = SPARX_MAX_PITCH;
            }
            if (svG[2] < -SPARX_MAX_PITCH) {
              svG[2] = -SPARX_MAX_PITCH;
            }

            actor->rotX = 0;
            actor->rotY = svG[2];
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
        if (ApproxDist2D(at, &SPY.posX) < STATUE_NEAR_RANGE) {
          SubtractVector(svN, at, &SPY.posX);
          svN[2] = (svN[2] * 3) >> 2;

          if (VectorLength(svN, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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

    /* Type 283: path guard. Walks its waypoint ring, waits on its partner,
     * rears up (and calls for help) when Spyro comes near, and is knocked
     * away when hit. */
    case ACTOR_TYPE_PATH_GUARD: {
      PathGuardState *st = actor->state;

      if (actor->mode != PATHGUARD_KNOCKED) {
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_WHITE_STREAK, actor, 0);
        if (actor->flags & ACTOR_HITS_SPYRO) {
          st->knockSpeed = PATHGUARD_KNOCK_SPEED;
          st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - SPY.posX,
                                                  actor->posY - SPY.posY, ATAN_ANGLE8),
                                    g_bSpyroPersistentEulerZ, PATHGUARD_KNOCK_BEND_STEP_WIDE, PATHGUARD_KNOCK_BEND_FRAC_WIDE);
          if (actor->flags & ACTOR_HIT_CHARGE) {
            st->knockSpeed += PATHGUARD_CHARGE_KNOCK_BONUS;
          }
          StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorLiveKill(actor);
          ENTER_POSE(actor, PATHGUARD_KNOCKED);
        }
      }

      switch (actor->mode) {
      case PATHGUARD_WALK:
        if (st->flags & PATHGUARD_F_HOLD) {
          st->path->cur = st->node;
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= PATHGUARD_NODE_ARRIVE) {
            int dz;

            TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
            if (ApproxDist2D(&actor->posX, &SPY.posX) < PATHGUARD_THROW_RANGE) {
              dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
              if (dz > 0 ? dz < PATHGUARD_THROW_DZ
                         : SPY.posZ - (actor->posZ - actor->floorOffset) < PATHGUARD_THROW_DZ) {
                st->timer = 0;
                ENTER_POSE(actor, PATHGUARD_THROW);
              }
            }
            if (TICK_DOWN(st->timer) != 0) {
              st->flags &= PATHGUARD_F_RING;
            }
            break;
          }
        } else {
          if (ApproxDist2D(&actor->posX, &SPY.posX) < PATHGUARD_THROW_RANGE) {
            int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

            if (dz > 0 ? dz < PATHGUARD_THROW_DZ
                       : SPY.posZ - (actor->posZ - actor->floorOffset) < PATHGUARD_THROW_DZ) {
              TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
              if (st->timer == 0 || --st->timer <= 0) {
                st->flags |= PATHGUARD_F_HOLD;
                ENTER_POSE(actor, PATHGUARD_THROW);
              }
              break;
            }
          }
          if (st->flags == PATHGUARD_F_RING) {
            PathGuardState *ps = g_pActorListBase[st->partner].state;

            st->path->cur = 0;
            if (st->dist == 0) {
              st->dist = ApproxDist2D(&actor->posX,
                                       &st->path->nodes[st->path->cur].x);
            }
            if (st->dist < ps->dist) {
              st->dist = ps->dist;
            }
            if (ps->heading != 0) {
              st->heading = (ps->heading + 0x80) & 0xFF;
              SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, PATHGUARD_RING_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, PATHGUARD_MOVE_RADIUS,
                            PATHGUARD_WALL_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
              st->heading = 0;
              ps->heading = 0;
            } else {
              st->heading =
                  (ArcTan2(
                       actor->posX - st->path->nodes[st->path->cur].x,
                       actor->posY - st->path->nodes[st->path->cur].y, ATAN_ANGLE8) +
                   st->angle) &
                  0xFF;
              SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, PATHGUARD_RING_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0,
                            MOVE_FOLLOW_FLOOR);
            }
            break;
          }
          st->path->cur = 0;
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= PATHGUARD_NODE_ARRIVE) {
            break;
          }
        }
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8),
            8, 0x10, YAW_STEP_IN_BAND);
        MoveActorAlongHeading(actor, PATHGUARD_WALK_SPEED, PATHGUARD_MOVE_RADIUS, PATHGUARD_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        break;

      case PATHGUARD_THROW:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        if (g_nActorAnimDone != 0) {
          actor->mode = PATHGUARD_WALK;
          ANIM_RESET(actor, PATHGUARD_WALK);
          continue;
        }
        if (st->timer <= 0) {
          st->timer = 1;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        } else if (st->timer < 2 && actor->nextFrame >= 2) {
          st->timer = 2;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        } else if (st->timer < PATHGUARD_THROW_COOLDOWN && actor->nextFrame >= 6) {
          st->timer = PATHGUARD_THROW_COOLDOWN;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        }
        break;

      case PATHGUARD_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, 0, PATHGUARD_KNOCK_DRAG, 0);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, PATHGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
      SubtractVector(svN, &SPY.posX, &actor->posX);
      distance = VectorLength(svN, VEC_LEN_XY);
      if (distance < st->radius && svN[2] >= -WHIRL_BELOW_REACH && svN[2] < st->spanZ &&
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
          g_anCameraActiveTargetParams.w[0] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
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
          svQ[0] = (GetRandomU32() & 7) + 0x10;
          svQ[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, ps, (int)svQ);
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

    /* Type 328: floor-snapped marker. Finds the floor triangle under itself
     * once, then sits at that triangle's centroid. */
    case ACTOR_TYPE_FLOOR_MARKER: {
      int *hitTri = actor->state; /* collision triangle under it, < 0 until found */

      if (*hitTri < 0) {
        int z;
        int d;

        actor->posZ += FLOOR_MARKER_RAISE;
        z = FindGroundHeightBelow(&actor->posX, FLOOR_MARKER_SNAP_RANGE);
        d = actor->posZ - z;
        if (d < 0) {
          d = -d;
        }
        if (d < FLOOR_MARKER_SNAP_RANGE) {
          actor->posZ = z;
          *hitTri = g_nCollisionHitTriIndex;
        } else {
          actor->posZ -= FLOOR_MARKER_RAISE;
        }
        if (*hitTri < 0) {
          break;
        }
      }
      UnpackWorldCollisionTri(*hitTri, tri[0]);
      CopyVector(&actor->posX, tri[0]);
      AddVector(&actor->posX, &actor->posX, tri[1]);
      AddVector(&actor->posX, &actor->posX, tri[2]);
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
          ZeroVector(svQ);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)svQ);
        }
        if ((ApproxDist2D(&st->prize->posX, &SPY.posX) < SEEDPOD_PRIZE_RANGE &&
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
            CopyVector(svR, &st->prize->posX);
            svR[0] += g_anCosineLut[j * 43] >> 6;
            svR[1] += g_anSineLut[j * 43] >> 6;
            svR[2] += 0x28;
            col2[0] = g_anCosineLut[j * 43] >> 7;
            col2[1] = g_anSineLut[j * 43] >> 7;
            col2[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, svR, (int)col2);
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
    case ACTOR_TYPE_CHECKPOINT + 2:
      switch (actor->mode) {
      case CHECKPT_REARM:
        if (ApproxDist2D(&actor->posX, &SPY.posX) > CHECKPT_REARM_RANGE) {
          actor->mode = CHECKPT_ARMED;
        }
        break;

      case CHECKPT_ARMED:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < CHECKPT_TOUCH_RANGE) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < CHECKPT_TOUCH_DZ
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < CHECKPT_TOUCH_DZ) {
            SetSpawnAnchorFromActor(actor, actor->rotZ);
            actor->mode = CHECKPT_FLASH;
            actor->anim = CHECKPT_ANIM_FLASH;
            actor->subMode = 0;
          }
        }
        break;

      case CHECKPT_FLASH:
        actor->subMode += g_nFrameStep;
        if (actor->subMode >= CHECKPT_FLASH_FRAMES) {
          actor->mode = CHECKPT_REARM;
          actor->anim = CHECKPT_ANIM_REST;
        }
        break;
      }
      break;

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      /* Removed as soon as the animation finishes. */
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

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
          svQ[0] = (g_anCosineLut[bearing] * 3) >> 4;
          svQ[1] = (g_anSineLut[bearing] * 3) >> 4;
          svQ[2] = 0;
          svR[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, svR, st->count - 1);
          LShiftVector3(svR, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = svQ[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = svQ[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(svR, svR, st->index);
          SubtractVector(letterSlot, letterSlot, svR);
          swing = &g_anCosineLut[bearing];
          actor->posX = svQ[0] * *swing;
          actor->posY = svQ[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, svQ);
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
    

    /* Type 495: the level boss. Patrols three paths in turn, dropping a
     * type 37 rider on Spyro when he is in front of it; each hit sends it
     * to the next path, and the third ends the fight. */
    case ACTOR_TYPE_BLOWHARD: {
      BlowhardState *st = actor->state;
      int dist = ApproxDist2D(&actor->posX, &SPY.posX);
      int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

      if ((dz > 0 ? dz < BLOWHARD_CAM_DZ
                  : SPY.posZ - (actor->posZ - actor->floorOffset) < BLOWHARD_CAM_DZ) &&
          dist < BLOWHARD_NEAR_RANGE && CheckSpyroLineOfSightTo(&actor->posX) != 0 &&
          g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR && actor->mode != BLOWHARD_P2_TRAVEL &&
          actor->mode != BLOWHARD_P3_TRAVEL) {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FOLLOW_CAMERA);
        g_pScriptedCameraActor = actor;
      }
      if (IsPointInRotatedBox(&SPY.posX, &st->soundBox) != 0) {
        actor->soundRange = SOUND_RANGE(0x4000);
      }
      if (actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) {
        int v[3];

        actor->flags = 0;
        CopyVector(v, &actor->posX);
        v[2] += BLOWHARD_EYE_HEIGHT;
        if (CheckLineOfSightSegment(v, &SPY.posX) != 0) {
          switch (actor->mode) {
          case 5: /* no Blowhard state sets 5 */
          case BLOWHARD_P1_HOVER:
          case BLOWHARD_P1_DROP_RIDER:
          case BLOWHARD_P2_PATROL:
          case BLOWHARD_P2_DROP_RIDER:
          case BLOWHARD_P2_WAIT:
          case BLOWHARD_P3_PATROL:
          case BLOWHARD_P3_DROP_RIDER:
          case BLOWHARD_P3_WAIT:
            g_nLevelProgress[0] += 1;
            if (st->minion != 0) {
              StopSoundVoicesByOwner(st->minion, VOICE_STOP_ALL);
              DespawnActorRecord(st->minion);
              st->minion = 0;
            }
            if (actor->mode == BLOWHARD_P3_PATROL || actor->mode == BLOWHARD_P3_DROP_RIDER ||
                actor->mode == BLOWHARD_P3_WAIT) {
              StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
              ENTER_POSE(actor, BLOWHARD_DYING);
            }
            if (st->prev >= 0) {
              Actor *a = &g_pActorListBase[st->prev];
              int *as = a->state;

              CopyVector(&a->posX, &actor->posX);
              SpawnActorDeathDrop(a, DROP_SCATTER, 0, 0);
              MarkActorLiveKill(a);
              st->prev = *as;
              DespawnActorRecord(a);
            }
            ENTER_POSE(actor, BLOWHARD_HIT);
          }
        }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      CopyVector(svQ, &actor->posX);
      svQ[2] += GROUND_PROBE_LIFT;
      FindGroundHeightBelow(svQ, GROUND_PROBE_FAR);
      EncodeHitNormalToActorGroundCode(actor);
      if (st->floorZ > 0) {
        int z = actor->posZ;
        int d = st->floorZ - z;

        if (ABS2(d) > BLOWHARD_BOB_SETTLE && actor->mode != BLOWHARD_START_PHASE) {
          actor->posZ = z + (d >> 3);
          goto settled;
        }
      }
      st->floorZ = -1;
    settled:
      if (st->started == 0) {
        if (g_nLevelProgressCheckpoint[0] == 0) {
          if (TICK_DOWN(st->delay) != 0) {
            g_nCinematicActive = 0;
            st->started = 1;
          } else {
            g_nCinematicActive = 1;
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          }
        } else {
          Actor *a = &g_pActorListBase[st->prev];
          int *as = a->state;

          st->started = 1;
          if (g_nLevelProgressCheckpoint[0] > 0) {
            actor->mode = BLOWHARD_P2_PATROL;
            st->pathDone = 0;
            st->phase = BLOWHARD_PHASE_2;
            st->path = st->paths[3];
            if (g_pActorListBase[st->prev].dropKind != DROP_GEM_ROLL &&
                g_pActorListBase[st->prev].mode < ACTOR_MODE_DORMANT) {
              SpawnActorDeathDrop(a, DROP_SCATTER, 0, 0);
              MarkActorLiveKill(a);
              DespawnActorRecord(a);
            }
            st->prev = *as;
            ModifyWorldChunkAnimFlags(BLOWHARD_CHUNK_PHASE1, CHUNK_ANIM_RESUME, 0);
            st->chunkAnim = -1;
          }
          if (g_nLevelProgressCheckpoint[0] >= 2) {
            actor->mode = BLOWHARD_P3_PATROL;
            st->path = st->paths[4];
            a = &g_pActorListBase[st->prev];
            as = a->state;
            if (a->dropKind != DROP_GEM_ROLL && a->mode < ACTOR_MODE_DORMANT) {
              SpawnActorDeathDrop(a, DROP_SCATTER, 0, 0);
              MarkActorLiveKill(a);
              DespawnActorRecord(a);
            }
            st->prev = *as;
          }
          CopyVector(&actor->posX, &st->path->nodes[0].x);
          st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
          if (g_nLevelProgressCheckpoint[0] == 3) {
            DespawnActorRecord(actor);
            continue;
          }
        }
      }

      switch (actor->mode) {
      case BLOWHARD_START_PHASE:
        switch (st->phase) {
        case BLOWHARD_PHASE_1:
          actor->mode = BLOWHARD_P1_RISE;
          st->tweenStep = 0;
          break;
        case BLOWHARD_PHASE_2:
          actor->mode = BLOWHARD_P2_TRAVEL;
          st->tweenStep = 0;
          break;
        case BLOWHARD_PHASE_3:
          actor->mode = BLOWHARD_P3_TRAVEL;
          st->tweenStep = 0;
          break;
        }
        break;

      case BLOWHARD_HIT:
        if (g_nActorAnimDone != 0) {
          if (st->chunkAnim >= 0) {
            if (GetWorldChunkAnimState(st->chunkAnim) & CHUNK_ANIM_HALTED) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              actor->mode = BLOWHARD_NEXT_PHASE;
              actor->flags = 0;
              st->chunkAnim = -1;
            }
          } else {
            actor->flags = 0;
            actor->mode = BLOWHARD_NEXT_PHASE;
          }
        }
        break;

      case BLOWHARD_DYING:
        if (g_nActorAnimDone != 0) {
          SnapActorToGround(actor);
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case BLOWHARD_P1_RISE:
        actor->mode = BLOWHARD_P1_HOVER;
        st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
        break;

      case BLOWHARD_P1_HOVER:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        if (dist < BLOWHARD_NEAR_RANGE && st->minion == 0 && st->floorZ == -1) {
          actor->mode = BLOWHARD_P1_DROP_RIDER;
          st->floorZ = actor->posZ - BLOWHARD_BOB_HEIGHT;
          ANIM_GO(actor, BLOWHARD_ANIM_DROP_RIDER);
        }
        break;

      case BLOWHARD_P1_DROP_RIDER:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        if (actor->anim == BLOWHARD_ANIM_DROP_RIDER && actor->frame >= 6 && st->minion == 0) {
          int v[3];

          CopyVector(v, &actor->posX);
          v[2] += BLOWHARD_EYE_HEIGHT;
          if (CheckLineOfSightSegment(v, &SPY.posX) != 0) {
            st->minion = g_pfnLevelSpawnActor(ACTOR_TYPE_RIM_RIDER, actor);
            PlayActorMeshVoice(st->minion, RIMRIDER_VOICE_DROPPED, 0);
          }
        }
        if (g_nActorAnimDone != 0) {
          if (actor->anim == BLOWHARD_ANIM_DROP_RIDER && st->minion != 0) {
            ANIM_SET(actor, BLOWHARD_ANIM_DROP_END);
          }
          if (g_nActorAnimDone != 0 && actor->anim == BLOWHARD_ANIM_DROP_END) {
            actor->mode = BLOWHARD_P1_HOVER;
            st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
            ANIM_SET(actor, BLOWHARD_ANIM_HOVER);
          }
        }
        break;

      case BLOWHARD_P2_TRAVEL:
        if (TweenActorAlongPath(actor, st->path, st->tweenLeg, &st->tweenStep, BLOWHARD_TRAVEL_STEP, TWEEN_FLY) == TWEEN_WRAPPED) {
          actor->mode = BLOWHARD_P2_PATROL;
          actor->soundRange = 0;
          st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
          st->path = st->paths[3];
        }
        actor->rotY = 0;
        break;

      case BLOWHARD_P2_PATROL:
        if (dist < BLOWHARD_NEAR_RANGE && st->floorZ == -1) {
          if (st->pathDone == 0) {
            if (PivotActorAlongPath(actor, st->path, 0x80, BLOWHARD_PATROL_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY,
                              MOVE_LIFT) != 0) {
              st->pathDone = 1;
            }
          } else if (st->minion == 0 && dist > BLOWHARD_DROP_MIN_RANGE) {
            actor->mode = BLOWHARD_P2_DROP_RIDER;
            st->floorZ = actor->posZ - BLOWHARD_BOB_HEIGHT;
            ANIM_GO(actor, BLOWHARD_ANIM_DROP_RIDER);
          }
          break;
        }
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        break;

      case BLOWHARD_P2_DROP_RIDER:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, ATAN_ANGLE8)) < BLOWHARD_DROP_FACING &&
            actor->anim == BLOWHARD_ANIM_DROP_RIDER && actor->frame >= 6 && st->minion == 0) {
          int v[3];

          CopyVector(v, &actor->posX);
          v[2] += BLOWHARD_EYE_HEIGHT;
          if (CheckLineOfSightSegment(v, &SPY.posX) != 0) {
            st->minion = g_pfnLevelSpawnActor(ACTOR_TYPE_RIM_RIDER, actor);
            PlayActorMeshVoice(st->minion, RIMRIDER_VOICE_DROPPED, 0);
          }
        }
        if (g_nActorAnimDone != 0) {
          if (actor->anim == BLOWHARD_ANIM_DROP_RIDER && st->minion != 0) {
            ANIM_SET(actor, BLOWHARD_ANIM_DROP_END);
          }
          if (g_nActorAnimDone != 0 && actor->anim == BLOWHARD_ANIM_DROP_END) {
            actor->mode = BLOWHARD_P2_WAIT;
            st->wait = BLOWHARD_WAIT_FRAMES;
            st->pathDone = 0;
            ANIM_SET(actor, BLOWHARD_ANIM_HOVER);
          }
        }
        break;

      case BLOWHARD_P2_WAIT:
        if (TICK_DOWN(st->wait) != 0) {
          st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
          actor->mode = BLOWHARD_P2_PATROL;
        }
        break;

      case BLOWHARD_P3_TRAVEL:
        if (TweenActorAlongPath(actor, st->path, st->tweenLeg, &st->tweenStep, BLOWHARD_TRAVEL_STEP, TWEEN_FLY) == TWEEN_WRAPPED) {
          actor->mode = BLOWHARD_P3_PATROL;
          st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
          st->path = st->paths[4];
        }
        actor->rotY = 0;
        break;

      case BLOWHARD_P3_PATROL:
        if (dist < BLOWHARD_NEAR_RANGE && st->floorZ == -1) {
          if (st->pathDone == 0) {
            if (PivotActorAlongPath(actor, st->path, 0x80, BLOWHARD_PATROL_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY,
                              MOVE_LIFT) != 0) {
              st->pathDone = 1;
            }
          } else if (st->minion == 0 && dist > BLOWHARD_DROP_MIN_RANGE) {
            actor->mode = BLOWHARD_P3_DROP_RIDER;
            st->floorZ = actor->posZ - BLOWHARD_BOB_HEIGHT;
            ANIM_GO(actor, BLOWHARD_ANIM_DROP_RIDER);
          }
          break;
        }
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        break;

      case BLOWHARD_P3_DROP_RIDER:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, ATAN_ANGLE8)) < BLOWHARD_DROP_FACING &&
            actor->anim == BLOWHARD_ANIM_DROP_RIDER && actor->frame >= 6 && st->minion == 0) {
          int v[3];

          CopyVector(v, &actor->posX);
          v[2] += BLOWHARD_EYE_HEIGHT;
          if (CheckLineOfSightSegment(v, &SPY.posX) != 0) {
            st->minion = g_pfnLevelSpawnActor(ACTOR_TYPE_RIM_RIDER, actor);
            PlayActorMeshVoice(st->minion, RIMRIDER_VOICE_DROPPED, 0);
          }
        }
        if (g_nActorAnimDone != 0) {
          if (actor->anim == BLOWHARD_ANIM_DROP_RIDER && st->minion != 0) {
            ANIM_SET(actor, BLOWHARD_ANIM_DROP_END);
          }
          if (g_nActorAnimDone != 0) {
            if (actor->anim == BLOWHARD_ANIM_DROP_RIDER) {
              ANIM_SET(actor, BLOWHARD_ANIM_DROP_END);
            }
            if (g_nActorAnimDone != 0 && actor->anim == BLOWHARD_ANIM_DROP_END) {
              actor->mode = BLOWHARD_P3_WAIT;
              st->pathDone = 0;
              st->wait = BLOWHARD_WAIT_FRAMES;
              ANIM_SET(actor, BLOWHARD_ANIM_HOVER);
            }
          }
        }
        break;

      case BLOWHARD_P3_WAIT:
        if (TICK_DOWN(st->wait) != 0) {
          st->floorZ = actor->posZ + BLOWHARD_BOB_HEIGHT;
          actor->mode = BLOWHARD_P3_PATROL;
        }
        break;

      case BLOWHARD_NEXT_PHASE:
        st->phase++;
        switch (st->phase) {
        case BLOWHARD_PHASE_1:
          st->path = st->paths[0];
          break;
        case BLOWHARD_PHASE_2:
          st->path = st->paths[1];
          break;
        case BLOWHARD_PHASE_3:
          st->path = st->paths[2];
          break;
        }
        st->path->cur = 0;
        st->p28 = 0;
        st->p2C = 0;
        ENTER_POSE(actor, BLOWHARD_START_PHASE);
      }
      break;
    }

    /* Type 497: humming bell. Rings a looping voice while it is alive,
     * rings out when struck, and is removed once the voice has died away. */
    case ACTOR_TYPE_HUMMING_BELL: {
      HummingBellState *st = actor->state;
      int r;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != HBELL_STRUCK) {
        StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
        SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
        MarkActorLiveKill(actor);
        GetWorldChunkAnimState(st->chunkAnim);
        ENTER_POSE(actor, HBELL_STRUCK);
      }

      switch (actor->mode) {
      case HBELL_INIT:
        actor->mode = HBELL_HUM;
        ANIM_RESET(actor, HBELL_HUM);
        continue;

      case HBELL_STRUCK:
        if (g_nActorAnimDone != 0 && actor->cullRadius != ACTOR_CULL_HIDDEN) {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->frameRate = 0;
          actor->collision = 0;
          BurstActorWithSound(actor, HBELL_BURST_SPARK_LIFE);
        }
        switch (st->chunkAnim) {
        case 3:
        case 4:
          r = GetWorldChunkAnimState(st->chunkAnim);
          if (r & CHUNK_ANIM_HALTED) {
            if (CHUNK_ANIM_STEP(r) != 0) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            } else if (actor->cullRadius == ACTOR_CULL_HIDDEN) {
              DespawnActorRecord(actor);
              continue;
            }
          }
          break;

        case 5:
        case 7:
          r = GetWorldChunkAnimState(st->chunkAnim);
          if (r & CHUNK_ANIM_HALTED) {
            if (CHUNK_ANIM_STEP(r) != 0) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            } else if (actor->cullRadius == ACTOR_CULL_HIDDEN) {
              DespawnActorRecord(actor);
              continue;
            }
          }
          break;
        }
        break;

      case HBELL_HUM:
        if (g_dwGamestateFrames & 1) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, (int)(&g_pActorListBase[st->target]));
        }
        r = GetWorldChunkAnimState(st->chunkAnim);
        if ((r & CHUNK_ANIM_HALTED) && TICK_DOWN(st->timer) != 0) {
          st->timer = st->period;
          if (st->chunkAnim >= 5 && CHUNK_ANIM_STEP(r) >= 0x65) {
            st->timer = 0;
          }
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }
}
  }
}
