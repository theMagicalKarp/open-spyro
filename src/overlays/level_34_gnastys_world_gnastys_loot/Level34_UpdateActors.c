/* Level34_UpdateActors -- per-frame actor update for level 34 (Gnasty's
 * Loot). 0x8007AD4C, 35,140 bytes.
 *
 * New classes decoded here: bolt (17), vault timer (217, falls through into
 * the 260 group), treasure thief (177), loot critter (179), loot vortex
 * (181) and debris (481-483). The rest are copied from levels 8, 9, 15, 16,
 * 25, 26 and 31.
 *
 * Load-bearing forms found so far:
 *   - A loop-top `spyc = &g_anSpyroWorldPos` carrier, used only by case 177's
 *     camera anchor and case 181's headLook: loop.c hoists and spills it,
 *     which drops the threshold so g_anSineLut is NOT hoisted (original).
 *   - The four path-index locals (thiefSpyroNode/thiefSpyroNode2/critterSpyroNode/critterAwayNode) are scalars
 *     (scalar MEMs do not alias the state block), and every array used from
 *     case 181 on is declared in a block that opens at case 181, so the
 *     scalars get their slots first (312..324) as in the original frame.
 *   - Case 181: `r = g_bSpyroPersistentEulerZ + K; rotZ = r + ...` keeps the constant on
 *     g_bSpyroPersistentEulerZ (fold would move it); the spark loop is goto-built so
 *     loop.c does not hoist 0x1000000; a NULL actor to PlaySoundEffect.
 *   - Case 177's knockback spawn loop is a `for`; case 2 uses `++st->laps`.
 *
 *   - Single-set temps (cookbook F14d): case 177's case-0 clamp uses its
 *     own `d0`; case 181 reads `SPY.bodyRotZ` (a struct MEM, so it depends
 *     on the preceding `st->timer` store) and writes the BFC..C04 triple as
 *     `g_nSpyroSwingEulerTargetY[-1..1]` so the `st->chunkAnim` load waits for them; the settleOffset
 *     init stores `w[0]` (the timer read) first.
 *
 *   - Case 177's camera block writes every camera word as an aggregate
 *     element (g_nSpyroSwingEulerTargetY[k], g_anCameraActiveTargetParams.w[k]): array/struct MEMs may alias
 *     the `st->path` arg load, so they all stay ahead of it, while the
 *     scalar `g_nCinematicActive = one` store is free to sink next to the call.
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
#include "cd_stream.h"
#include "hud_counter.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. The
 * clones that need a loop-top carrier for 0x10 declare `int sixteen`; here
 * it is the plain constant, so arm bodies read the same in every level. */
#define ANIM_RATE16 sixteen
#define sixteen 0x10

#define spy ((SpyroObj *)&g_anSpyroWorldPos)

