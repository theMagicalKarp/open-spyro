/* Level20_UpdateActors -- per-frame actor update for level 20 (Misty
 * Bog). 0x8007AFBC, 38,748 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 1/3/7/10/13/18/26/30; the level-20 classes
 * are the gunner and its partner (28), the lurker with its tether (403), the
 * grabber that carries Spyro off (476) and a small animated prop (487). The
 * guard (137) and roaming ram (466) are shared with level 18.
 *
 * Load-bearing source forms:
 *   - Carriers: `sixteen` (s7) and, at the loop top, `base = g_anSparxHealthBlock` with
 *     `spy = base - 0x164` (fp). Making `base` a loop-top movable is what
 *     keeps 5, 8 and g_apActorMeshTable out of loop.c's hoist set; case 28's pose-5
 *     and pose-8 sites are ENTER_POSE_B (A285).
 *   - The hit blocks of 28/137/403/466 open with an empty do/while(0) so cse
 *     keeps spy->x fp-relative (A200).
 *   - Case 28 reads its partner through a pointer in one place and by index
 *     in the other; the two spellings give the original's two addu orders.
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

void Level20_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int sixteen;
  int dt;

  /* Scratch vectors, in first-use order. */
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
  int A256[3];
  int twinklePos[3];
  int A288[3];
  int A304[3];
  int sparkVel[3];
  int A336[3];
  int attachPt[3];
  int nearestPt[2];
  int A376[3];
  int letterSlot[3];
  int A408[3];

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
    sixteen = 0x10;
    base = (char *)g_anSparxHealthBlock;
    spy = (SpyroObj *)SPY_FROM_SPARX(base);

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
              int yaw = g_nCameraEulerPitch & ANGLE12_MASK;

              if (yaw > ANGLE12_HALF) {
                yaw -= ANGLE12_TURN;
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

    /* Butterfly (ButterflyState): flutters about its spawn point. Near a hurt
     * Spyro it becomes Sparx when Sparx is gone, else Sparx comes to eat it. */
    case ACTOR_TYPE_BUTTERFLY: {
      ButterflyState *st;

      EBB_BREAK();
      st = actor->state;
      if (SPY.sparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, (int *)SPY_FROM_SPARX(base)) < BUTTERFLY_CALL_RANGE) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < BUTTERFLY_CALL_DZ : (SPY.posZ - height) < BUTTERFLY_CALL_DZ) {
          if (g_pSparxActor == 0) {
            if (g_nSparxHealth >= SPARX_GONE) {
              Actor *sparx = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              g_pSparxActor = sparx;
              PlayActorMeshVoice(sparx, SPARX_VOICE_BUZZ, 0);
              g_nSparxHealth = SPARX_GREEN;
              DespawnActorRecord(actor);
              continue;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            SparxState *sparx = g_pSparxActor->state;

            if (sparx->target == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, ATAN_ANGLE8)) <
                  0) {
                st->circleDir = 1;
              } else {
                st->circleDir = -1;
              }
              g_pSparxActor->subMode = SPARX_SUB_CHASE;
              sparx->target = actor;
              sparx->timer = RandomIntInRange(SPARX_CHASE_MIN_FRAMES, SPARX_CHASE_MAX_FRAMES);
              actor->mode = BUTTERFLY_CIRCLE;
            }
          }
        }
      }

      if (!(actor->cullRadius & ACTOR_CULL_ALWAYS_BIT)) {
        switch (actor->mode) {
        case BUTTERFLY_FLUTTER:
          if (TICK_DOWN(st->offscreenTimer) == 0 || actor->drawn != 0) {
            int moved;
            int blocked;
            int step;
            int z;
            unsigned int spd;
            int *at;

            if (TICK_DOWN(st->headingTimer) != 0) {
              int turn = RandomSignedInRange(BUTTERFLY_VEER_MIN, BUTTERFLY_VEER_MAX);

              if (GetRandomU32() & 1) {
                turn = -turn;
              }
              st->heading = WrapAddAngle8(st->heading, turn);
              st->headingTimer = RandomIntInRange(BUTTERFLY_VEER_MIN_FRAMES, BUTTERFLY_VEER_MAX_FRAMES);
            }

            if (TICK_DOWN(st->climbTimer) != 0) {
              st->vz = RandomSignedInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
              st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
            }

            spd = st->vz;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            EBB_BREAK();
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < BUTTERFLY_FLUTTER_MIN_HEIGHT) {
              st->vz = RandomIntInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
              st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
            }

            if (BUTTERFLY_FLUTTER_MAX_HEIGHT < step && step < BUTTERFLY_DROP_HEIGHT) {
              st->vz = -RandomIntInRange(BUTTERFLY_FLUTTER_VZ_MIN, BUTTERFLY_FLUTTER_VZ_MAX);
              st->climbTimer = RandomIntInRange(BUTTERFLY_FLUTTER_CLIMB_MIN_FRAMES, BUTTERFLY_FLUTTER_CLIMB_MAX_FRAMES);
            }

            moved = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, YAW_STEP_IN_BAND);
            blocked = MoveActorAlongHeading(actor, BUTTERFLY_SPEED, 0, BUTTERFLY_RADIUS, MOVE_LIFT);

            if (moved != 0) {
              if (blocked != 0) {
                st->heading =
                    WrapAddAngle8(st->heading, RandomIntInRange(BUTTERFLY_BOUNCE_MIN, BUTTERFLY_BOUNCE_MAX));
                st->headingTimer = RandomIntInRange(BUTTERFLY_BOUNCE_MIN_FRAMES, BUTTERFLY_BOUNCE_MAX_FRAMES);
              }
            }

            if (ApproxDist2D(&actor->posX, st->home) > BUTTERFLY_LEASH) {
              st->heading = ArcTan2(st->home[0] - actor->posX,
                                          st->home[1] - actor->posY, ATAN_ANGLE8);
              st->headingTimer = BUTTERFLY_LEASH_TURN_FRAMES;
            }
          } else {
            DespawnActorRecord(actor);
            continue;
          }
          break;

        case BUTTERFLY_CIRCLE:
          SteerActorAroundPoint(actor, &spy->posX, BUTTERFLY_CIRCLE_RADIUS,
                        WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                    actor->posY - spy->posY, ATAN_ANGLE8),
                                      st->circleDir * 8),
                        8, BUTTERFLY_CIRCLE_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);

          if (g_pSparxActor == 0 || g_pSparxActor->subMode == SPARX_SUB_PARKED) {
            actor->mode = BUTTERFLY_FLUTTER;
            break;
          }

          if (TICK_DOWN(st->climbTimer) != 0) {
            st->vz = RandomSignedInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
            st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
          }

          {
            int step;
            int z;
            unsigned int spd;

            spd = st->vz;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            EBB_BREAK();
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < BUTTERFLY_CIRCLE_MIN_HEIGHT) {
              st->vz = RandomIntInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
              st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
            }

            if (BUTTERFLY_CIRCLE_MAX_HEIGHT < step && step < BUTTERFLY_DROP_HEIGHT) {
              st->vz = -RandomIntInRange(BUTTERFLY_CIRCLE_VZ_MIN, BUTTERFLY_CIRCLE_VZ_MAX);
              st->climbTimer = RandomIntInRange(BUTTERFLY_CIRCLE_CLIMB_MIN_FRAMES, BUTTERFLY_CIRCLE_CLIMB_MAX_FRAMES);
            }
          }
          break;
        }
      }

      break;
    }

    /* Type 28: gunner. Wakes its partner, turns to it, then fires. */
    case ACTOR_TYPE_GUNNER: {
      GunnerState *st = actor->state;

      TickActorFlameGlow(actor);
      EBB_BREAK();
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode != GUNNER_KNOCKED) {
        if (actor->flags & ACTOR_HIT_SUPER) {
          st->knockSpeed = 0;
          st->knockZSpeed = GUNNER_SUPER_KNOCK_VZ;
        } else {
          st->knockSpeed = GUNNER_KNOCK_SPEED;
          st->knockZSpeed = GUNNER_KNOCK_VZ;
          st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE_B(actor, GUNNER_KNOCKED);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case GUNNER_IDLE:
        if (st->mode == GUNNER_ROLE_GUARD) {
          ENTER_POSE(actor, GUNNER_GUARD);
        } else if (st->mode == GUNNER_ROLE_PATROL) {
          Actor *partner = &g_pActorListBase[st->partner];

          if (partner->mode != CHARGER_IDLE) {
            st->mode = GUNNER_ROLE_GUARD;
            ENTER_POSE(actor, GUNNER_GUARD);
          }
          ANIM_GO(actor, GUNNER_ADVANCE);
          PivotActorAlongPath(actor, st->path, GUNNER_PIVOT_THRESH, GUNNER_WALK_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        } else if (st->mode == GUNNER_ROLE_LOOKOUT) {
          ANIM_GO(actor, GUNNER_IDLE);
          if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < GUNNER_STARTLE_RANGE &&
              g_pActorListBase[st->partner].mode == ONESHOT_IDLE) {
            ENTER_POSE(actor, GUNNER_STARTLE);
          }
          if (actor->subMode != 0) {
            ENTER_POSE(actor, GUNNER_WAKE);
          }
        }
        break;

      case GUNNER_WAKE:
        if (actor->nextFrame >= GUNNER_WAKE_FRAME && g_pActorListBase[st->partner].mode == ONESHOT_IDLE) {
          g_pActorListBase[st->partner].mode = ONESHOT_PLAY;
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, GUNNER_TURN);
        }
        break;

      case GUNNER_TURN:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, ATAN_ANGLE8),
            4, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          st->timer = GUNNER_CHAT_TICKS;
          ENTER_POSE(actor, GUNNER_CHAT);
        }
        break;

      case GUNNER_CHAT:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, ATAN_ANGLE8),
            6, 0, YAW_HOLD_IN_BAND);
        if ((TICK_DOWN(st->timer) != 0 ||
             ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < GUNNER_FIRE_RANGE) &&
            g_nActorAnimDone != 0) {
          if (actor->anim != GUNNER_GUARD) {
            actor->frameProgress = ANIM_RATE_TICKS(8);
            actor->frameRate = ANIM_RATE_TICKS(8);
            actor->anim = actor->nextAnim;
            actor->nextAnim = GUNNER_GUARD;
            actor->frame = actor->nextFrame;
            actor->nextFrame = 0;
            OnActorAnimQueued(actor);
          }
          actor->mode = GUNNER_GUARD;
          continue;
        }
        break;

      case GUNNER_GUARD: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int r = st->range;

        if (r < 0x20) {
          r <<= 10;
        }
        if (d < r && g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
          st->timer = GUNNER_ADVANCE_TICKS;
          ENTER_POSE(actor, GUNNER_ADVANCE);
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && d < GUNNER_FIRE_RANGE) {
          st->timer = GUNNER_ADVANCE_TICKS;
          ENTER_POSE_B(actor, GUNNER_FIRE);
        }
        break;
      }

      case GUNNER_KNOCKED: {
        int r = DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, GUNNER_KNOCK_DRAG, sixteen);

        if ((g_nGroundSurfaceType & TRIGGER_REGION_MASK) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < GUNNER_SPLASH_DZ : h - z < GUNNER_SPLASH_DZ) {
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
            st->timer = GUNNER_SINK_TICKS;
            actor->mode = GUNNER_SINK;
            continue;
          }
        }
        if (r == MOVED_LANDED && st->hit == 0) {
          st->hit = 1;
          PlayActorMeshVoice(actor, GUNNER_VOICE_LAND, 0);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, GUNNER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      case GUNNER_ADVANCE:
        if (TICK_DOWN(st->timer) != 0 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < GUNNER_ADVANCE_STOP_RANGE) {
          st->range = 0;
          st->timer = 0;
          ENTER_POSE_B(actor, GUNNER_FIRE);
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0x14, YAW_STEP_IN_BAND) != 0 &&
            MoveActorAlongHeading(actor, GUNNER_WALK_SPEED, GUNNER_MOVE_RADIUS, GUNNER_MOVE_RADIUS, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) != 0) {
          st->timer = 0;
          ENTER_POSE(actor, GUNNER_GUARD);
        }
        break;

      case GUNNER_FIRE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, GUNNER_GUARD);
        }
        break;

      case GUNNER_STARTLE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, GUNNER_GUARD);
        }
        break;

      case GUNNER_SINK:
        if (TICK_DOWN(st->timer) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= GUNNER_SINK_STEP;
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
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
          st->angle = g_bSpyroPersistentEulerZ - ANGLE8_QUARTER;
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

    /* Bouncing chest fragments (ChestShardState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1:
    case ACTOR_TYPE_LIFE_CHEST_SHARD:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 1: {
      ChestShardState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, DEBRIS_RADIUS, DEBRIS_RADIUS)) {
          int *nrm = &g_anCollisionHitNormal;
          int dot;

          SetVector3Magnitude(nrm, FIXED_ONE);
          dot = st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionHitNormalY +
                    st->vel[2] * g_nCollisionHitNormalZ >>
                11;
          if (dot < 0) {
            ScaleVector3ByRatio(nrm, FIXED_ONE, (dot >> 2) - (dot));
            st->vel[0] += nrm[0];
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
    case ACTOR_TYPE_SEED_POD_SHARD + 2:
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
                               ANGLE8_HALF;
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
              if (actor->nextFrame >= SPARX_CATCH_FRAME) {
                actor->subMode += 1;
              }

              {
                int tilt = actor->rotY;
                int back = -tilt;

                if ((unsigned int)((tilt - 3) & ANGLE8_MASK) < 0xFB) {
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
                      (actor->rotZ + (bp->circleDir * SPARX_CATCH_LEAD)) & ANGLE8_MASK,
                      (CAMERA_YAW8 + ANGLE8_HALF) &
                          ANGLE8_MASK) < SPARX_CATCH_ALIGN))) {
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
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], ATAN_ANGLE8), SPARX_SEEK_TURN, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(svA, VEC_LEN_XY), svA[2], ATAN_ANGLE8);
            actor->rotY = StepAngle8Toward(actor->rotY & ANGLE8_MASK, 0, SPARX_SEEK_MAX_PITCH);
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
              TurnActorYawTowardAngle(actor, ArcTan2(sparxToTarget[0], sparxToTarget[1], ATAN_ANGLE8), SPARX_SEEK_TURN, 0, YAW_HOLD_IN_BAND);
              actor->rotY = ArcTan2(VectorLength(sparxToTarget, VEC_LEN_XY), sparxToTarget[2], ATAN_ANGLE8);
              actor->rotY = StepAngle8Toward(actor->rotY & ANGLE8_MASK, 0, SPARX_SEEK_MAX_PITCH);
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
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, SPARX_HOVER_TURN, 0, YAW_HOLD_IN_BAND);
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

    /* Type 137: guard. Patrols, waits or lurks until Spyro comes within
     * range, then turns on him and charges; after a charge the camera is
     * pulled in on it, and a hit knocks it back. */
    case ACTOR_TYPE_CHARGING_GUARD: {
      ChargingGuardState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != CHARGER_KNOCKED) {
        EBB_BREAK();
        st->knockHeading = StepAngle8Toward(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, 0x20);
        st->speed = CHARGER_KNOCK_SPEED;
        st->turn = 0;
        if (actor->mode != CHARGER_IDLE) {
          *st->chargeLock = 0;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, CHARGER_KNOCKED);
      }
      if (st->init == 0) {
        st->init = 1;
        if (st->lurk != 0 && ApproxDist2D(&actor->posX, &SPY.posX) < CHARGER_LURK_RANGE) {
          if (actor->dropKind != DROP_GEM_ROLL) {
            SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
            MarkActorLiveKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      if (TICK_DOWN(st->camHold) == 0) {
        int d = ApproxDist2D(&actor->posX, &spy->posX) + CHARGER_CAM_PULLBACK;

        if (st->camDist < d) {
          st->camDist = d;
        }
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        g_anCameraActiveTargetParams.w[CAMP_YAW] = -ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, ATAN_ANGLE8) *
                          16;
        g_anCameraActiveTargetParams.w[CAMP_DISTANCE] = st->camDist;
      }

      switch (actor->mode) {
      case CHARGER_IDLE: {
        int *p;

        TICK_DOWN(st->cooldown);
        switch (st->mode) {
        case CHARGER_ROLE_STAND:
          TrackEntityYawTowardSpyro(actor, 0xA, 0, YAW_HOLD_IN_BAND);
          break;

        case CHARGER_ROLE_PATROL:
          ANIM_GO(actor, CHARGER_CHARGE);
          if (st->speed < CHARGER_PATROL_MIN_SPEED) {
            st->speed = CHARGER_PATROL_MIN_SPEED;
          }
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < CHARGER_NODE_ARRIVE) {
            int r;
            PathData *pp;

            do {
              pp = st->path;
              r = RandomIntInRange(0, pp->count - 1);
              pp = st->path;
            } while (r == pp->cur);
            pp->cur = r;
            st->speed = RandomIntInRange(CHARGER_PATROL_SPEED_MIN, CHARGER_PATROL_SPEED_MAX);
          }
          if (TurnActorYawTowardAngle(
                  actor,
                  ArcTan2(
                      st->path->nodes[st->path->cur].x - actor->posX,
                      st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8),
                  6, 0x1E, YAW_STEP_IN_BAND) != 0) {
            MoveActorAlongHeading(actor, st->speed, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          }
          break;

        case CHARGER_ROLE_POST:
          if (st->orbitRadius == 0) {
            st->orbitRadius = ApproxDist2D(&actor->posX, &st->path->nodes[0].x);
          }
          break;

        case CHARGER_ROLE_FOLLOW: {
          int spd = CHARGER_FOLLOW_SPEED;
          int d = ApproxDist2D(&actor->posX, &g_pActorListBase[st->target].posX);

          ANIM_GO(actor, CHARGER_CHARGE);
          if (d < CHARGER_FOLLOW_NEAR) {
            spd = CHARGER_FOLLOW_NEAR_SPEED;
          } else if (d > CHARGER_FOLLOW_FAR) {
            spd = CHARGER_FOLLOW_FAR_SPEED;
          }
          TurnActorYawTowardAngle(actor,
                        ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                      g_pActorListBase[st->target].posY - actor->posY,
                                      ATAN_ANGLE8),
                        4, 0, YAW_HOLD_IN_BAND);
          MoveActorAlongHeading(actor, spd, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          break;
        }
        }
        if (st->mode == CHARGER_ROLE_PATROL || st->mode == CHARGER_ROLE_FOLLOW) {
          p = &st->path->nodes[0].x;
        } else {
          p = &actor->posX;
        }
        if (st->mode == CHARGER_ROLE_POST) {
          if (ApproxDist2D(&spy->posX, &st->path->nodes[1].x) < CHARGER_POST_TRIGGER_RANGE ||
              ApproxDist2D(&spy->posX, &st->path->nodes[2].x) < CHARGER_POST_TRIGGER_RANGE) {
            st->charging = 0;
            actor->mode = CHARGER_CHARGE;
            continue;
          }
          break;
        }
        if (st->cooldown == 0 && *st->chargeLock == 0 && g_nSpyroAirborneFrames == 0) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if ((dz > 0 ? dz < CHARGER_SIGHT_DZ
                      : SPY.posZ - (actor->posZ - actor->floorOffset) < CHARGER_SIGHT_DZ) &&
              ApproxDist2D(p, &SPY.posX) < st->range << 10) {
            *st->chargeLock = 1;
            st->timer = 0;
            st->charging = 0;
            if (st->mode == CHARGER_ROLE_FOLLOW) {
              st->mode = CHARGER_ROLE_PATROL;
              CopyVector(st->vec, p);
            }
            actor->mode = CHARGER_CHARGE;
            continue;
          }
        }
        break;
      }

      case CHARGER_CHARGE: {
        int chargeSpeed;
        int dist = ApproxDist2D(&actor->posX, &spy->posX);

        if (TICK_DOWN(st->timer) == 0) {
          break;
        }
        ANIM_GO(actor, CHARGER_CHARGE);
        if (st->mode == CHARGER_ROLE_POST) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->orbitRadius,
                        ArcTan2(spy->posX - st->path->nodes[0].x,
                                      SPY.posY - st->path->nodes[0].y, ATAN_ANGLE8),
                        8, 0x12C, 0xE, 8, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);
          if (dist < CHARGER_RAM_RANGE) {
            st->speed = CHARGER_POST_RAM_SPEED;
            st->cam = 0;
            ENTER_POSE(actor, CHARGER_RAM);
          }
          break;
        }
        if (st->filter != 0) {
          Actor **pp = g_apActiveActorList;
          Actor *a;

          while ((a = *pp++) != 0) {
            int d;

            if (a->type == ACTOR_TYPE_LURKER || a->type == st->filter) {
              d = actor->posX - a->posX;
              if (d > 0 ? d < CHARGER_TRAMPLE_BOX : a->posX - actor->posX < CHARGER_TRAMPLE_BOX) {
                d = actor->posY - a->posY;
                if ((d > 0 ? d < CHARGER_TRAMPLE_BOX : a->posY - actor->posY < CHARGER_TRAMPLE_BOX) &&
                    ApproxDist2D(&actor->posX, &a->posX) < CHARGER_TRAMPLE_RANGE &&
                    AbsAngleDelta8(actor->rotZ,
                                  ArcTan2(a->posX - actor->posX,
                                                a->posY - actor->posY, ATAN_ANGLE8)) <
                        CHARGER_TRAMPLE_CONE) {
                  a->flags |= ACTOR_HIT_SUPER;
                }
              }
            }
          }
        }
        if (TICK_DOWN(st->cooldown) != 0) {
          st->knockHeading = SPY.bodyRotZ;
          st->cooldown = CHARGER_REAIM_FRAMES;
        }
        st->turn = (ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, ATAN_ANGLE8) -
                    st->knockHeading + 0x100) %
                   0x100;
        if (TurnActorYawTowardAngle(actor,
                          WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                      spy->posY - actor->posY,
                                                      ATAN_ANGLE8),
                                        st->turn),
                          5, 0x1E, YAW_STEP_IN_BAND) == 0) {
          if (st->charging == 0) {
            break;
          }
        } else {
          st->charging = 1;
        }
        chargeSpeed = CHARGER_CHARGE_SPEED;
        if (st->speedOverride != 0) {
          chargeSpeed = st->speedOverride;
        }
        if (MoveActorAlongHeading(actor, chargeSpeed, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) != 0) {
        give_up:
          *st->chargeLock = 0;
          st->cooldown = CHARGER_REST_FRAMES;
          actor->mode = CHARGER_GO_HOME;
          continue;
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, ATAN_ANGLE8)) >= CHARGER_GIVE_UP_FACING ||
            ApproxDist2D(&actor->posX, st->vec) > CHARGER_LEASH) {
          goto give_up;
        }
        if (dist < CHARGER_RAM_RANGE &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, ATAN_ANGLE8)) < CHARGER_RAM_CONE) {
          st->speed = CHARGER_RAM_SPEED;
          st->cam = 0;
          ENTER_POSE(actor, CHARGER_RAM);
        }
        break;
      }

      case CHARGER_KNOCKED:
        if (actor->frame < CHARGER_KNOCK_SPIN_FRAMES) {
          actor->rotZ += st->turn;
        }
        if (st->speed > CHARGER_KNOCK_DRAG) {
          st->speed -= CHARGER_KNOCK_DRAG;
          MoveActorAlongAngle(actor, st->knockHeading, st->speed, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, CHARGER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case CHARGER_RAM:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (MoveActorAlongHeading(actor, st->speed, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) == MOVED_BLOCKED) {
          *st->chargeLock = 0;
          st->cooldown = CHARGER_REST_FRAMES;
          actor->mode = CHARGER_GO_HOME;
          continue;
        }
        if (actor->frame >= CHARGER_RAM_DRAG_FRAME && st->speed > CHARGER_RAM_MIN_SPEED) {
          st->speed -= CHARGER_RAM_DRAG;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < CHARGER_HIT_RANGE) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < CHARGER_HIT_DZ
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < CHARGER_HIT_DZ) {
            if (st->cam == 0) {
              st->cam = 1;
              st->cooldown = CHARGER_HIT_REST_FRAMES;
              SPY.triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
              g_anSpyroRequestVelocity.x = LookupCosine(actor->rotZ * 16) * CHARGER_HIT_KNOCK_SPEED >> 12;
              g_nSpyroRequestVelocityY = LookupSine(actor->rotZ * 16) * CHARGER_HIT_KNOCK_SPEED >> 12;
              g_nSpyroRequestVelocityZ = CHARGER_HIT_KNOCK_VZ;
            } else if (st->cam == 1) {
              st->cam = 2;
              st->camHold = CHARGER_CAM_HOLD_FRAMES;
              SPY.requestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
              SPY.pCamAnchor = &actor->posX;
              SPY.pCamTarget = &g_anCameraActiveTargetParams;
              g_anCameraActiveTargetParams.w[CAMP_YAW] = -ArcTan2(SPY.posX - actor->posX,
                                               SPY.posY - actor->posY, ATAN_ANGLE8) *
                                16;
              g_anCameraActiveTargetParams.w[CAMP_DISTANCE] =
                  ApproxDist2D(&actor->posX, &SPY.posX) + CHARGER_CAM_HIT_PULLBACK;
              g_anCameraActiveTargetParams.w[CAMP_PITCH] = 0x64;
              g_anCameraActiveTargetParams.w[CAMP_ROLL] = 0;
              g_anCameraActiveTargetParams.w[CAMP_PITCH_BIAS] = -0xC8;
              g_anCameraActiveTargetParams.w[CAMP_YAW_BIAS] = 0;
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, CHARGER_TURN_BACK);
        }
        break;

      case CHARGER_TURN_BACK:
        if (g_nActorAnimDone != 0) {
          actor->rotZ = WrapAddAngle8(actor->rotZ, ANGLE8_HALF);
          if (st->cam == 0) {
            ENTER_POSE(actor, CHARGER_CHARGE);
          }
          *st->chargeLock = 0;
          ANIM_GO(actor, CHARGER_CHARGE);
          actor->mode = CHARGER_GO_HOME;
          continue;
        }
        break;

      case CHARGER_GO_HOME:
        ANIM_GO(actor, CHARGER_CHARGE);
        if (st->mode == CHARGER_ROLE_POST) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->orbitRadius,
                        ArcTan2(st->vec[0] - st->path->nodes[0].x,
                                      st->vec[1] - st->path->nodes[0].y, ATAN_ANGLE8),
                        1, CHARGER_POST_HOME_SPEED, 0xE, 8, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);
          if (ApproxDist2D(&actor->posX, st->vec) < CHARGER_POST_HOME_ARRIVE) {
            ENTER_POSE(actor, CHARGER_IDLE);
          }
          break;
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2_8bit(st->vec[0] - actor->posX,
                                        st->vec[1] - actor->posY),
                          4, 0x14, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongHeading(actor, CHARGER_HOME_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (ApproxDist2D(&actor->posX, st->vec) < CHARGER_HOME_ARRIVE) {
          ENTER_POSE(actor, CHARGER_IDLE);
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

    /* Breakable chest (BreakState): one hit showers gems. */
    case ACTOR_TYPE_WOOD_CHEST: {
      BreakState *st = actor->state;

      if (st->snapToGround != 0) {
        SnapActorToGround(actor);
      }

      if (actor->flags & ACTOR_HIT_BY_SPYRO) {
        int i;

        actor->soundRange = SOUND_RANGE(0x8000);
        PlayActorMeshVoice(actor, WOODCHEST_VOICE_BREAK, 0);

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

      break;
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
          SubtractVector(A256, at, &spy->posX);
          A256[2] = (A256[2] * 3) >> 2;

          if (VectorLength(A256, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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

    /* Floating gem-value digits (GemDigitStateZ): fall and bounce off the world while their life lasts. */
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
      GemDigitStateZ *st = actor->state;
      int radius = actor->cullRadius;

      if (!(radius & 0x80) && radius != 0) {
        if (st->life > 0) {
          int z;

          actor->rotZ += 1;
          st->velZ -= 6;
          if (st->velZ < -0x80) {
            st->velZ = -0x80;
          }
          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          z = actor->posZ + st->velZ;
          actor->posZ = z;

          if (z >= 0x400) {
            if (CollideSphereWithWorldAndActors(&actor->posX, DEBRIS_RADIUS, DEBRIS_RADIUS) != 0) {
              int *nrm = &g_anCollisionHitNormal;
              int along;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);
              SetVector3Magnitude(nrm, FIXED_ONE);
              along = ((st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionHitNormalY) +
                       (st->velZ * g_nCollisionHitNormalZ)) >>
                      11;

              if (along < 0) {
                ScaleVector3ByRatio(nrm, FIXED_ONE, -along);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionHitNormalY);
                st->velZ += AS_I32(g_nCollisionHitNormalZ);
              }
            }
            st->life -= 1;
          } else {
            DespawnActorRecord(actor);
            continue;
          }
        } else {
          g_pfnLevelEmitParticles(0xA, EMIT_STREAK_BURST_WHITE, &actor->posX, 0);
          DespawnActorRecord(actor);
          continue;
        }
      }

      break;
    }

    /* Sparkling shard: one sparx per frame. */
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

        A256[0] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[1] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(A256, A256, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, A256, 1);
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
            twinklePos[0] = (GetRandomU32() & 0xFE) - 0x7F;
            twinklePos[1] = (GetRandomU32() & 0xFE) - 0x7F;
            twinklePos[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(twinklePos, twinklePos, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, twinklePos, 1);
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
        if (actor->flags & ACTOR_HIT_BY_SPYRO) {
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
          ZeroVector(A288);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)A288);
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
            CopyVector(A304, &st->prize->posX);
            A304[0] += g_anCosineLut[j * 43] >> 6;
            A304[1] += g_anSineLut[j * 43] >> 6;
            A304[2] += 0x28;
            sparkVel[0] = g_anCosineLut[j * 43] >> 7;
            sparkVel[1] = g_anSineLut[j * 43] >> 7;
            sparkVel[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, A304, (int)sparkVel);
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_SPLASH:
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

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
        st->lid->cullRadius = CULL_RADIUS(0x1A00);
        actor->updateDist = UPDATE_DIST(0x4000);
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
        actor->updateDist = UPDATE_DIST(0x4000);
      } else {
        actor->updateDist = UPDATE_DIST(0x10000);
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
          SubtractVector(A288, &a->posX, &actor->posX);
          d = ABS2(A288[0]) + ABS2(A288[1]);
          if (d < LID_TARGET_PREFILTER &&
              (unsigned int)(VectorLength(A288, VEC_LEN_XY) - LID_TARGET_MIN_RANGE) <=
                  LID_TARGET_MAX_RANGE - LID_TARGET_MIN_RANGE &&
              (unsigned int)(A288[2] + LID_TARGET_DZ) <= 2 * LID_TARGET_DZ) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, ATAN_ANGLE12),
                              ArcTan2(A288[0], A288[1], ATAN_ANGLE12));
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
        actor->cullRadius = CULL_RADIUS(0x2000);
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

    /* Type 403: lurker. Roams its path, then pops up and drags Spyro with a
     * tether actor (type 31). */
    case ACTOR_TYPE_LURKER: {
      LurkerState *st = actor->state;
      int dist;

      SubtractVector(A304, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &spy->posX);
      if (dist < EXACT_DIST_RANGE) {
        dist = VectorLength(A304, VEC_LEN_XY);
      }

      if ((actor->flags & ACTOR_HIT_BY_SPYRO) && actor->mode != LURKER_KNOCKED &&
          actor->mode != LURKER_SINK) {
        EBB_BREAK();
        actor->flags = 0;
        st->stunTimer = LURKER_STUN_FRAMES;
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, ATAN_ANGLE8),
                                g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->knockZSpeed = LURKER_KNOCK_VZ;
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->knockSpeed = LURKER_FLAME_KNOCK_SPEED;
        } else {
          st->knockSpeed = LURKER_KNOCK_SPEED;
        }
        if (st->tether != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          if (g_pSparxActor != 0 && st->holdsSpyro != 0) {
            g_pSparxActor->subMode = SPARX_SUB_CHASE;
          }
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, LURKER_KNOCKED);
      }

      switch (actor->mode) {
      case LURKER_WATCH:
        if (dist < LURKER_NOTICE_RANGE && TICK_DOWN(st->coolDown) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < LURKER_LASH_DZ
                               : -SPY_DZ(actor) < LURKER_LASH_DZ) &&
            AbsAngleDelta8(CAMERA_YAW8,
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], ATAN_ANGLE8)) <
                LURKER_CAM_CONE &&
            dist < LURKER_LASH_RANGE &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, ATAN_ANGLE8)) < LURKER_LASH_CONE) {
          int k;

          st->tether = g_pfnLevelSpawnActor(ACTOR_TYPE_LURKER_TETHER, actor);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, A336);
          GetActorAttachPoint(actor, 1, attachPt);
          AddVector(&st->tether->posX, A336, attachPt);
          RShiftVector3(&st->tether->posX, 1);
          SubtractVector(A336, &st->tether->posX, &spy->posX);
          k = LURKER_TETHER_SCALE_NUM / VectorLength(A336, VEC_LEN_XYZ);
          if (k < 5) {
            k = 5;
          }
          if (k >= 0x80) {
            k = 0x7F;
          }
          st->tether->scale = k;
          st->tether->rotZ = actor->rotZ;
          actor->mode = LURKER_LASH;
          continue;
        }
        if (st->mode == LURKER_POST_ROAM) {
          if (dist > LURKER_NOTICE_RANGE) {
            if (TICK_DOWN(st->roamTimer) != 0) {
              PathData *p = st->path;
              int c = p->cur;

              do {
                st->path->cur = RandomIntInRange(0, p->count - 1);
                p = st->path;
              } while (c == p->cur);
              actor->mode = LURKER_AIM;
              continue;
            }
          } else if (dist >= LURKER_CHASE_MIN_RANGE) {
            FindNearestPathPointToSpyro(st->path, nearestPt);
            if (st->path->cur != nearestPt[0] &&
                dist - ApproxDist2D((int *)&g_anSpyroWorldPos,
                                     PATH_PT(st->path,
                                             nearestPt[0])) >=
                    LURKER_CHASE_GAIN) {
              st->path->cur = nearestPt[0];
              actor->mode = LURKER_AIM;
              continue;
            }
          }
        }
        if (AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, ATAN_ANGLE8)) >= LURKER_FACE_CONE) {
          actor->mode = LURKER_FACE;
          continue;
        }
        break;

      case LURKER_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, LURKER_KNOCK_DRAG, sixteen);
        if ((g_nGroundSurfaceType & TRIGGER_REGION_MASK) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < LURKER_SPLASH_DZ : h - z < LURKER_SPLASH_DZ) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);

            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->voiceRow);
            o->scale = 0x34;
            st->coolDown = 0xA;
            actor->mode = LURKER_SINK;
            continue;
          }
        }
        if (TICK_DOWN(st->stunTimer) != 0 || g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, LURKER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case LURKER_SPIN:
        actor->rotZ += 5;
        if (TICK_DOWN(st->roamTimer) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, LURKER_WATCH);
        }
        break;

      case LURKER_AIM:
        ANIM_GO(actor, LURKER_ANIM_TURN);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0xDAC) {
          actor->mode = LURKER_FACE;
          continue;
        }
        if (TurnActorYawTowardAngle(
                actor,
                ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                              FLY_Y(st->path, st->path->cur) - actor->posY,
                              ATAN_ANGLE8),
                3, 2, YAW_STEP_IN_BAND) != 0 &&
            g_nActorAnimDone != 0) {
          st->hopSpeed = ApproxDist2D(&actor->posX,
                                  PATH_PT(st->path,
                                          st->path->cur)) /
                    0x14;
          actor->mode++;
        }
        break;

      case LURKER_HOP:
        MoveActorAlongHeading(actor, st->hopSpeed, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        if (g_nActorAnimDone != 0) {
          st->roamTimer = RandomIntInRange(LURKER_ROAM_MIN_FRAMES, LURKER_ROAM_MAX_FRAMES);
          ENTER_POSE(actor, LURKER_WATCH);
        }
        break;

      case LURKER_FACE:
        ANIM_GO(actor, LURKER_ANIM_TURN);
        if (TrackEntityYawTowardSpyro(actor, 8, 2, YAW_STEP_IN_BAND) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, LURKER_WATCH);
        }
        break;

      case LURKER_LASH: {
        int d;
        int k;

        ANIM_GO(actor, LURKER_ANIM_LASH);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, attachPt);
        GetActorAttachPoint(actor, 1, A376);
        AddVector(&st->tether->posX, attachPt, A376);
        RShiftVector3(&st->tether->posX, 1);
        SubtractVector(attachPt, &st->tether->posX, (int *)&g_anSpyroWorldPos);
        d = VectorLength(attachPt, VEC_LEN_XYZ) + 0x1F4;
        if (d < LURKER_TETHER_MIN) {
          d = LURKER_TETHER_MIN;
        }
        if (d > LURKER_TETHER_MAX) {
          d = LURKER_TETHER_MAX;
        }
        k = LURKER_TETHER_SCALE_NUM / d;
        if (k < 5) {
          k = 5;
        }
        if (k >= 0x80) {
          k = 0x7F;
        }
        st->tether->scale = k;
        if (actor->nextFrame < 2) {
          st->tether->cullRadius = ACTOR_CULL_HIDDEN;
        } else {
          st->tether->cullRadius = CULL_RADIUS(0x2000);
        }
        if (actor->nextFrame >= 4 && dist < LURKER_LASH_RANGE) {
          SPY.triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
        }
        actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                     spy->posY - actor->posY, ATAN_ANGLE8);
        st->tether->rotZ = ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8);
        st->tether->rotY =
            ArcTan2(ApproxDist2D(&st->tether->posX, &spy->posX),
                          SPY.posZ - st->tether->posZ, ATAN_ANGLE8);
        st->tether->rotY = StepAngle8Toward(st->tether->rotY, 0, 0x20);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          st->coolDown = LURKER_LASH_REST;
          ENTER_POSE(actor, LURKER_WATCH);
        }
        break;
      }

      case LURKER_SINK:
        if (TICK_DOWN(st->coolDown) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= LURKER_SINK_STEP;
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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

      if ((actor->flags & ACTOR_HIT_BY_SPYRO) == 0) {
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
        A376[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        A376[1] = (g_anSineLut[i * 32] >> 8) * 3;
        A376[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)A376);
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
          A376[0] = (g_anCosineLut[bearing] * 3) >> 4;
          A376[1] = (g_anSineLut[bearing] * 3) >> 4;
          A376[2] = 0;
          A336[0] = (unsigned short)g_anCosineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          A336[1] = (unsigned short)g_anSineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          A336[2] = 0;
          bearing = (bearing + ANGLE8_HALF) & ANGLE8_MASK;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, A336, st->count - 1);
          LShiftVector3(A336, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          A408[0] = A376[0] * g_anCosineLut[arm * 2];
          A408[1] = A376[1] * g_anCosineLut[arm * 2];
          A408[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & ANGLE8_MASK;
          ScaleVector3Sat(A336, A336, st->index);
          SubtractVector(letterSlot, letterSlot, A336);
          swing = &g_anCosineLut[bearing];
          actor->posX = A376[0] * *swing;
          actor->posY = A376[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, A408);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, A376);
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

    /* Type 466: roaming ram. Wanders its waypoint ring away from Spyro,
     * charges down its path once he is close, and is knocked back by hits
     * or by its partner. */
    case ACTOR_TYPE_ROAMING_RAM: {
      RoamingRamState *st = actor->state;

      if ((actor->flags & ACTOR_HIT_ANY) && actor->mode != RAM_KNOCKED) {
        EBB_BREAK();
        if (actor->flags & ACTOR_HIT_BLAST) {
          st->knockHeading = ArcTan2(actor->posX - g_pActorListBase[st->target].posX,
                                    actor->posY - g_pActorListBase[st->target].posY,
                                    ATAN_ANGLE8);
          st->speed = RAM_BLAST_KNOCK_SPEED;
          st->zSpeed = RAM_BLAST_KNOCK_VZ;
        } else {
          st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
                                    spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
          st->speed = RAM_KNOCK_SPEED;
          st->zSpeed = RAM_KNOCK_VZ;
          if (actor->flags & ACTOR_HIT_CHARGE) {
            st->speed += RAM_CHARGE_KNOCK_BONUS;
            st->zSpeed += RAM_CHARGE_KNOCK_VZ_BONUS;
          }
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorLiveKill(actor);
        }
        if (actor->nextAnim != RAM_KNOCKED) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = RAM_KNOCKED;
          actor->nextFrame = 0;
          OnActorAnimQueued(actor);
        }
        actor->mode = RAM_KNOCKED;
        continue;
      }
      actor->flags = 0;
      if (st->triggerSlot != -1 && g_anLevelTriggerStateSlots[st->triggerSlot] != 0 &&
          (unsigned int)(actor->mode - RAM_KNOCKED) >= 2 &&
          IsPointInRotatedBox(&actor->posX, &st->box) != 0) {
        ENTER_POSE(actor, RAM_TRAPPED);
      }

      switch (actor->mode) {
      case RAM_GRAZE:
        if (TICK_DOWN(st->call) != 0) {
          st->call = (GetRandomU32() & 0x1F) + RAM_CALL_MIN;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == RAMKIND_PATH && ApproxDist2D(&actor->posX, &SPY.posX) < RAM_PATH_RANGE) {
          actor->mode = RAM_RUN_PATH;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < RAM_FLEE_RANGE) {
          int ang = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, ATAN_ANGLE8);
          int best = 0;
          int sel;
          int k;
          int d;

          for (k = 0; k < st->path->count; k++) {
            if (k == st->path->cur) {
              continue;
            }
            d = AbsAngleDelta8(ang,
                              ArcTan2(st->path->nodes[k].x - actor->posX,
                                            st->path->nodes[k].y - actor->posY,
                                            ATAN_ANGLE8));
            if (best < d) {
              best = d;
              sel = k;
            }
          }
          if (best > RAM_FLEE_MIN_TURN) {
            st->path->cur = sel;
            ENTER_POSE(actor, RAM_WANDER);
          }
        }
        break;

      case RAM_WANDER: {
        int ang =
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);

        if (TICK_DOWN(st->call) != 0) {
          st->call = (GetRandomU32() & 0x1F) + RAM_CALL_MIN;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (TurnActorYawTowardAngle(actor, ang, 6, 0x14, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongAngle(actor, ang, RAM_WANDER_SPEED, 0, RAM_MOVE_RADIUS, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < RAM_NODE_RANGE) {
            ENTER_POSE(actor, RAM_GRAZE);
          }
        }
        break;
      }

      case RAM_KNOCKED:
        DriveActorTrackedMove(actor, &st->speed, st->knockHeading, &st->zSpeed, RAM_KNOCK_DRAG, RAM_KNOCK_GRAVITY);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, RAM_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case RAM_TRAPPED:
        if (g_anLevelTriggerStateSlots[st->triggerSlot] == 0) {
          st->speed = 0;
          st->zSpeed = 0;
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(actor);
          ENTER_POSE(actor, RAM_KNOCKED);
        }
        if (actor->nextFrame >= RAM_TRAPPED_LOOP_FRAME) {
          actor->frame = 0;
          actor->nextFrame = 1;
          actor->frameProgress = 0;
        }
        break;

      case RAM_RUN_PATH:
        if (SPY.airborneFrames != 0) {
          break;
        }
        ANIM_GO(actor, RAM_WANDER);
        if (PivotActorAlongPath(actor, st->path, RAM_PIVOT_THRESH, RAM_RUN_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            PATH_ARRIVED(0)) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->mode = RAM_PATH_END;
          continue;
        }
        break;

      case RAM_PATH_END:
        if (TICK_DOWN(st->call) != 0) {
          st->call = (GetRandomU32() & 0x3F) + RAM_END_CALL_MIN;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        ANIM_GO(actor, RAM_GRAZE);
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 476: grabber. Creeps up on Spyro, seizes him, carries him off and
     * throws him; it drops him when hit. */
    case ACTOR_TYPE_GRABBER: {
      GrabberState *st = actor->state;
      int d0 = ApproxDist2D(&actor->posX, &spy->posX);

      EBB_BREAK();
      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != GRABBER_KNOCKED &&
          actor->mode != GRABBER_HUSK) {
        Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_GRABBER, actor);

        CopyVector(&o->posX, &actor->posX);
        o->mode = GRABBER_HUSK;
        if (o->anim != GRABBER_ANIM_HUSK) {
          ANIM_RESET(o, GRABBER_ANIM_HUSK);
        }
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        st->knockSpeed = GRABBER_KNOCK_SPEED;
        if (actor->mode >= GRABBER_SEIZE) {
          g_nSpyroDrawSuppressed = 0;
        }
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, ATAN_ANGLE8),
                                g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->knockZSpeed = GRABBER_KNOCK_VZ;
        if (actor->mode != GRABBER_LURK) {
          *st->held = 0;
        }
        PlayActorMeshVoice(actor, GRABBER_VOICE_HIT, 0);
        ANIM_SET(actor, GRABBER_ANIM_HIT);
        actor->mode = GRABBER_KNOCKED;
        continue;
      }
      actor->flags = 0;
      if (actor->nextAnim == GRABBER_ANIM_EMERGE &&
          GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[GRABBER_VOICE_EMERGE]) == VOICE_OWNER_NONE) {
        PlayActorMeshVoice(actor, GRABBER_VOICE_EMERGE, &st->voice);
      }

      switch (actor->mode) {
      case GRABBER_LURK:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (d0 > GRABBER_CALM_RANGE) {
          st->grabChance = 0;
          ANIM_GO(actor, GRABBER_ANIM_LURK);
        }
        if (TICK_DOWN(st->timer) != 0) {
          int k;
          int r;

          if (st->grabChance == 0) {
            if (actor->nextAnim == GRABBER_ANIM_LURK && actor->nextFrame >= GRABBER_LURK_LOOP_END) {
              actor->frame = 0;
              actor->nextFrame = 1;
              actor->frameProgress = 0;
            }
          } else if (actor->nextAnim == GRABBER_ANIM_LURK && actor->nextFrame >= GRABBER_LURK_EMERGE_FRAME) {
            g_nActorAnimDone = 0;
            ANIM_ADVANCE(actor, GRABBER_ANIM_EMERGE);
          }
          if (st->grabChance == 0 && d0 < GRABBER_NOTICE_RANGE) {
            st->grabChance = RandomIntInRange(0, 0x3F) + GRABBER_CHANCE_MIN;
          }
          k = st->grabChance - (d0 >> 6);
          r = GetRandomU32() & 0xFF;
          if (TICK_DOWN(st->grabCoolDown) != 0 && *st->held == 0 && r < k &&
              AbsAngleDelta8(CAMERA_YAW8,
                            ArcTan2(actor->posX - g_anCameraPos[0],
                                          actor->posY - g_anCameraPos[1], ATAN_ANGLE8)) <
                  GRABBER_CAM_CONE &&
              SPY_DZ(actor) >= -GRABBER_GRAB_BELOW &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < GRABBER_GRAB_DZ
                                 : -SPY_DZ(actor) < GRABBER_GRAB_DZ)) {
            *st->held = 1;
            st->timer = sixteen;
            StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
            actor->mode = GRABBER_RUSH;
            continue;
          }
          if (*st->held != 0) {
            st->grabCoolDown = GRABBER_GRAB_COOLDOWN;
          }
        }
        break;

      case GRABBER_RUSH: {
        int d1 = ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos);

        if (d1 < GRABBER_REACH_RANGE) {
          ANIM_GO(actor, GRABBER_ANIM_REACH);
        } else {
          ANIM_GO(actor, GRABBER_ANIM_MOVE);
        }
        if (TICK_DOWN(st->timer) != 0) {
          Actor **lp;
          Actor *o;

          if (MoveActorAlongAngle(actor,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8),
                            GRABBER_RUSH_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) != 0 ||
              spy->triggerEventId != 0) {
            actor->mode = GRABBER_RETREAT;
            continue;
          }
          lp = g_apActiveActorList;
          while ((o = *lp++) != 0) {
            if (o->type == ACTOR_TYPE_GUNNER &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x7D0 &&
                AbsAngleDelta8(actor->rotZ,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, ATAN_ANGLE8)) < 0x19) {
              o->flags |= ACTOR_HIT_SUPER;
            }
          }
          if (d1 < GRABBER_SEIZE_RANGE) {
            st->hasSpyro = 0;
            actor->mode = GRABBER_SEIZE;
            continue;
          }
        }
        break;
      }

      case GRABBER_RETREAT:
        ANIM_GO(actor, GRABBER_ANIM_MOVE);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->home[0] - actor->posX,
                                        st->home[1] - actor->posY, ATAN_ANGLE8),
                          6, 0x14, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, ATAN_ANGLE8),
                        GRABBER_RETREAT_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (ApproxDist2D(&actor->posX, st->home) < GRABBER_HOME_ARRIVE) {
          *st->held = 0;
          st->timer = GRABBER_HOME_REST;
          ENTER_POSE(actor, GRABBER_LURK);
        }
        break;

      case GRABBER_KNOCKED:
        actor->rotX += 4;
        actor->rotY += 4;
        if (DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, GRABBER_KNOCK_DRAG, GRABBER_KNOCK_GRAVITY) == MOVED_LANDED) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, GRABBER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case GRABBER_RECOVER:
        if (g_nActorAnimDone != 0) {
          actor->mode = GRABBER_RUSH;
          continue;
        }
        break;

      case GRABBER_SEIZE: {
        int dd = ApproxDist2D(&actor->posX, &spy->posX);
        int lim = GRABBER_SEIZE_SLOW_SPEED;
        int v;

        if (dd >= GRABBER_SEIZE_FAST_RANGE) {
          lim = GRABBER_SEIZE_FAST_SPEED;
        }
        if (dd < 0x190) {
          g_nSpyroDrawSuppressed = 1;
        }
        if (st->hasSpyro == 0) {
          if (dd < lim) {
            st->hasSpyro = 1;
            actor->posX = spy->posX;
            actor->posY = SPY.posY;
          } else {
            TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
            if (MoveActorAlongAngle(actor,
                              ArcTan2(spy->posX - actor->posX,
                                            SPY.posY - actor->posY, ATAN_ANGLE8),
                              lim, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) != 0 &&
                g_nSpyroDrawSuppressed == 0) {
              actor->mode = GRABBER_RETREAT;
              continue;
            }
          }
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (actor->nextFrame >= GRABBER_SEIZE_FRAME) {
          SPY.posX = actor->posX;
          g_nSpyroDrawSuppressed = 1;
          SPY.posY = actor->posY;
          v = actor->rotZ + ANGLE8_HALF;
          g_bSpyroPersistentEulerZ = v;
          g_nSpyroBodyYaw = (v & ANGLE8_MASK) << 4;
          if (g_nSparxHealth == SPARX_GONE) {
            spy->triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
            actor->mode = GRABBER_DEVOUR;
            continue;
          }
          if (g_bLevelUseShortReleaseAnim != 0) {
            actor->mode = GRABBER_THROW_SHORT;
            continue;
          }
          actor->mode++;
        }
        break;
      }

      case GRABBER_CARRY:
        ANIM_GO(actor, GRABBER_ANIM_CARRY);
        spy->posX = actor->posX;
        spy->posY = actor->posY;
        g_nSpyroDrawSuppressed = 1;
        spy->requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (g_nActorAnimDone != 0) {
          actor->mode++;
        }
        break;

      case GRABBER_THROW:
        ANIM_GO(actor, GRABBER_ANIM_THROW);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= GRABBER_THROW_RELEASE_FRAME) {
            g_nSpyroDrawSuppressed = 0;
            SPY.triggerEvents = (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * GRABBER_THROW_OFFSET) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * GRABBER_THROW_OFFSET) >> 10);
            SubtractVector(A408, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A408, VectorLength(A408, VEC_LEN_XY), GRABBER_THROW_SPEED);
            g_nSpyroRequestVelocityZ = GRABBER_THROW_VZ;
            g_anSpyroRequestVelocity.x = A408[0];
            g_nSpyroRequestVelocityY = A408[1];
          } else {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          }
        }
        if (g_nActorAnimDone != 0) {
          g_bLevelUseShortReleaseAnim = 1;
          actor->mode = GRABBER_RETREAT;
          continue;
        }
        break;

      case GRABBER_DEVOUR:
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ANIM_GO(actor, GRABBER_ANIM_DEVOUR);
        g_dwSpyroTriggerEventFlags |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
        break;

      case GRABBER_THROW_SHORT:
        ANIM_GO(actor, GRABBER_ANIM_THROW_SHORT);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= GRABBER_THROW_SHORT_RELEASE_FRAME) {
            g_nSpyroDrawSuppressed = 0;
            SPY.triggerEvents = (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * GRABBER_THROW_OFFSET) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * GRABBER_THROW_OFFSET) >> 10);
            SubtractVector(A376, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A376, VectorLength(A376, VEC_LEN_XY), GRABBER_THROW_SPEED);
            g_nSpyroRequestVelocityZ = GRABBER_THROW_VZ;
            g_anSpyroRequestVelocity.x = A376[0];
            g_nSpyroRequestVelocityY = A376[1];
          } else {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          }
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = GRABBER_RETREAT;
          continue;
        }
        break;

      case GRABBER_HUSK:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x200) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      }
      continue;
    }

    case ACTOR_TYPE_ONE_SHOT_PROP:
      if (actor->flags != 0 && actor->mode < ONESHOT_HIT) {
        actor->mode = ONESHOT_HIT;
        ANIM_RESET(actor, ONESHOT_HIT);
        continue;
      }
      switch (actor->mode) {
      case ONESHOT_IDLE:
        break;

      case ONESHOT_PLAY:
        ANIM_SET(actor, ONESHOT_PLAY);
        if (actor->nextFrame >= 0xE) {
          actor->mode = ONESHOT_PLAYED;
          ANIM_RESET(actor, ONESHOT_PLAYED);
          continue;
        }
        break;

      case ONESHOT_PLAYED:
        break;

      case ONESHOT_HIT:
        if (actor->nextFrame >= 0x13) {
          actor->mode = ONESHOT_HIT_DONE;
          ANIM_RESET(actor, ONESHOT_HIT_DONE);
          continue;
        }
        break;

      case ONESHOT_HIT_DONE:
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
}
  }
}
