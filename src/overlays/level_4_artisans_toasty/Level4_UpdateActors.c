/* Level4_UpdateActors -- per-frame actor update for level 4 (Toasty).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (level 0's
 * Level0_UpdateActors, level 30's Level30_UpdateActors); the level-4-only
 * classes are the exit drop (9), the post guard (314), the lunging enemy
 * (335) and the spinning enemy (349).
 *
 * Load-bearing source forms (matched 2026-09-28):
 *   - Loop-top constant carriers, in this order: `sixteen` (s5), then `oneq`
 *     (a `short`) and `one` (an int), each with exactly one use. loop.c's
 *     combine_movables takes leaders in list order: `oneq` leads the byte
 *     `= 1` sites, `one` then leads the int sites and absorbs `oneq`, which
 *     orphans oneq's members so they stay inline `li rX,1`. `short` rather
 *     than `unsigned char` keeps cse from substituting oneq into later QImode
 *     stores (that broke the case 110/331 cross-jump). Without the carriers
 *     the hoisted 1 lands after `spy` in the preheader.
 *   - `dt` is a `$5` register variable: as a pseudo it dies in case 250's
 *     `phase += dt` and expand_preferences hands its a1 preference to the sum.
 *   - Cases 314/335 guard: `(c = actor->mode) != K` with `unsigned char c`,
 *     and the range test in QImode (`(unsigned char)(c - 13) >= 3`), which
 *     keeps the original's `andi` on the compare and the raw lbu reg in the
 *     range test.
 *   - Case 9: the posZ read is the non-struct `ACTOR_RAW(actor, int, posZ)` so it
 *     stays below the g_dwSpyroTriggerEventFlags store, and the level/tens stores sit before
 *     the 0x80000012 store (sched1 LUID order of the two hoisted constants).
 *   - Case 314: `limit = 10` after the first call; `mate->rotZ` copied before
 *     the two 0x28 stores.
 *   - Case 335: `&actor->posX` (not `pos`) for the floorProbe copy, so `pos` dies
 *     early; the `do { } while (0);` barrier sits after the head call.
 *   - Case 349 shares ONE `ENTER_POSE(actor, 4)` through gotos from cases 3
 *     and 5. Three separate copies push the constant-4 movable over loop.c's
 *     threshold and 4 gets hoisted (li t1,4 everywhere).
 *   - Case 234 is a fade-out prop placed between 195 and 250; it tests the
 *     loop head's cse'd `animFlags & 2` (the a0 the head computes).
 *   - The camera look-at pair C60/C64 is one struct (in_struct stores keep
 *     the sine-table loads below them); C68 is a separate scalar.
 *   - Case 120 reads the stage global through `extern int g_anSparxHealthBlock[]`.
 *   - `x << 16 >> 21` for a sine value >> 5 (a plain `>> 5` is narrowed to a
 *     HImode shift and comes out lhu/sll/sra).
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

void Level4_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int sixteen;
  int one;
  short oneq;
  char *base;
  register int dt asm("$5"); /* a1, as in the original (A374) */

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
  int svK[3];
  int shardTwinkle[3];
  int floorProbe[3];
  int letterFacing[3];
  int letterOffset[3];
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
              int yaw = g_nCameraEulerPitch & ANGLE12_MASK;

              if (yaw > ANGLE12_HALF) {
                yaw -= ANGLE12_TURN;
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
          actor->updateDist = UPDATE_DIST(0x8000);
        }

        if (st->active != 0 &&
            (parent->mode >= ACTOR_MODE_DORMANT || (parent->dropKind & DROP_ROLL_BIT))) {
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
        } else if (actor->subMode != FOLLOWER_HELD || parent->mode >= ACTOR_MODE_DORMANT) {
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

          ADD_SIGNED_BYTE(actor->posZ, st->vz);

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

          ADD_SIGNED_BYTE(actor->posZ, st->vz);

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
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1: {
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

      if (st->timer < ROT_BILLBOARD_GLINT_TIMER_CAP) {
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

          if (g_aRotBillboards[st->attach].life < ROT_BILLBOARD_FADING_LIFE) {
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
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) == VOICE_OWNER_NONE) {
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
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[SPARX_VOICE_BUZZ]) == VOICE_OWNER_NONE) {
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
            st->glow->r = SPARX_GLOW_R;
            st->glow->g = SPARX_GLOW_G;
            st->glow->b = SPARX_GLOW_B;
            st->glow->points = SPARX_GLOW_POINTS;
            st->glow->desc = &g_anSparxGlowShape;
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
              ScaleVector3ByRatio(svA, dist, limit);
            } else {
              ScaleVector3ByRatio(svA, dist, reach - SPARX_CHASE_STEP_SLACK);
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

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[SFX_GEM_BOUNCE]) == VOICE_OWNER_NONE) {
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
          if (g_aRotBillboards[st->attach].life < ROT_BILLBOARD_FADING_LIFE) {
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
      if ((actor->flags & ACTOR_HIT_BY_SPYRO) == 0) {
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_REBOUND_FX:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

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

          if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[STATUE_VOICE_RATTLE]) == VOICE_OWNER_NONE) {
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
              g_anSpyroRequestVelocity.y = g_nSpyroMotionY >> 8;
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

              if (SPY.requestVelX != 0 || g_anSpyroRequestVelocity.y != 0) {
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
      register int i asm("$16"); /* s0, as in the original (A374) */

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

    /* Post guard (GuardState): patrols between posts, raises the alarm when
     * Spyro gets close, and hands off to its partner when knocked down. */
    case ACTOR_TYPE_ALARM_POST_GUARD: {
      GuardState *st = actor->state;
      unsigned char c;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && (unsigned char)((c = actor->mode) - APGUARD_HIT) >= 10 &&
          c != APGUARD_HANDED_OFF) {
        actor->mode = APGUARD_HIT;
        continue;
      }

      actor->flags = 0;

      switch (actor->mode) {
      case APGUARD_HOLD: {
        int limit;
        int toSpy;
        int toMe;
        int speed;

        toSpy = ArcTan2(st->posts->nodes[st->stage].x - spy->posX,
                              st->posts->nodes[st->stage].y - spy->posY, ATAN_ANGLE8);
        limit = 10;
        toMe = ArcTan2(actor->posX - st->posts->nodes[st->stage].x,
                             actor->posY - st->posts->nodes[st->stage].y, ATAN_ANGLE8);
        speed = st->stage * 20 + 0xB4;

        if (actor->nextAnim == APGUARD_ANIM_STAND) {
          limit = 0x19;
        }

        if (limit < AbsAngleDelta8(toSpy, toMe)) {
          SteerActorAroundPoint(actor, &st->posts->nodes[st->stage].x, 0x1388, toSpy, 6,
                        speed, 6, 0x14, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);
          ANIM_GO(actor, st->anim + APGUARD_ANIM_STEP);
        } else {
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          ANIM_GO(actor, st->anim);
        }

        if (PickupGroupHasLiveActor(st->stage) == 0) {
          actor->mode = APGUARD_WATCH;
          continue;
        }
        break;
      }

      case APGUARD_HIT:
        switch (st->stage) {
        case 0:
          ModifyWorldChunkAnimFlags(APGUARD_STAGE_CHUNK(0), CHUNK_ANIM_RESUME, 0);
          if (st->drop >= 0) {
            Actor *drop = &g_pActorListBase[st->drop];

            CopyVector(&drop->posX, &actor->posX);
            SpawnActorDeathDrop(drop, DROP_SCATTER, 0, 0);
            MarkActorLiveKill(drop);
            st->drop = -1;
            DespawnActorRecord(drop);
          }
          if (actor->nextAnim < st->anim + APGUARD_ANIM_SET_LEN) {
            ANIM_GO(actor, st->anim + APGUARD_ANIM_HIT);
          } else {
            ANIM_SET(actor, st->anim + APGUARD_ANIM_HIT);
          }
          break;

        case 1: {
          Actor *mate = &g_pActorListBase[st->partner];
          GuardState *ms = mate->state;

          ModifyWorldChunkAnimFlags(APGUARD_STAGE_CHUNK(1), CHUNK_ANIM_RESUME, 0);
          mate->mode = APGUARD_HIT_PLAYOUT;
          mate->anim = APGUARD_ANIM_PARTNER_SET;
          mate->nextAnim = APGUARD_ANIM_PARTNER_SET;
          ms->anim = APGUARD_ANIM_PARTNER_SET;
          ms->stage = 1;
          ms->path->cur = st->path->cur;
          CopyVector(&mate->posX, &actor->posX);
          mate->rotZ = actor->rotZ;
          mate->cullRadius = CULL_RADIUS(0x2800);
          mate->updateDist = UPDATE_DIST(0xA000);
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          ANIM_SET(actor, APGUARD_ANIM_HAND_OFF);
          break;
        }

        case 2:
          ANIM_GO(actor, APGUARD_ANIM_PARTNER_SET + APGUARD_ANIM_HIT);
          break;
        }
        actor->mode += 1;
        break;

      case APGUARD_HIT_PLAYOUT:
        if (g_nActorAnimDone == 0) {
          break;
        }
        switch (st->stage) {
        case 0:
          ANIM_GO(actor, st->anim + APGUARD_ANIM_STEP);
          break;

        case 1:
          if (st->anim == 0) {
            ANIM_SET(actor, APGUARD_ANIM_HIDDEN);
            actor->collision = 0;
            actor->mode = APGUARD_HANDED_OFF;
            continue;
          }
          ANIM_GO(actor, st->anim + APGUARD_ANIM_STEP);
          break;

        case 2:
          ModifyWorldChunkAnimFlags(APGUARD_STAGE_CHUNK(2), CHUNK_ANIM_RESUME, 0);
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          BurstActorWithSound(actor, APGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        actor->mode += 1;
        break;

      case APGUARD_RECOVER:
        st->alarm = 0x78;
        actor->mode += 1;
        break;

      case APGUARD_TO_POST:
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->wp[st->stage]].x) <
            0x258) {
          st->stage += 1;
          actor->mode = APGUARD_HOLD;
          continue;
        }
        PivotActorAlongPath(actor, st->path, 0x200, 0xC8, 0, 9, 0x14, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        break;

      case APGUARD_WATCH: {
        int range = 0xBB8;

        if (st->stage == 2) {
          range = 0x960;
        }

        if (TrackEntityYawTowardSpyro(actor, 6, 0x14, YAW_STEP_IN_BAND) != 0 && g_nActorAnimDone != 0) {
          if (st->stage < 2) {
            if (st->count == 0) {
              st->count = RandomIntInRange(2, 4);
              ANIM_SET(actor, APGUARD_ANIM_FIDGET);
            } else {
              st->count -= 1;
              if (actor->nextAnim == APGUARD_ANIM_FIDGET) {
                ANIM_SET(actor, APGUARD_ANIM_STAND);
              } else {
                ANIM_GO(actor, APGUARD_ANIM_STAND);
              }
            }
          } else {
            ANIM_GO(actor, st->anim);
          }
        }

        if (TICK_DOWN(st->timer) != 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < range) {
          actor->mode = APGUARD_SHOVE;
          continue;
        }
        break;
      }

      case APGUARD_SHOVE:
        if (actor->nextAnim != st->anim + APGUARD_ANIM_SHOVE) {
          st->timer = 0xB4;
          g_anSpyroRequestVelocity.x = g_anSineLut[actor->rotZ] << 16 >> 21;
          g_anSpyroRequestVelocity.y = -(g_anCosineLut[actor->rotZ] << 7) >> 12;
          g_nSpyroRequestVelocityZ = 0;
          if (actor->nextAnim < st->anim + APGUARD_ANIM_SET_LEN) {
            ANIM_GO(actor, st->anim + APGUARD_ANIM_SHOVE);
          } else {
            ANIM_SET(actor, st->anim + APGUARD_ANIM_SHOVE);
          }
          actor->mode = APGUARD_WATCH;
          continue;
        }
        break;
      }

      if (actor->mode != APGUARD_HANDED_OFF) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
        int tick = actor->subMode + g_nFrameStep;

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

    /* Lunging enemy (LungeState): charges when Spyro comes close, leaps at
     * him, and takes a few hits before it goes down. */
    case ACTOR_TYPE_LUNGER: {
      int *pos = &actor->posX;
      LungeState *st;
      int dist;
      unsigned char c;

      st = actor->state;
      dist = ApproxDist2D(pos, &spy->posX);
      EBB_BREAK();

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && (c = actor->mode) != LUNGER_KNOCKED &&
          (unsigned char)(c - LUNGER_RECOIL) >= 3) {
        if (st->hits == 0) {
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          st->knockSpeed = LUNGER_KNOCK_SPEED;
          st->knockHeading = StepAndLerpAngle8(
              ArcTan2(actor->posX - spy->posX, actor->posY - SPY.posY, ATAN_ANGLE8),
              g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
          st->knockZSpeed = LUNGER_KNOCK_VZ;
          actor->mode = LUNGER_KNOCKED;
          continue;
        }
        if (st->guard == 0) {
          WrapAngleDelta8(actor->rotZ, ArcTan2(spy->posX - actor->posX,
                                                    SPY.posY - actor->posY, ATAN_ANGLE8));
          st->hits -= 1;
          st->anim += 7;
          goto lunge_hit;
        }
        if (st->guard >= 0) {
          if (st->guard < 4) {
            WrapAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8));
            *st->flag = 1;
            st->hits -= 1;
            st->anim += 7;
            st->heading = actor->rotZ;
            CopyVector(st->home, pos);
          lunge_hit:
            actor->mode = LUNGER_RECOIL;
            ANIM_RESET(actor, LUNGER_RECOIL);
            continue;
          }
        }
      }

      actor->flags = 0;
      CopyVector(floorProbe, &actor->posX);
      floorProbe[2] += GROUND_PROBE_LIFT;
      if (FindGroundHeightBelow(floorProbe, 0x1000) == 0) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
        MarkActorLiveKill(actor);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        BurstActorWithSound(actor, LUNGER_BURST_SPARK_LIFE);
        DespawnActorRecord(actor);
        continue;
      }

      if (TICK_DOWN(st->burn) == 0) {
        SPY.triggerEvents |= (SPYRO_EV_HIT | SPYRO_EV_CONTACT_FRONT | SPYRO_EV_HIT_PINNED);
      }

      if (TICK_DOWN(st->wait) != 0 && dist < LUNGER_AIM_RANGE &&
          actor->mode == LUNGER_IDLE) {
        CopyVector(st->target, &SPY.posX);
        st->wait = 0;
        actor->subMode = 0;
        actor->mode = LUNGER_AIM;
        continue;
      }

      switch (actor->mode) {
      case LUNGER_IDLE:
        if (st->heading == -1) {
          st->heading = actor->rotZ;
        }
        ANIM_GO(actor, st->anim);
        break;

      case LUNGER_AIM:
        st->knockZSpeed = (ApproxDist2D(&actor->posX, &SPY.posX) * 35 >> 10) + LUNGER_LEAP_VZ;
        ANIM_GO(actor, st->anim + 2);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->target[0] - actor->posX,
                                        st->target[1] - actor->posY, ATAN_ANGLE8),
                          0xA, 4, YAW_STEP_IN_BAND) != 0) {
          PlayActorMeshVoice(actor, LUNGER_VOICE_LEAP, 0);
          actor->mode = LUNGER_LEAP;
          continue;
        }
        break;

      case LUNGER_LEAP: {
        int floor = ProbeActorGroundHeight(actor);

        actor->posZ += st->knockZSpeed;
        st->knockZSpeed -= LUNGER_LEAP_GRAVITY;
        if (st->knockZSpeed < -LUNGER_LEAP_MAX_FALL) {
          st->knockZSpeed = -LUNGER_LEAP_MAX_FALL;
        }
        if (SPY.state == SPYRO_ST_HURT_PINNED) {
          st->burn = LUNGER_BURN_FRAMES;
        }

        if (floor + actor->floorOffset >= actor->posZ) {
          if (actor->subMode == 0) {
            PlayActorMeshVoice(actor, LUNGER_VOICE_LAND, 0);
            actor->subMode = 1;
          }
          if (actor->nextFrame < 0xD) {
            actor->frame = 0xD;
            actor->nextFrame = 0xE;
            actor->frameProgress = 0;
          }
          actor->posZ = floor + actor->floorOffset;
          if (actor->nextFrame >= 0x18) {
            if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x384) {
              actor->collision = 0;
            }
            ANIM_GO(actor, st->anim + 1);
            actor->mode = LUNGER_GO_HOME;
            continue;
          }
        } else {
          dist = ApproxDist2D(&actor->posX, &SPY.posX);
          if (actor->nextFrame == 0xD) {
            actor->frame = 8;
            actor->nextFrame = 9;
            actor->frameProgress = 0;
          }
          if ((unsigned int)(dist - LUNGER_LEAP_HOMING_MIN) < 0x2CF && st->knockZSpeed < 0x64 &&
              actor->posZ - floor >= LUNGER_LEAP_HOMING_HEIGHT) {
            MoveActorAlongAngle(actor,
                          ArcTan2(SPY.posX - actor->posX,
                                        SPY.posY - actor->posY, ATAN_ANGLE8),
                          LUNGER_LEAP_HOMING_SPEED, 0, 0, 0);
          } else if (ApproxDist2D(&actor->posX, st->target) >= LUNGER_LEAP_ARRIVE) {
            MoveActorAlongHeading(actor, LUNGER_LEAP_SPEED, 0, 0x12C, 0);
          }
          if (dist < LUNGER_PIN_RANGE && actor->posZ < g_nSpyroWorldPosZ - LUNGER_PIN_DEPTH) {
            g_nSpyroWorldPosZ = actor->posZ + LUNGER_PIN_DEPTH;
          }
        }
        break;
      }

      case LUNGER_KNOCKED:
        ANIM_GO(actor, st->anim + 4);
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, LUNGER_KNOCK_DRAG,
                      LUNGER_KNOCK_GRAVITY);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LUNGER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case LUNGER_BURST:
        if (actor->nextAnim >= 0xC) {
          ANIM_SET(actor, st->anim + 5);
        } else {
          ANIM_GO(actor, st->anim + 5);
        }
        if (g_nActorAnimDone != 0) {
          BurstActorWithSound(actor, LUNGER_BURST_SPARK_LIFE);
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case LUNGER_RECOIL:
        if (actor->nextFrame < 2) {
          CopyVector(st->target, &SPY.posX);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->target[0] - actor->posX,
                                    st->target[1] - actor->posY, ATAN_ANGLE8),
                      7, 0, YAW_HOLD_IN_BAND);
        if (actor->nextFrame >= 9) {
          ANIM_SET(actor, st->anim + 1);
          st->wait = 0;
          actor->subMode = 0;
          actor->mode = LUNGER_AIM;
          continue;
        }
        break;

      case LUNGER_RECOIL_B:
      case LUNGER_RECOIL_C:
        if (actor->nextFrame >= 9) {
          ANIM_SET(actor, st->anim + 1);
          actor->mode = LUNGER_AIM;
          continue;
        }
        break;

      case LUNGER_ALERTED:
        st->alerted = 1;
        actor->mode = LUNGER_IDLE;
        continue;

      case LUNGER_GO_HOME:
        if (ApproxDist2D(&actor->posX, st->home) < LUNGER_HOME_ARRIVE) {
          if (TurnActorYawTowardAngle(actor, st->heading, 6, 5, YAW_STEP_IN_BAND) != 0) {
            *st->flag = 0;
            st->wait = LUNGER_WAIT_FRAMES;
            actor->mode = LUNGER_IDLE;
            continue;
          }
        } else {
          ANIM_GO(actor, st->anim + 1);
          if (TurnActorYawTowardAngle(actor,
                            ArcTan2(st->home[0] - actor->posX,
                                          st->home[1] - actor->posY, ATAN_ANGLE8),
                            6, 0x14, YAW_STEP_IN_BAND) != 0) {
            MoveActorAlongHeading(actor, LUNGER_HOME_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          }
        }
        break;
      }

      if (actor->mode == LUNGER_LEAP ||
          ApproxDist2D(&actor->posX, &spy->posX) >= 0x385) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      } else if (actor->collision == 0 &&
                 ApproxDist2D(&spy->posX, st->home) < LUNGER_NUDGE_RANGE) {
        if (TICK_DOWN(st->look) != 0) {
          int ang =
              ArcTan2(spy->posX - actor->posX, SPY.posY - actor->posY, ATAN_ANGLE8);

          g_anSpyroRequestVelocity.x = g_anCosineLut[ang] * 25 >> 10;
          g_anSpyroRequestVelocity.y = g_anSineLut[ang] * 25 >> 10;
          g_nSpyroRequestVelocityZ = 0;
          st->look = LUNGER_NUDGE_FRAMES;
        } else {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_RAW_VELOCITY);
        }
      }
      break;
    }

    /* Spinning enemy (SpinnerState): idles, rears up at Spyro, and spins
     * away when knocked back. */
    case ACTOR_TYPE_SPINNER: {
      SpinnerState *st;

      EBB_BREAK();
      st = actor->state;

      if ((actor->flags & ACTOR_HIT_BY_SPYRO) && actor->mode != REAR_KNOCKED) {
        SetActorGroupSubstate(actor, 1);
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->speed = REAR_KNOCK_SPEED;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->speed = REAR_CHARGE_KNOCK_SPEED;
        }
        st->spin = RandomIntInRange(2, 5);
        if (GetRandomU32() & 1) {
          st->spin = -st->spin;
        }
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE4(actor, REAR_KNOCKED);
      }

      switch (actor->mode) {
      case REAR_IDLE:
        if (g_nActorAnimDone != 0) {
          if (st->count == 0) {
            st->count = RandomIntInRange(3, 6);
            ANIM_GO(actor, REAR_ANIM_FIDGET);
          } else {
            st->count -= 1;
            ANIM_GO(actor, REAR_IDLE);
          }
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < REAR_ROUSE_RANGE) {
          if (actor->anim == REAR_IDLE) {
            ENTER_POSE4(actor, REAR_NOTICE);
          }
          ENTER_POSE4(actor, REAR_REAR);
        }
        break;

      case REAR_NOTICE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE4(actor, REAR_REAR);
        }
        break;

      case REAR_REAR:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        goto spin_wait;

      case REAR_REARED: {
        int dist = ApproxDist2D(&actor->posX, &SPY.posX);

        if (g_nActorAnimDone != 0) {
          if (dist > REAR_FIDGET_RANGE) {
            if (st->count == 0) {
              st->count = RandomIntInRange(3, 6);
              ANIM_GO(actor, REAR_ANIM_REARED_FIDGET);
            } else {
              st->count -= 1;
              ANIM_GO(actor, REAR_REARED);
            }
          } else {
            ANIM_GO(actor, REAR_REARED);
          }
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (dist < REAR_SHOVE_RANGE) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < REAR_SHOVE_DZ : (SPY.posZ - height) < REAR_SHOVE_DZ) {
            ENTER_POSE4(actor, REAR_SHOVE);
          }
        }
        if (dist > REAR_LOSE_RANGE) {
          ENTER_POSE4(actor, REAR_DROP);
        }
        break;
      }

      case REAR_SHOVE:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nSpyroState == SPYRO_ST_KNOCKBACK) {
          st->timer = REAR_GLOAT_TICKS;
        }
        if (g_nActorAnimDone != 0) {
          if (st->timer != 0) {
            ENTER_POSE4(actor, REAR_GLOAT);
          }
          goto spin_pose;
        }
        g_anSpyroRequestVelocity.x = g_anSineLut[actor->rotZ] << 16 >> 21;
        g_anSpyroRequestVelocity.y = -(g_anCosineLut[actor->rotZ] << 7) >> 12;
        g_nSpyroRequestVelocityZ = 0;
        break;

      case REAR_DROP:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE4(actor, REAR_IDLE);
        }
        break;

      case REAR_KNOCKED:
        if (actor->frame < REAR_SPIN_FRAMES) {
          actor->rotZ += st->spin;
        }
        if (st->speed > REAR_KNOCK_DRAG) {
          st->speed -= REAR_KNOCK_DRAG;
          MoveActorAlongAngle(actor, st->knockHeading, st->speed, 0, REAR_MOVE_RADIUS, MOVE_LIFT);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, REAR_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case REAR_GLOAT:
        if (actor->frame >= REAR_GLOAT_LOOP_END && TICK_DOWN(st->timer) != 0) {
          actor->frame = REAR_GLOAT_LOOP_START;
        }
      spin_wait:
        if (g_nActorAnimDone != 0) {
        spin_pose:
          ENTER_POSE4(actor, REAR_REARED);
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
      break;
    }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD2_ENTER_FX:
    case ACTOR_TYPE_HAZARD2_DEATH_FX:
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
        if (ApproxDist2D(&st->sign->posX, &spy->posX) < LEVELEXIT_ENTER_RANGE) {
          unsigned char was = actor->subMode;
          int prev = g_anCosineLut[was];
          unsigned char now = was + LETTER_SWAY_STEP;

          actor->subMode = now;
          actor->rotZ =
              actor->rotZ - ((prev * 3) >> 9) + ((g_anCosineLut[now] * 3) >> 9);
        } else {
          int bearing;
          int arm;
          int idx;
          short *swing;

          actor->subMode += LETTER_SWAY_STEP;
          bearing = ArcTan2(spy->posX - st->sign->posX,
                                  SPY.posY - st->sign->posY, ATAN_ANGLE8);
          letterFacing[0] = (g_anCosineLut[bearing] * 3) >> 4;
          letterFacing[1] = (g_anSineLut[bearing] * 3) >> 4;
          letterFacing[2] = 0;
          letterOffset[0] =
              (unsigned short)g_anCosineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          letterOffset[1] =
              (unsigned short)g_anSineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          letterOffset[2] = 0;
          bearing = (bearing + ANGLE8_HALF) & ANGLE8_MASK;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterOffset, st->count - 1);
          LShiftVector3(letterOffset, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = letterFacing[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = letterFacing[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterOffset, letterOffset, st->index);
          SubtractVector(letterSlot, letterSlot, letterOffset);
          swing = &g_anCosineLut[bearing];
          actor->posX = letterFacing[0] * *swing;
          actor->posY = letterFacing[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, letterFacing);
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