void Level34_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  short oneq;
  int one;
  SpyroObj *spyc;
  char *base;
  int dt;
  int thiefSpyroNode;
  int thiefSpyroNode2;
  int critterSpyroNode;
  int critterAwayNode;

  /* Scratch vectors, in frame order. */
  int nextPos[3];
  int apex[3];
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

    /* Type 17: bolt. Flies along its velocity until its timer runs out or it
     * hits the world, then despawns. */
    case ACTOR_TYPE_AIMED_SHOT: {
      BoltFlight *st = actor->state;
      int *pos;

      if (TICK_DOWN(st->timer)) {
        DespawnActorRecord(actor);
        continue;
      }
      pos = &actor->posX;
      AddVector(nextPos, pos, st->vel);
      if (CastRayWorldAndActors(pos, nextPos) || RaiseSpyroTriggerInRadius(pos, 0, SPYRO_EV_HIT)) {
        DespawnActorRecord(actor);
        continue;
      }
      CopyVector(pos, nextPos);
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

          CopyVector(apex, &SPY.posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * EGG_APEX_DIAG_4;
          y = cs * EGG_APEX_DIAG_4;
          apex[0] += (y + x) * 4 >> 12;
          apex[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * EGG_APEX_DIAG_4) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * (unsigned)EGG_APEX_DIAG_4)) *
                        4 >>
                    12;
          apex[2] += EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->apex, apex);
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
          CopyVector(apex, &actor->posX);
          apex[2] += GROUND_PROBE_LIFT;
          FindGroundHeightBelow(apex, GROUND_PROBE_FAR);
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

            CopyVector(apex, &st->target->posX);

            if (high == 0) {
              apex[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              apex[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              apex[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              apex[1] -=
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

            SubtractVector(apex, apex, &actor->posX);
            dist = VectorLength(apex, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(apex, dist, limit);
            } else {
              ScaleVector3ByRatio(apex, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, apex);
            TurnActorYawTowardAngle(actor, ArcTan2(apex[0], apex[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(apex, VEC_LEN_XY), apex[2], ATAN_ANGLE8);
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

    /* Type 177: treasure thief (LootThiefState). */
    case ACTOR_TYPE_LOOT_THIEF: {
      LootThiefState *st = actor->state;
      int speed;
      int n;
      int d;
      int d0;

      if (st->init == 0) {
        actor->frameRate = 0;
        actor->cullRadius = ACTOR_CULL_HIDDEN;
        actor->drawn = 0;
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        if (CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim)) > 0) {
          st->init = one;
          actor->frameRate = 0x10;
          actor->cullRadius = 0x40;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          ZeroVector(st->look);
          actor->subMode = 0;
          st->speed = LTHIEF_INTRO_SPEED;
          st->path->cur = 0;
          if (st->onRing != 0) {
            st->laps = 0;
          } else {
            st->laps = 0xFF;
          }
        }
        break;
      }

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) != 0 && actor->mode != one) {
        int i;

        st->knockZSpeed = 0;
        st->speed = LTHIEF_KNOCK_SPEED;
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8),
            g_bSpyroPersistentEulerZ, 0x28, 0x40);
        for (i = 0; i < 3; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS + 1, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARK_DEBRIS + 2, actor);
        }
        PlayActorMeshVoice(actor, LTHIEF_VOICE_KNOCKED, 0);
        actor->mode = LTHIEF_KNOCKED;
        ANIM_RESET(actor, LTHIEF_KNOCKED);
        continue;
      }

      switch (actor->mode) {
      case LTHIEF_RUN:
        if (st->onRing != 0) {
          speed = LTHIEF_RING_SPEED - st->laps * LTHIEF_LAP_SLOWDOWN;
          switch (st->leg) {
          case LTHIEF_LEG_PARKED:
            if (ApproxDist2D(&actor->posX, &SPY.posX) < LTHIEF_NOTICE_RANGE) {
              FindNearestPathPointToSpyro(st->ring, &thiefSpyroNode);
              if (thiefSpyroNode < st->ring->cur) {
                thiefSpyroNode += st->ring->count;
              }
              thiefSpyroNode -= st->ring->cur;
              if (thiefSpyroNode < st->ring->count >> 1) {
                st->ring->dir = one;
              } else {
                st->ring->dir = -1;
              }
              if (st->ring->dir > 0) {
                st->curPath = st->pathB;
              } else {
                st->curPath = st->pathA;
              }
              st->curPath->cur = st->curPath->count - 1;
              st->curPath->dir = one;
              st->leg = one; /* LTHIEF_LEG_SPUR_IN */
            }
            d0 = (FLY_Z(st->curPath, st->curPath->count - 1) - actor->posZ) >> 3;
            if (d0 > LTHIEF_CLIMB_STEP) {
              d0 = LTHIEF_CLIMB_STEP;
            }
            if (d0 < -LTHIEF_CLIMB_STEP) {
              d0 = -LTHIEF_CLIMB_STEP;
            }
            actor->posZ += d0;
            actor->rotY = 0;
            break;

          case LTHIEF_LEG_SPUR_IN:
            if (PivotActorAlongPath(actor, st->curPath, 0x400, speed, 0, 6, 0x18, PIVOT_TURN_ANY,
                              0) == ((st->curPath->count - 1) | 0x100)) {
              if (st->curPath == st->pathB) {
                st->ring->dir = one;
                st->ring->cur = st->ring->count - 1;
              } else {
                st->ring->dir = -1;
                st->ring->cur = 0;
              }
              st->leg = LTHIEF_LEG_RING;
              st->curPath = st->ring;
            }
            d = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            if (d > LTHIEF_CLIMB_STEP) {
              d = LTHIEF_CLIMB_STEP;
            }
            if (d < -LTHIEF_CLIMB_STEP) {
              d = -LTHIEF_CLIMB_STEP;
            }
            actor->posZ += d;
            actor->rotY = -ArcTan2(d, speed, ATAN_ANGLE8) + 0x40;
            break;

          case LTHIEF_LEG_RING:
            n = 0x100;
            if (st->ring->dir > 0) {
              n = (st->curPath->count - 1) | 0x100;
            }
            if (PivotActorAlongPath(actor, st->curPath, 0x400, speed, 0, 6, 0x18, PIVOT_TURN_ANY,
                              0) == n) {
              st->path->dir = -st->ring->dir;
              if (st->path->dir > 0) {
                st->path->cur = st->path->count - 1;
              } else {
                st->path->cur = 0;
              }
              st->curPath = st->path;
              if (++st->laps >= LTHIEF_MAX_LAPS) {
                st->laps = LTHIEF_MAX_LAPS - 1;
              }
              st->leg = LTHIEF_LEG_CROSS;
            }
            n = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            if (n > LTHIEF_RING_CLIMB_STEP) {
              n = LTHIEF_RING_CLIMB_STEP;
            }
            if (n < -LTHIEF_RING_CLIMB_STEP) {
              n = -LTHIEF_RING_CLIMB_STEP;
            }
            actor->posZ += n;
            actor->rotY = -ArcTan2(n, speed, ATAN_ANGLE8) + 0x40;
            if (ApproxDist2D(&actor->posX, &SPY.posX) > LTHIEF_LOSE_RANGE) {
              st->leg = LTHIEF_LEG_RING_OUT;
            }
            break;

          case LTHIEF_LEG_CROSS:
            n = 0x100;
            if (st->ring->dir > 0) {
              n = (st->curPath->count - 1) | 0x100;
            }
            if (PivotActorAlongPath(actor, st->curPath, 0x400, speed, 0, 6, 0x18, PIVOT_TURN_ANY,
                              0) == n) {
              st->ring->dir = -st->path->dir;
              if (st->ring->dir > 0) {
                st->ring->cur = st->ring->count - 1;
              } else {
                st->ring->cur = 0;
              }
              st->leg = LTHIEF_LEG_RING;
              st->curPath = st->ring;
            }
            n = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            if (n > LTHIEF_RING_CLIMB_STEP) {
              n = LTHIEF_RING_CLIMB_STEP;
            }
            if (n < -LTHIEF_RING_CLIMB_STEP) {
              n = -LTHIEF_RING_CLIMB_STEP;
            }
            actor->posZ += n;
            actor->rotY = -ArcTan2(n, speed, ATAN_ANGLE8) + 0x40;
            break;

          case LTHIEF_LEG_RING_OUT:
            FindNearestPathPointToSpyro(st->ring, &thiefSpyroNode2);
            if (thiefSpyroNode2 < st->ring->cur) {
              thiefSpyroNode2 += st->ring->count;
            }
            thiefSpyroNode2 -= st->ring->cur;
            if (thiefSpyroNode2 < st->ring->count >> 1) {
              st->ring->dir = one;
            } else {
              st->ring->dir = -1;
            }
            st->curPath = st->ring;
            n = 0x100;
            if (st->ring->dir > 0) {
              n = (st->curPath->count - 1) | 0x100;
            }
            if (PivotActorAlongPath(actor, st->curPath, 0x400, speed, 0, 6, 0x18, PIVOT_TURN_ANY,
                              0) == n) {
              if (st->ring->dir > 0) {
                st->curPath = st->pathA;
              } else {
                st->curPath = st->pathB;
              }
              st->curPath->cur = 0;
              st->curPath->dir = -1;
              st->laps++;
              if (st->laps >= LTHIEF_MAX_LAPS) {
                st->laps = LTHIEF_MAX_LAPS - 1;
              }
              st->leg = LTHIEF_LEG_SPUR_OUT;
            } else if (ApproxDist2D(&actor->posX, &SPY.posX) < LTHIEF_NOTICE_RANGE) {
              st->leg = LTHIEF_LEG_RING;
            }
            n = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            if (n > LTHIEF_RING_CLIMB_STEP) {
              n = LTHIEF_RING_CLIMB_STEP;
            }
            if (n < -LTHIEF_RING_CLIMB_STEP) {
              n = -LTHIEF_RING_CLIMB_STEP;
            }
            actor->posZ += n;
            actor->rotY = -ArcTan2(n, speed, ATAN_ANGLE8) + 0x40;
            break;

          case LTHIEF_LEG_SPUR_OUT:
            if (PivotActorAlongPath(actor, st->curPath, 0x400, speed, 0, 6, 0x18, PIVOT_TURN_ANY,
                              MOVE_CHECK_Z) == PATH_ARRIVED(0)) {
              st->leg = LTHIEF_LEG_PARKED;
            }
            d = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            if (d > LTHIEF_CLIMB_STEP) {
              d = LTHIEF_CLIMB_STEP;
            }
            if (d < -LTHIEF_CLIMB_STEP) {
              d = -LTHIEF_CLIMB_STEP;
            }
            actor->posZ += d;
            actor->rotY = -ArcTan2(d, speed, ATAN_ANGLE8) + 0x40;
            break;
          }
        } else {
          int yaw = actor->rotZ;

          if (actor->subMode != 0) {
            TweenActorAlongPath(actor, st->curPath, st->look, &st->laps, 2,
                          0);
            if (AbsAngleDelta8(actor->rotZ,
                              ArcTan2(SPY.posX - actor->posX,
                                            SPY.posY - actor->posY, ATAN_ANGLE8)) <
                0x40) {
              d = (LTHIEF_DODGE_Z - actor->posZ) >> 1;
            } else {
              d = (FLY_Z(st->curPath, st->curPath->cur) - actor->posZ) >> 3;
            }
            if (d > 0x80) {
              d = 0x80;
            }
            if (d < -0x80) {
              d = -0x80;
            }
            actor->posZ += d;
            actor->rotY = -ArcTan2(d, 0x400, ATAN_ANGLE8) + 0x40;
          } else {
            int r;

            g_nCinematicActive = one;
            /* g_pSpyroRequestCameraAnchor / g_pSpyroRequestCameraTargetParams / g_dwSpyroRequestMask as array elements (see the
             * header): the camera anchor, target and control word. */
            g_nSpyroSwingEulerTargetY[29] = (int)&spyc->posX;
            g_nSpyroSwingEulerTargetY[30] = (int)&g_anCameraActiveTargetParams;
            g_nSpyroSwingEulerTargetY[19] = /* g_dwSpyroRequestMask */
                (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_CAMERA);
            g_anCameraActiveTargetParams.w[2] = 0xC00;
            g_anCameraActiveTargetParams.w[1] = 0x80;
            g_anCameraActiveTargetParams.w[4] = -0x100;
            g_anCameraActiveTargetParams.w[3] = 0;
            g_anCameraActiveTargetParams.w[5] = 0;
            g_anCameraActiveTargetParams.w[0] = 0x800 - g_bSpyroPersistentEulerZ * 16;
            r = TweenActorAlongPath(actor, st->path, st->look, &st->laps,
                              st->speed, TWEEN_FLY);
            if (r == TWEEN_WRAPPED) {
              st->curPath->cur = 2;
              g_nCinematicActive = 0;
              actor->subMode = 1;
            } else if (r == one) { /* TWEEN_NEXT_LEG */
              st->speed -= LTHIEF_INTRO_DRAG;
              if (st->speed < LTHIEF_INTRO_MIN_SPEED) {
                st->speed = LTHIEF_INTRO_MIN_SPEED;
              }
            }
          }
          yaw = (actor->rotZ - yaw) & 0xFF;
          if (yaw > 0x80) {
            yaw -= 0x100;
          }
          st->rollVel += ((-yaw << 8) - (st->rollVel << 3) - st->roll) >> 6;
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
        }
        break;

      case LTHIEF_KNOCKED:
        if (DriveActorTrackedMove(actor, &st->speed, st->knockHeading, &st->knockZSpeed,
                          0xC, 0xC) == MOVED_LANDED) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }

    /* Type 179: loot critter (CritterState). */
    case ACTOR_TYPE_LOOT_CRITTER: {
      CritterState *st = actor->state;
      int speed;
      int *pos;
      int turn;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != LCRITTER_KNOCKED) {
        st->knockSpeed = LCRITTER_KNOCK_SPEED;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knockSpeed = LCRITTER_CHARGE_KNOCK_SPEED;
        }
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8),
            g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->knockZSpeed = LCRITTER_KNOCK_VZ;
        if (actor->dropKind == DROP_GEM_ROLL) {
          actor->dropKind = ACTOR_TYPE_SPARX_FOOD;
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorLiveKill(actor);
        }
        ENTER_POSE(actor, LCRITTER_KNOCKED);
      }
      if (TICK_DOWN(st->voiceTimer) && actor->mode == one) {
        int v;

        st->voiceTimer = (int)GetRandomU32() % 3 + 4;
        v = (int)GetRandomU32() % 5;
        while (v == st->lastVoice) {
          v = (int)GetRandomU32() % 5;
        }
        PlayActorMeshVoice(actor, v, 0);
        st->lastVoice = v;
      }
      if (st->attach != 0) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, &g_pActorListBase[st->attach].posX);
      }
      if (actor->nextAnim == LCRITTER_WATCH && actor->cullRadius != ACTOR_CULL_HIDDEN) {
        if (actor->nextFrame == 2 && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[LCRITTER_VOICE_TAUNT_EARLY]) == 0) {
          PlayActorMeshVoice(actor, LCRITTER_VOICE_TAUNT_EARLY, 0);
        }
        if (actor->nextFrame == 0xC && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[LCRITTER_VOICE_TAUNT_LATE]) == 0) {
          PlayActorMeshVoice(actor, LCRITTER_VOICE_TAUNT_LATE, 0);
        }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);

      switch (actor->mode) {
      case LCRITTER_WATCH:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &SPY.posX) < LCRITTER_NOTICE_RANGE) {
          actor->subMode = LCRITTER_HOME_ORBIT;
          ENTER_POSE(actor, LCRITTER_FLEE);
        }
        break;

      case LCRITTER_FLEE:
        if (st->hasHome != 0) {
          pos = &actor->posX;
          speed = LCRITTER_FLEE_BASE_SPEED;
          speed -= ApproxDist2D(pos, &SPY.posX) >> 5;
          if (speed < LCRITTER_FLEE_MIN_SPEED) {
            speed = LCRITTER_FLEE_MIN_SPEED;
          }
          if (speed > LCRITTER_FLEE_MAX_SPEED) {
            speed = LCRITTER_FLEE_MAX_SPEED;
          }
          switch (actor->subMode) {
          case LCRITTER_HOME_ORBIT: {
            int home = ArcTan2(FP_X(st->path, 0) - actor->posX,
                                     FP_Y(st->path, 0) - actor->posY, ATAN_ANGLE8);
            int lim;
            int a;
            int b;

            if (ApproxDist2D(pos, FP_PT(st->path, 0)) < LCRITTER_HOME_NEAR) {
              TurnActorYawTowardAngle(actor,
                            ArcTan2(actor->posX - SPY.posX,
                                          actor->posY - SPY.posY, ATAN_ANGLE8),
                            8, 0, YAW_HOLD_IN_BAND);
              MoveActorAlongHeading(actor, speed, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
              break;
            }
            lim = LCRITTER_ORBIT_FACING;
            a = ArcTan2(FP_X(st->path, st->path->cur) - SPY.posX,
                              FP_Y(st->path, st->path->cur) - SPY.posY, ATAN_ANGLE8);
            b = ArcTan2(actor->posX - FP_X(st->path, 0),
                              actor->posY - FP_Y(st->path, 0), ATAN_ANGLE8);
            if (actor->nextAnim == 4) {
              lim = LCRITTER_ORBIT_FACING_WIDE;
            }
            if (AbsAngleDelta8(a, b) > lim &&
                ApproxDist2D(pos, &SPY.posX) < LCRITTER_ORBIT_RANGE) {
              turn = SteerActorAroundPoint(actor, FP_PT(st->path, 0), LCRITTER_ORBIT_RADIUS, a, 5,
                                   speed, 0xE, 0x14, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, MOVE_FOLLOW_FLOOR);
            }
            if (turn > LCRITTER_STUCK_TURN) {
              st->stuck++;
            } else {
              st->stuck = 0;
            }
            if (turn < LCRITTER_ORBIT_SETTLED) {
              ENTER_POSE(actor, LCRITTER_WATCH);
            }
            if (st->stuck >= LCRITTER_STUCK_FRAMES &&
                AbsAngleDelta8(home, ArcTan2(SPY.posX - actor->posX,
                                                  SPY.posY - actor->posY, ATAN_ANGLE8)) >
                    LCRITTER_BREAKOUT_FACING) {
              actor->subMode = LCRITTER_HOME_BREAKOUT;
            }
            break;
          }

          case LCRITTER_HOME_BREAKOUT: {
            int a = ArcTan2(FP_X(st->path, 0) - SPY.posX,
                                  FP_Y(st->path, 0) - SPY.posY, ATAN_ANGLE8);
            int b = ArcTan2(FP_X(st->path, 0) - actor->posX,
                                  FP_Y(st->path, 0) - actor->posY, ATAN_ANGLE8);

            TurnActorYawTowardAngle(actor, a, 8, 0, YAW_HOLD_IN_BAND);
            MoveActorAlongHeading(actor, speed, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (AbsAngleDelta8(a, b) > LCRITTER_BREAKOUT_TURN) {
              actor->subMode = LCRITTER_HOME_ORBIT;
            }
            if (ApproxDist2D(pos, FP_PT(st->path, 0)) > LCRITTER_ORBIT_RADIUS) {
              actor->subMode = LCRITTER_HOME_ORBIT;
            }
            break;
          }
          }
        } else {
          switch (st->mode) {
          case LCRITTER_RUN_PICK: {
            int dir;

            FindNearestPathPointToSpyro(st->path, &critterSpyroNode);
            if (st->path->cur == critterSpyroNode) {
              dir = -st->path->dir;
            } else {
              if (critterSpyroNode < st->path->cur) {
                critterSpyroNode += st->path->count;
              }
              critterSpyroNode -= st->path->cur;
              dir = -1;
              if (critterSpyroNode < st->path->count >> 1) {
                dir = 1;
              }
            }
            if (dir != st->path->dir) {
              st->path->dir = dir;
            }
            st->mode = LCRITTER_RUN_DASH;
            break;
          }

          case LCRITTER_RUN_OPPOSITE: {
            int dir;
            int r;

            if (FindNearestPathPointToSpyro(st->path, &critterAwayNode) > LCRITTER_WANDER_RANGE) {
              st->mode = LCRITTER_RUN_WANDER;
            }
            critterAwayNode = (critterAwayNode + (st->path->count >> 1)) % st->path->count;
            if (st->path->cur == critterAwayNode) {
              dir = -st->path->dir;
            } else {
              if (critterAwayNode < st->path->cur) {
                critterAwayNode += st->path->count;
              }
              critterAwayNode -= st->path->cur;
              dir = 1;
              if (critterAwayNode < st->path->count >> 1) {
                dir = -1;
              }
            }
            if (dir != st->path->dir) {
              st->path->dir = dir;
            }
            r = PivotActorAlongPath(actor, st->path, LCRITTER_PIVOT_THRESH, LCRITTER_WANDER_SPEED, 0, 8, 0x1E, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (r & PATH_ARRIVED_FLAG) {
              r &= ~PATH_ARRIVED_FLAG;
              if (st->path->dir == -1) {
                if (r == one) {
                  ENTER_POSE(actor, LCRITTER_WATCH);
                }
              } else if (r == st->path->count - 1) {
                ENTER_POSE(actor, LCRITTER_WATCH);
              }
            }
            if (ApproxDist2D(&actor->posX, &SPY.posX) < LCRITTER_NOTICE_RANGE) {
              st->mode = LCRITTER_RUN_PICK;
            }
            break;
          }

          case LCRITTER_RUN_WANDER: {
            int r = PivotActorAlongPath(actor, st->path, LCRITTER_PIVOT_THRESH, LCRITTER_WANDER_SPEED, 0, 8, 0x1E,
                                  PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);

            if (r & PATH_ARRIVED_FLAG) {
              r &= ~PATH_ARRIVED_FLAG;
              if (st->path->dir == -1) {
                if (r == one) {
                  ENTER_POSE(actor, LCRITTER_WATCH);
                }
              } else if (r == st->path->count - 1) {
                ENTER_POSE(actor, LCRITTER_WATCH);
              }
            }
            if (ApproxDist2D(&actor->posX, &SPY.posX) < LCRITTER_NOTICE_RANGE) {
              st->mode = LCRITTER_RUN_PICK;
            }
            break;
          }

          case LCRITTER_RUN_DASH:
            if (ApproxDist2D(&actor->posX, &SPY.posX) > LCRITTER_CLEAR_RANGE) {
              st->mode = one;
            }
            PivotActorAlongPath(actor, st->path, LCRITTER_PIVOT_THRESH, LCRITTER_DASH_SPEED, 0, 8, 0x1E, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            break;
          }
        }
        break;

      case LCRITTER_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed,
                      LCRITTER_KNOCK_DRAG, LCRITTER_KNOCK_GRAVITY);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LCRITTER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      break;
    }

    {
    int swoopPos[3];
    int settleOffset[3];
    int twinklePos[3];
    int twinklePos2[3];
    int ps[6];
    int w416[3];
    int w432[3];
    int w448[3];
    int fusePos[3];
    int fuseVel[3];
    int y400[3];
    int w512[3];
    int svR[3];
    int svS[3];
    int w560[3];

    /* Type 181: loot vortex (LootVortexState). */
    case ACTOR_TYPE_LOOT_VORTEX: {
      LootVortexState *st = actor->state;
      unsigned int ks = g_pActorListBase[st->key].mode;
      int kt = g_pActorListBase[st->key].type;
      int open = 0;

      if (ks >= ACTOR_MODE_DORMANT || (kt == ACTOR_TYPE_LANE_PATROLLER && ks == LANEPAT_KNOCKED)) {
        open = 1;
      }
      if (actor->subMode == 0) {
        if (open) {
          actor->rotX = (unsigned short)g_anCosineLut[st->phase] >> 7;
          actor->rotY = (unsigned short)g_anSineLut[st->phase] >> 7;
          st->phase += g_nFrameStep * 2;
        }
        if ((ApproxDist2D(&actor->posX, &SPY.posX) < LVORTEX_TOUCH_RANGE &&
             ABS2(SPY_DZ(actor)) < LVORTEX_TOUCH_RANGE && open) ||
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
          g_nLevelKeyState = one;
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->updateDist = ACTOR_UPDATE_ALWAYS;
          actor->drawn = 0;
          actor->groundCode = 0;
          actor->scale = 0x40;
          actor->gateChunk = GATE_CHUNK_ALWAYS;
          actor->subMode = 1;
          st->mode = one;
        }
        if (st->spark != ROT_BILLBOARD_NONE && actor->cullRadius != ACTOR_CULL_HIDDEN) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset4, g_aRotBillboards[st->spark].pos);
          AddVector(g_aRotBillboards[st->spark].pos, g_aRotBillboards[st->spark].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->spark].life < 5) {
            st->spark = ROT_BILLBOARD_NONE;
          }
        } else if (st->timer >= ROT_BILLBOARD_GLINT_TICK && actor->cullRadius != ACTOR_CULL_HIDDEN) {
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
        st->mode = one;
        g_nLevelKeyState = one;
        break;
      }

      case LVORTEX_OPEN:
        if (((unsigned)SPY.state <= SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG || SPY.state == SPYRO_ST_RUN) &&
            IsPointInRotatedBox(&SPY.posX, &st->box) &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(g_pActorListBase[st->target].posX - SPY.posX,
                                        g_pActorListBase[st->target].posY - SPY.posY,
                                        ATAN_ANGLE8)) < LOCKCHEST_FACING_ANGLE) {
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

              st->timer = LOCKCHEST_KEY_TURN_TICK;
              d = AbsAngleDelta8(g_pActorListBase[st->target].rotZ,
                                (unsigned char)(SPY.bodyRotZ + 0x40));
              actor->rotY = 0xC0;
              r = g_bSpyroPersistentEulerZ + 0x40;
              actor->rotZ = r + d * (st->timer - LOCKCHEST_KEY_TURN_TICK) / 64;
            } else {
              g_nCutsceneId[st->seen] = one;
            }
          }
          CopyVector(&actor->posX, &SPY.posX);
        }
        break;

      case LVORTEX_ACTIVE:
        if (g_dwPadPressed & PAD_CROSS) {
          st->timer = LOCKCHEST_SKIP_TICK;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        actor->cullRadius = 0x18;
        st->timer += g_nFrameStep;
        g_nCinematicActive = one;
        if (st->timer < LOCKCHEST_INSERT_TICK) {
          int *v = swoopPos;

          v[0] = LOCKCHEST_LOCK_FWD;
          v[1] = 0;
          v[2] = LOCKCHEST_LOCK_UP;
          RotateVectorByMatrix(actor->mtx, v, v);
          AddVector(v, v, &g_pActorListBase[st->target].posX);
          SubtractVector(v, v, &SPY.posX);
          v[2] -= 0x100;
          RShiftVector3(v, 5);
          ScaleVector3Sat(v, v, st->timer);
          DivideVector3(v, v, 6);
          AddVector(v, v, &SPY.posX);
          v[2] += 0x100;
          v[2] += (unsigned short)g_anSineLut[(st->timer << 7) / 192] << 16 >> 19;
          CopyVector(&actor->posX, v);
          g_nSpyroSwingEulerTargetY[0] =
              (unsigned short)g_anSineLut[(st->timer << 7) / 192] << 16 >> 19;
          if (st->timer < LOCKCHEST_KEY_SPIN_TICK) {
            int r;

            actor->rotY = -st->timer;
            r = g_bSpyroPersistentEulerZ + 0x80;
            actor->rotZ = r + st->timer * 4;
          } else if (st->timer < LOCKCHEST_KEY_TURN_TICK) {
            int r;

            actor->rotY = 0xC0;
            r = g_bSpyroPersistentEulerZ + 0x80;
            actor->rotZ = r + (st->timer - LOCKCHEST_KEY_SPIN_TICK) * 3;
          } else {
            int d = AbsAngleDelta8(g_pActorListBase[st->target].rotZ,
                                  (unsigned char)(g_bSpyroPersistentEulerZ + 0x40));
            int r = g_bSpyroPersistentEulerZ + 0x40;

            actor->rotZ = r + d * (st->timer - LOCKCHEST_KEY_TURN_TICK) / 64;
          }
        } else if (st->timer < LOCKCHEST_BURST_TICK) {
          int *w = settleOffset;

          actor->rotZ = g_pActorListBase[st->target].rotZ;
          w[0] = LOCKCHEST_LOCK_FWD - (st->timer - LOCKCHEST_INSERT_TICK) * 8;
          w[2] = LOCKCHEST_LOCK_UP;
          w[1] = 0;
          RotateVectorByMatrix(actor->mtx, w, w);
          AddVector(&actor->posX, w, &g_pActorListBase[st->target].posX);
        }
        if (st->timer >= LOCKCHEST_BURST_TICK || !((unsigned)SPY.state <= SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
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

        actor->soundRange = 0x20;
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

    /* Type 217: vault timer. While Spyro rides the vault lift it counts the
     * vault down on the HUD, then flags the vault cleared and cues the
     * exit. */
    case ACTOR_TYPE_VAULT_TIMER: {
      int *t = actor->state;

      if (g_nTotalTreasureCollected == 0x36B0 && SPY.state == SPYRO_ST_LIFT) {
        if (*t < 0x10) {
          g_nScreenFadeLevel = 0x10 - *t;
          if (g_nScreenFadeLevel >= 0x10) {
            g_nScreenFadeLevel = 0xF;
          }
        }
        if (TICK_DOWN(*t)) {
          g_abLevelExited[g_nLevelIndex] = 1;
          g_nCutsceneId[0] = CUTSCENE_LOOT_ENDING;
          g_nGamestate = GS_CUTSCENE;
          g_nSkipGamestateDraw = one;
          g_pPathTableHead->frame = 0;
        }
      }
    }
    /* fall through */

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

        twinklePos[0] = (GetRandomU32() & 0xFE) - 0x7F;
        twinklePos[1] = (GetRandomU32() & 0xFE) - 0x7F;
        twinklePos[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(twinklePos, twinklePos, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, twinklePos, 1);
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
            twinklePos2[0] = (GetRandomU32() & 0xFE) - 0x7F;
            twinklePos2[1] = (GetRandomU32() & 0xFE) - 0x7F;
            twinklePos2[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(twinklePos2, twinklePos2, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, twinklePos2, 1);
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
          w416[0] = (GetRandomU32() & 7) + 0x10;
          w416[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, ps, (int)w416);
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
          ZeroVector(w416);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)w416);
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
            CopyVector(w432, &st->prize->posX);
            w432[0] += g_anCosineLut[j * 43] >> 6;
            w432[1] += g_anSineLut[j * 43] >> 6;
            w432[2] += 0x28;
            w448[0] = g_anCosineLut[j * 43] >> 7;
            w448[1] = g_anSineLut[j * 43] >> 7;
            w448[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, w432, (int)w448);
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_SPLASH:
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
    case ACTOR_TYPE_HAZARD2_ENTER_FX:
    case ACTOR_TYPE_HAZARD2_DEATH_FX:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case ACTOR_TYPE_SPIN_CHEST: {
      SpinChestState *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[SPINCHEST_VOICE_SPIN]);
      int cur = g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0];
      int i;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) == ACTOR_HIT_FLAME) {
        actor->updateDist = ACTOR_UPDATE_ALWAYS;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[SPINCHEST_VOICE_SPIN], actor, SFX_MODE_3D,
                      &actor->voiceRow);
        if (actor->voiceRow != VOICE_ROW_NONE) {
          g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed <= SPINCHEST_MIN_SPEED) {
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
      }
      if (actor->subMode == SPINCHEST_INIT) {
        st->lid = g_pfnLevelSpawnActor(ACTOR_TYPE_SPIN_CHEST_LID, actor);
        st->lid->cullRadius = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = SPINCHEST_SPINNING;
        continue;
      }
      if (st->timer == 0 || --st->timer == 0) {
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->timer = SPINCHEST_FLAME_COOLDOWN;
          st->speed += SPINCHEST_FLAME_SPIN;
        }
      }
      st->scorchBand = StepActorScorchTint(actor, st->scorchBand);
      st->speed -= 1;
      if (pitch < cur && actor->voiceRow != VOICE_ROW_NONE) {
        g_anSpuVoiceRowPitchStep[actor->voiceRow & 0x7F][0] = -4;
      }
      if (st->speed < SPINCHEST_MIN_SPEED) {
        st->speed = SPINCHEST_MIN_SPEED;
        actor->updateDist = 0x10;
      } else {
        actor->updateDist = 0x40;
      }
      st->angle += (st->speed * g_nFrameStep) >> 1;
      st->lid->rotZ = st->angle >> 4;
      if (st->speed > SPINCHEST_BURST_SPEED || (actor->flags & ACTOR_HIT_SUPER)) {
        st->lid->subMode = LID_LAUNCH;
        PlayActorMeshVoice(actor, SPINCHEST_VOICE_LAUNCH, 0);
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
      }
      actor->flags = 0;
      break;
    }

    /* Type 392: spin chest lid. Spins on its chest (SpinChestState.lid);
     * when the chest bursts it is launched at the nearest enemy in front of
     * Spyro (or away from him when there is none) and breaks on what it
     * hits. */
    case ACTOR_TYPE_SPIN_CHEST_LID: {
      SpinChestLidState *st = actor->state;
      int i;

      if (actor->subMode == LID_LAUNCH) {
        Actor *best = 0;
        int bestAng = LID_TARGET_MAX_ANGLE;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= ACTOR_MODE_DORMANT || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(w416, &a->posX, &actor->posX);
          d = ABS2(w416[0]) + ABS2(w416[1]);
          if (d < LID_TARGET_PREFILTER &&
              (unsigned int)(VectorLength(w416, VEC_LEN_XY) - LID_TARGET_MIN_RANGE) <=
                  LID_TARGET_MAX_RANGE - LID_TARGET_MIN_RANGE &&
              (unsigned int)(w416[2] + LID_TARGET_DZ) <= 2 * LID_TARGET_DZ) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, ATAN_ANGLE12),
                              ArcTan2(w416[0], w416[1], ATAN_ANGLE12));
            if (d < bestAng) {
              bestAng = d;
              best = a;
            }
          }
        }
        if (best != 0) {
          SubtractVector(st->vel, &best->posX, &actor->posX);
          st->vel[2] += LID_AIM_RAISE;
          RShiftVector3(st->vel, 7);
        } else {
          SubtractVector(st->vel, &actor->posX, &SPY.posX);
          st->vel[2] = 0;
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, VEC_LEN_XY), LID_MISS_SPEED);
        }
        st->vel[2] += LID_LAUNCH_VZ;
        st->timer = 0;
        actor->subMode = LID_FLYING;
        actor->cullRadius = 0x20;
        actor->updateDist = ACTOR_UPDATE_ALWAYS;
        actor->rotX = 0xA;
        actor->rotY = 0;
        continue;
      }
      if (actor->subMode != LID_FLYING) {
        continue;
      }
      st->timer += g_nFrameStep;
      actor->rotZ += g_nFrameStep * LID_SPIN_STEP;
      if (st->vel[2] > -LID_MAX_FALL) {
        st->vel[2] -= (g_nFrameStep * 5) >> 1;
      }
      for (i = 0; i < g_nFrameStep; i++) {
        AddVector(&actor->posX, &actor->posX, st->vel);
        if (actor->posZ < 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += LID_HIT_RAISE;
        if (st->timer >= LID_ARM_TICKS &&
            (CollideSphereWithWorldAndActors(&actor->posX, LID_HIT_RADIUS, LID_HIT_RADIUS) != 0 ||
             DispatchActorContactAtSphere(&actor->posX, LID_CONTACT_RADIUS, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, CONTACT_STOP_AT_SOLID) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, LID_BLAST_RADIUS, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, CONTACT_STOP_AT_SOLID);
          for (i = 0; i < LID_SHARD_PAIRS; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
          }
          for (i = 0; i < LID_SPARKLES; i++) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          }
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, 0x18);
          PlayActorMeshVoice(actor, SPINLID_VOICE_BURST, 0);
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= LID_HIT_RAISE;
      }
      continue;
    }

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

        actor->soundRange = 0x20;
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

        SubtractVector(w432, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(w432, VEC_LEN_XY), w432[2], ATAN_ANGLE8);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < CANNON_LOW_PITCH_SPAN) {
          actor->rotY = CANNON_LOW_PITCH;
        }
        actor->rotZ = ArcTan2(w432[0], w432[1], ATAN_ANGLE8);
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
          fusePos[0] = 0;
          fusePos[1] = 0;
          fusePos[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, fusePos, fusePos);
          AddVector(fusePos, fusePos, &actor->posX);
          fuseVel[0] = 0;
          fuseVel[1] = 6;
          fuseVel[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, fuseVel, fuseVel);
          fuseVel[0] += (GetRandomU32() & 0xF) - 8;
          fuseVel[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, fusePos, (int)fuseVel);
          if (actor->voiceRow != VOICE_ROW_NONE) {
            continue;
          }
          PlayActorMeshVoice(actor, CANNON_VOICE_FUSE, 0);
          continue;
        }
        if (st->timer < CANNON_FIRE_TICK &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          fusePos[0] = 0;
          fusePos[1] = 0;
          fusePos[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, fusePos, fusePos);
          AddVector(fusePos, fusePos, &actor->posX);
          fuseVel[0] = 0;
          fuseVel[1] = ((st->timer - CANNON_SMOKE_TICKS) >> 3) + 9;
          fuseVel[2] = -0x12 - ((st->timer - CANNON_SMOKE_TICKS) >> 2);
          RotateVectorByMatrix(actor->mtx, fuseVel, fuseVel);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, fusePos, (int)fuseVel);
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
          SubtractVector(y400, PATH_PT(st->path, st->path->cur), &actor->posX);
          CopyVector(st->vel, y400);
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
            SubtractVector(y400, PATH_PT(st->path, st->path->cur), &actor->posX);
            RShiftVector3(y400, 1);
          } else {
            SubtractVector(y400, PATH_PT(st->path, st->path->cur),
                          (int *)((unsigned char *)st->path + ((st->path->cur << 4) - 8)));
            ScaleVector3Sat(y400, y400, g_anSineLut[st->timer * 2 - 6]);
            ScaleVector3Sat(w512, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(y400, y400, w512);
            RShiftVector3(y400, 0xA);
          }
          st->timer++;
        }
        VectorLength(y400, VEC_LEN_XYZ);
        actor->rotZ = ArcTan2_8bit(y400[0], y400[1]);
        actor->rotY = -ArcTan2_8bit(y400[2], VectorLength(y400, VEC_LEN_XY));
        AddVector(&actor->posX, &actor->posX, y400);
        ZeroVector(y400);
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)y400);
        continue;
      }
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
          w512[0] = (g_anCosineLut[bearing] * 3) >> 4;
          w512[1] = (g_anSineLut[bearing] * 3) >> 4;
          w512[2] = 0;
          svR[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svR[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(svS, svR, st->count - 1);
          LShiftVector3(svR, 1);
          arm = st->count - 1;
          svS[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          w560[0] = w512[0] * g_anCosineLut[arm * 2];
          w560[1] = w512[1] * g_anCosineLut[arm * 2];
          w560[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(svR, svR, st->index);
          SubtractVector(svS, svS, svR);
          swing = &g_anCosineLut[bearing];
          actor->posX = w512[0] * *swing;
          actor->posY = w512[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, w560);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, w512);
          AddVector(&actor->posX, &actor->posX, &st->sign->posX);
          SubtractVector(&actor->posX, &actor->posX, svS);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* ACTOR_TYPE_SPARK_DEBRIS (481..483). Tumbles under gravity, shedding sparks, until
     * its life runs out or it leaves the drawn set, then bursts and is
     * removed. */
    case ACTOR_TYPE_SPARK_DEBRIS:
    case ACTOR_TYPE_SPARK_DEBRIS + 1:
    case ACTOR_TYPE_SPARK_DEBRIS + 2: {
      Debris *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {
        actor->posX += st->vx;
        actor->posY += st->vy;
        st->vz -= 6;
        if (st->vz < -0x80) {
          st->vz = -0x80;
        }
        actor->posZ += st->vz;
        actor->rotX += st->spinX;
        actor->rotY += st->spinY;
        actor->rotZ += st->spinZ;
        if ((st->life & 3) == 0) {
          w560[0] = GetRandomU32() & 3;
          w560[1] = GetRandomU32() & 3;
          w560[2] = 0x14;
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)w560);
        }
        st->life--;
        break;
      }
      g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, 0x10);
      DespawnActorRecord(actor);
      continue;
    }
    }
}
  }
}
