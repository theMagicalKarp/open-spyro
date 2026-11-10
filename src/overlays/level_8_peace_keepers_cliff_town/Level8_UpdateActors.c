/* Level8_UpdateActors -- per-frame actor update for level 8 (Cliff
 * Town).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 9,
 * 15, 16, 25, 26 and 31); the level-8 classes are the bell prop (206/239/
 * 242), the catapult (218), the two-path runner (226), the lookout (229, a
 * variant of level 10's), the swooping guard (230), the wandering critter
 * (238) and the bouncing debris (288/289).
 *
 * Load-bearing source forms (matched 2026-09-30):
 *   - Level 15's loop top and frame scheme; fourteen shared scratch slots
 *     after sparxToTarget (launchVel, then y288..y464) in the original frame order.
 *   - Case 416 reads g_nFrameStep directly (level 0's form), so no `dt`
 *     carrier is kept live.
 *   - Case 288 reverses its sway through a temp (`sw`); re-reading the field
 *     in the `||` test costs a reload phantom slot and grows the frame by 8.
 *   - Case 230's default weave step is assigned after the heading call: a
 *     callee-saved-only insn is scheduled across the call to its slot.
 *   - Case 218 keeps both velocity factors in temps so fold does not move
 *     the constant onto the distance term.
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

void Level8_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  short oneq;
  char *base;
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
  int launchVel[3];
  int sparkVel[3];
  int y288[3];
  int floorProbe[3];
  int y320[3];
  int shardTwinkle[3];
  int barrelAxis[3];
  int muzzlePos[3];
  int smokeVel[3];
  int ballToNode[3];
  int y416[3];
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

    /* Egg thief (ThiefState). */
    case ACTOR_TYPE_EGG_THIEF: {
      ThiefState *st;

      EBB_BREAK();
      st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != THIEF_CAUGHT) {
        st->knockSpeed = THIEF_KNOCK_SPEED;
        if ((actor->flags & ACTOR_HIT_CHARGE) != 0) {
          st->knockSpeed = THIEF_CHARGE_KNOCK_SPEED;
        }
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        if (actor->dropKind == DROP_GEM_ROLL) {
          actor->dropKind = ACTOR_TYPE_SPARX_FOOD;
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorLiveKill(actor);
        }
        actor->mode = THIEF_CAUGHT;
        continue;
      }

      if (actor->dropKind != DROP_GEM_ROLL && st->egg == 0 && actor->mode != THIEF_CAUGHT) {
        st->egg = g_pfnLevelSpawnActor(ACTOR_TYPE_DRAGON_EGG, actor);
        st->egg->otBias = 0;
        ((EggRiseState *)st->egg->state)->owner = actor;
      }

      if (TICK_DOWN(st->voice) != 0 &&
          (actor->mode == THIEF_RUN || actor->mode == THIEF_CLIMB_RUN)) {
        int k;

        st->voice = (int)GetRandomU32() % 3 + 4;
        k = (int)GetRandomU32() % 5;
        while (k == st->lastVoice) {
          k = (int)GetRandomU32() % 5;
        }
        PlayActorMeshVoice(actor, k, 0);
        st->lastVoice = k;
      }

      if (actor->nextAnim == THIEF_ANIM_TAUNT) {
        if (actor->nextFrame == THIEF_TAUNT_EARLY_FRAME && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[THIEF_VOICE_TAUNT_EARLY]) == 0) {
          PlayActorMeshVoice(actor, THIEF_VOICE_TAUNT_EARLY, 0);
        }
        if (actor->nextFrame == THIEF_TAUNT_LATE_FRAME && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[THIEF_VOICE_TAUNT_LATE]) == 0) {
          PlayActorMeshVoice(actor, THIEF_VOICE_TAUNT_LATE, 0);
        }
      }

      switch (actor->mode) {
      case THIEF_IDLE:
        actor->otBias = 2;
        if (st->mode == THIEFKIND_HOP) {
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < THIEF_NOTICE_RANGE) {
            st->landed = 0;
            ANIM_GO(actor, THIEF_ANIM_RUN);
            actor->mode = THIEF_RUN;
            continue;
          }
        } else if (st->mode == THIEFKIND_GUARD) {
          int spd = THIEF_BASE_SPEED;

          spd -= ApproxDist2D(&actor->posX, &spy->posX) >> 5;
          if (spd < THIEF_GUARD_MIN_SPEED) {
            spd = THIEF_GUARD_MIN_SPEED;
          }
          if (spd > THIEF_GUARD_MAX_SPEED) {
            spd = THIEF_GUARD_MAX_SPEED;
          }
          if (actor->subMode == THIEFSUB_ORBIT) {
            int home = ArcTan2(st->path->nodes[0].x - actor->posX,
                                     st->path->nodes[0].y - actor->posY, ATAN_ANGLE8);

            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) < THIEF_GUARD_HOME_RANGE) {
              TurnActorYawTowardAngle(actor,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - SPY.posY, ATAN_ANGLE8),
                            8, 0, YAW_HOLD_IN_BAND);
              MoveActorAlongHeading(actor, spd, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            } else {
              int lim = 10;
              int r;
              int a = ArcTan2(
                  st->path->nodes[st->path->cur].x - spy->posX,
                  st->path->nodes[st->path->cur].y - SPY.posY, ATAN_ANGLE8);
              int b = ArcTan2(actor->posX - st->path->nodes[0].x,
                                    actor->posY - st->path->nodes[0].y, ATAN_ANGLE8);

              if (actor->nextAnim == THIEF_ANIM_TAUNT) {
                lim = 0x28;
              }
              if (AbsAngleDelta8(a, b) > lim &&
                  ApproxDist2D(&actor->posX, &spy->posX) < THIEF_GUARD_STEER_RANGE) {
                r = SteerActorAroundPoint(actor, &st->path->nodes[0].x, THIEF_GUARD_ORBIT_RADIUS, a, 5,
                                  spd, 0xE, 0x14, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, MOVE_FOLLOW_FLOOR);
              }
              if (r > 0x5A) {
                st->stuck++;
              } else {
                st->stuck = 0;
              }
              if (r < 0x1E) {
                TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
                ANIM_GO(actor, THIEF_ANIM_TAUNT);
              } else {
                ANIM_GO(actor, THIEF_ANIM_RUN);
              }
              if (st->stuck >= 0x1E &&
                  AbsAngleDelta8(home, ArcTan2(spy->posX - actor->posX,
                                                    spy->posY - actor->posY,
                                                    ATAN_ANGLE8)) > 0x20) {
                actor->subMode = THIEFSUB_BREAK;
              }
            }
          } else {
            int a = ArcTan2(st->path->nodes[0].x - spy->posX,
                                  st->path->nodes[0].y - SPY.posY, ATAN_ANGLE8);
            int b = ArcTan2(st->path->nodes[0].x - actor->posX,
                                  st->path->nodes[0].y - actor->posY, ATAN_ANGLE8);

            TurnActorYawTowardAngle(actor, a, 8, 0, YAW_HOLD_IN_BAND);
            MoveActorAlongHeading(actor, spd, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (AbsAngleDelta8(a, b) > 0x50) {
              actor->subMode = THIEFSUB_ORBIT;
            }
            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > THIEF_GUARD_ORBIT_RADIUS) {
              actor->subMode = THIEFSUB_ORBIT;
            }
          }
        } else if (st->mode == THIEFKIND_LEDGE) {
          ANIM_GO(actor, THIEF_ANIM_TAUNT);
          TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < THIEF_LEDGE_RESET_RANGE) {
            actor->mode = THIEF_LEDGE_RESET;
            continue;
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < THIEF_NOTICE_RANGE) {
            actor->mode = THIEF_LEDGE_RUN;
            continue;
          }
        } else if (st->mode == THIEFKIND_FLEE) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
              ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < THIEF_FLEE_NOTICE_DZ &&
              (st->needLos == 0 || CheckSpyroLineOfSightTo(&actor->posX) != 0)) {
            st->node = FindFarthestPathPointFromSpyro(st->path);
            st->speed = 0;
            if (st->node - st->path->dir != st->path->cur) {
              st->turned = 0;
              if (ApproxDist2D(&actor->posX,
                                &st->path->nodes[st->path->cur].x) < THIEF_NODE_REACHED_RANGE) {
                st->path->cur = (st->path->cur + st->path->dir +
                                 st->path->count) %
                                st->path->count;
              }
              actor->mode = THIEF_FLEE;
              continue;
            }
            ANIM_GO(actor, THIEF_ANIM_TAUNT);
            TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
            break;
          }
          ANIM_GO(actor, THIEF_ANIM_TAUNT);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        } else if (st->mode == THIEFKIND_CLIMB) {
          if (IsPointInRotatedBox(&SPY.posX, &st->area) != 0) {
            actor->mode = THIEF_CLIMB_RUN;
            continue;
          }
          ANIM_GO(actor, THIEF_ANIM_TAUNT);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        } else if (st->mode == THIEFKIND_CIRCLE) {
          if (st->dist == 0) {
            st->dist = ApproxDist2D(&actor->posX,
                                     &st->path->nodes[st->path->cur].x);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < THIEF_NEAR_RANGE) {
            int d = ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                  SPY.posY - st->path->nodes[st->path->cur].y, ATAN_ANGLE8);
            int b = ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                  actor->posY - st->path->nodes[st->path->cur].y,
                                  ATAN_ANGLE8) -
                    0x80;

            d = (d - b) & 0xFF;

            if (d > 0x80) {
              d -= 0x100;
            }
            if (ABS(d) > THIEF_CIRCLE_ENTER_ANGLE) {
              actor->mode = THIEF_CIRCLE;
              continue;
            }
          }
        } else if (st->mode == THIEFKIND_STAND) {
          actor->mode = THIEF_STAND;
          continue;
        }
        break;

      case THIEF_RUN: {
        int spd = THIEF_BASE_SPEED;

        spd -= ApproxDist2D(&actor->posX, &SPY.posX) >> 5;
        if (spd < THIEF_RUN_MIN_SPEED) {
          spd = THIEF_RUN_MIN_SPEED;
        }
        if (spd > THIEF_RUN_MAX_SPEED) {
          spd = THIEF_RUN_MAX_SPEED;
        }
        if (PivotActorAlongPath(actor, st->path, 0x73A, spd, 0xC8, 0xA, 0x80,
                          actor->group, 0) == PATH_ARRIVED(4)) {
          st->vz = THIEF_LEAP_VZ;
          actor->mode = THIEF_LEAP;
          continue;
        }
        break;
      }

      case THIEF_LEAP:
        actor->otBias = 0;
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        MoveActorAlongHeading(actor, THIEF_HOP_SPEED, 0, 0, MOVE_LIFT);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x) <
            THIEF_NODE_REACHED_RANGE) {
          actor->mode = THIEF_PERCH;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= THIEF_GRAVITY;
        break;

      case THIEF_PERCH: {
        int g = ProbeActorGroundHeight(actor);

        ANIM_GO(actor, THIEF_ANIM_TAUNT);
        if (st->landed == 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->landed = 1;
            break;
          }
          actor->posZ = z;
          st->vz -= THIEF_GRAVITY;
          if (st->vz < -THIEF_MAX_FALL) {
            st->vz = -THIEF_MAX_FALL;
          }
        } else {
          int k;
          int h;

          TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[4].x) <
              ApproxDist2D(&spy->posX, &st->path->nodes[5].x)) {
            k = 4;
          } else {
            k = 5;
          }
          h = ArcTan2(st->path->nodes[k].x - actor->posX,
                            st->path->nodes[k].y - actor->posY, ATAN_ANGLE8);
          if (ApproxDist2D(&actor->posX, &st->path->nodes[k].x) > THIEF_NODE_REACHED_RANGE) {
            if (TurnActorYawTowardAngle(actor, h, 8, 0x14, YAW_STEP_IN_BAND) != 0) {
              MoveActorAlongHeading(actor, THIEF_HOP_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            }
          } else if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < THIEF_PERCH_LEAVE_RANGE ||
                     ApproxDist2D(&spy->posX, &st->path->nodes[7].x) < THIEF_PERCH_LEAVE_RANGE) {
            st->vz = THIEF_HOP_VZ;
            actor->mode = THIEF_LEAP_HOME;
            continue;
          }
        }
        break;
      }

      case THIEF_LEAP_HOME: {
        int g;

        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[0].x - actor->posX,
                                    st->path->nodes[0].y - actor->posY, ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > THIEF_NODE_REACHED_RANGE) {
          MoveActorAlongHeading(actor, THIEF_LEAP_HOME_SPEED, 0, 0, MOVE_LIFT);
        }
        actor->posZ += st->vz;
        st->vz -= THIEF_GRAVITY;
        g = ProbeActorGroundHeight(actor);
        if (st->vz < 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->path->cur = 0;
            ANIM_GO(actor, THIEF_ANIM_IDLE);
            actor->mode = THIEF_IDLE;
            continue;
          }
          actor->posZ = z;
          st->vz -= THIEF_GRAVITY;
          if (st->vz < -THIEF_MAX_FALL) {
            st->vz = -THIEF_MAX_FALL;
          }
        }
        break;
      }

      case THIEF_CAUGHT:
        if (st->egg != 0) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          if (actor->nextAnim == THIEF_ANIM_CAUGHT && actor->nextFrame >= THIEF_EGG_RELEASE_FRAME) {
            st->egg->mode = EGG_RELEASED;
            st->egg->otBias = 4;
            st->egg = 0;
          }
        }
        if (st->knockSpeed != 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          st->knockSpeed -= THIEF_KNOCK_DRAG;
          if (st->knockSpeed < 0) {
            st->knockSpeed = 0;
          }
        }
        ANIM_GO(actor, THIEF_ANIM_CAUGHT);
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        BurstActorWithSound(actor, 0x18);
        DespawnActorRecord(actor);
        continue;

      case THIEF_LEDGE_RUN:
        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (PivotActorAlongPath(actor, st->path, 0x100, THIEF_LEDGE_RUN_SPEED, 0, 8, sixteen, PIVOT_TURN_ANY,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == PATH_ARRIVED(3)) {
          actor->mode = THIEF_LEDGE_WAIT;
          continue;
        }
        break;

      case THIEF_LEDGE_WAIT:
        if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < THIEF_LEDGE_RESET_RANGE) {
          actor->mode = THIEF_LEDGE_RESET;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < THIEF_NEAR_RANGE) {
          actor->mode = THIEF_LEDGE_RUN_ON;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        ANIM_GO(actor, THIEF_ANIM_TAUNT);
        break;

      case THIEF_LEDGE_RUN_ON:
        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (PivotActorAlongPath(actor, st->path, 0x100, THIEF_LEDGE_RUN_ON_SPEED, 0, 0xA, 0x30, PIVOT_TURN_ANY,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == PATH_ARRIVED(1)) {
          actor->mode = THIEF_LEDGE_TAUNT;
          continue;
        }
        break;

      case THIEF_LEDGE_TAUNT: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        ANIM_GO(actor, THIEF_ANIM_TAUNT);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (d < THIEF_NEAR_RANGE && SPY.posZ - actor->posZ >= -0xC7) {
          st->vz = THIEF_HOP_VZ;
          st->landed = 0;
          actor->mode = THIEF_LEDGE_JUMP;
          continue;
        }
        break;
      }

      case THIEF_LEDGE_JUMP: {
        int sp = 1;

        if (st->landed != 0) {
          sp = 5;
        }
        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[2].x - actor->posX,
                                        st->path->nodes[2].y - actor->posY, ATAN_ANGLE8),
                          8, 0x1E, YAW_STEP_IN_BAND) == 0) {
          break;
        }
        MoveActorAlongHeading(actor, THIEF_LEDGE_JUMP_SPEED, 0, 0, sp);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[2].x) < THIEF_NODE_REACHED_RANGE) {
          st->path->cur = 3;
          actor->mode = THIEF_LEDGE_WAIT;
          continue;
        }
        if (st->landed == 0) {
          int g = ProbeActorGroundHeightRaised(actor, 0x1800);

          actor->posZ += st->vz;
          st->vz -= THIEF_GRAVITY;
          if (st->vz < 0 && actor->posZ - g < ABS(st->vz)) {
            st->landed = 1;
          }
        }
        break;
      }

      case THIEF_LEDGE_RESET:
        CopyVector(&actor->posX, &st->path->nodes[0].x);
        st->path->cur = 1;
        SnapActorToGround(actor);
        actor->mode = THIEF_LEDGE_TAUNT;
        continue;

      case THIEF_FLEE: {
        int top = THIEF_FLEE_SPEED;
        int next;
        int dCur;
        int dNext;
        int r;

        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (st->path->nodes[st->path->cur].speed > 0) {
          top = st->path->nodes[st->path->cur].speed;
        }
        if (st->speed < top) {
          st->speed += THIEF_FLEE_ACCEL;
          if (top < st->speed) {
            st->speed = top;
          }
        } else if (top < st->speed) {
          st->speed -= THIEF_FLEE_ACCEL;
          if (st->speed < top) {
            st->speed = top;
          }
        }
        next = (st->path->cur + st->path->dir + st->path->count) %
               st->path->count;
        dCur = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);
        dNext = ApproxDist2D(&actor->posX, &st->path->nodes[next].x);
        if (dCur > THIEF_FLEE_TURN_CLEARANCE && dNext > THIEF_FLEE_TURN_CLEARANCE) {
          int aCur = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                   st->path->nodes[st->path->cur].y - actor->posY,
                                   ATAN_ANGLE8);
          int aNext = ArcTan2(st->path->nodes[next].x - actor->posX,
                                    st->path->nodes[next].y - actor->posY, ATAN_ANGLE8);
          int aSpy =
              ArcTan2(spy->posX - actor->posX, spy->posY - actor->posY, ATAN_ANGLE8);
          int dA = AbsAngleDelta8(aCur, aSpy);
          int dB = AbsAngleDelta8(aNext, aSpy);

          if (st->turned == 0 && ABS2(dA - dB) < 0x28) {
            st->turned = 1;
            st->path->dir = -st->path->dir;
          } else if (dA < dB && ABS2(dA - dB) >= 0x1D && dB > 0x3C &&
                     AbsAngleDelta8(actor->rotZ,
                                   ArcTan2(spy->posX - actor->posX,
                                                 spy->posY - actor->posY,
                                                 ATAN_ANGLE8)) < 0x3C) {
            st->path->dir = -st->path->dir;
            st->path->cur = next;
          }
        }
        r = PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x1E, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        if (r & PATH_ARRIVED_FLAG) {
          if ((r & ~PATH_ARRIVED_FLAG) ==
              (st->node - st->path->dir) % st->path->count) {
            actor->mode = THIEF_IDLE;
            continue;
          }
          st->node = FindFarthestPathPointFromSpyro(st->path);
        }
        break;
      }

      case THIEF_CLIMB_RUN:
        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (actor->posZ < THIEF_FELL_BELOW_Z) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        st->speed = THIEF_CLIMB_SPEED;
        if (st->path->nodes[st->path->cur].speed > 0) {
          st->speed = st->path->nodes[st->path->cur].speed;
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, PIVOT_TURN_ANY,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == PATH_ARRIVED(0xB)) {
          st->vz = THIEF_HOP_VZ;
          actor->mode = THIEF_CLIMB_HOP;
          continue;
        }
        break;

      case THIEF_CLIMB_HOP: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        if (actor->posZ < THIEF_FELL_BELOW_Z) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        if (d > THIEF_NODE_REACHED_RANGE) {
          MoveActorAlongHeading(actor, THIEF_HOP_SPEED, 0, 0, MOVE_LIFT);
        }
        if (d < THIEF_NODE_REACHED_RANGE && actor->posZ == g) {
          if (++st->path->cur == THIEF_CLIMB_TOP_NODE) {
            actor->mode = THIEF_CLIMB_TOP;
            continue;
          }
          st->vz = THIEF_HOP_VZ;
          break;
        }
        actor->posZ += st->vz;
        st->vz -= THIEF_GRAVITY;
        if (st->vz < -THIEF_MAX_FALL) {
          st->vz = -THIEF_MAX_FALL;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case THIEF_CLIMB_TOP:
        ANIM_GO(actor, THIEF_ANIM_TAUNT);
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (actor->posZ < THIEF_FELL_BELOW_Z) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        if (SPY.posZ - st->path->nodes[10].z < -THIEF_CLIMB_DOWN_DZ &&
            ApproxDist2D(&actor->posX, (int *)(base - 8)) < THIEF_CLIMB_DOWN_RANGE) {
          st->vz = THIEF_CLIMB_DOWN_VZ;
          st->path->cur = 9;
          actor->mode = THIEF_CLIMB_HOP_DOWN;
          continue;
        }
        break;

      case THIEF_CLIMB_HOP_DOWN: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        ANIM_GO(actor, THIEF_ANIM_RUN);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        if (d > THIEF_NODE_REACHED_RANGE) {
          MoveActorAlongHeading(actor, THIEF_HOP_SPEED, 0, 0, MOVE_LIFT);
        }
        if (d < THIEF_NODE_REACHED_RANGE && actor->posZ == g) {
          st->path->dir = -st->path->dir;
          st->path->cur = 8;
          actor->mode = THIEF_CLIMB_RUN_BACK;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= THIEF_GRAVITY;
        if (st->vz < -THIEF_MAX_FALL) {
          st->vz = -THIEF_MAX_FALL;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case THIEF_CLIMB_RUN_BACK:
        ANIM_GO(actor, THIEF_ANIM_RUN);
        st->speed = THIEF_CLIMB_BACK_SPEED;
        if (actor->posZ < THIEF_FELL_BELOW_Z) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, PIVOT_TURN_ANY,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == st->path->count + 0xFF) {
          st->path->dir = -st->path->dir;
          st->path->cur = 1;
          actor->mode = THIEF_IDLE;
          continue;
        }
        break;

      case THIEF_CIRCLE:
        ANIM_GO(actor, THIEF_ANIM_RUN);
        if (ApproxDist2D(&actor->posX, &spy->posX) < THIEF_NEAR_RANGE) {
          int a = (ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                 SPY.posY - st->path->nodes[st->path->cur].y,
                                 ATAN_ANGLE8) +
                   0x80) &
                  0xFF;

          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist, a, 8,
                        st->speed, 0xF, 0x40, STEER_NO_BLOCK, STEER_NO_BLOCK, 0, 0, 0);
          a = (a - ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                 actor->posY - st->path->nodes[st->path->cur].y,
                                 ATAN_ANGLE8)) &
              0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS(a) < 0x20) {
            ENTER_POSE(actor, THIEF_IDLE);
          }
          break;
        }
        ENTER_POSE(actor, THIEF_IDLE);

      case THIEF_STAND:
        ANIM_GO(actor, THIEF_ANIM_TAUNT);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        break;
      }

      if (st->egg != 0) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, &st->egg->posX);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
        if (ApproxDist2D(&anchor->posX, &SPY.posX) > SAVEHOST_REARM_RANGE) {
          actor->mode = SAVEHOST_WAIT_ARMED;
        }
        break;

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

    /* Bell prop: a flame rings it (spawning a ripple), any other hit wobbles it (BellAnim). */
    case ACTOR_TYPE_BELL_A:
    case ACTOR_TYPE_BELL_B:
    case ACTOR_TYPE_BELL_C:
      if (actor->flags != 0) {
        if (actor->flags == ACTOR_HIT_FLAME && actor->subMode == 0) {
          actor->subMode = 0x50;
          ANIM_SET(actor, BELL_ANIM_SWING);
        } else if (actor->nextAnim == BELL_ANIM_REST) {
          ANIM_SET(actor, BELL_ANIM_WOBBLE);
        }
      }
      actor->flags = 0;
      if (g_nActorAnimDone != 0) {
        switch (actor->nextAnim) {
        case BELL_ANIM_SWING: {
          Actor *p = g_pfnLevelSpawnActor(ACTOR_TYPE_BELL_RIPPLE, actor);

          p->rotZ = ((unsigned short)g_nCameraEulerYaw << 16 >> 18) - 0x80;
          CopyVector(&p->posX, &actor->posX);
          ANIM_SET(actor, BELL_ANIM_RING);
          break;
        }
        case BELL_ANIM_WOBBLE:
        case BELL_ANIM_SETTLE:
          ANIM_SET(actor, BELL_ANIM_REST);
          break;
        }
      }
      if (TICK_DOWN(actor->subMode) == 2) {
        ANIM_SET(actor, BELL_ANIM_SETTLE);
      }
      break;

    /* Catapult (CatapultState). */
    case ACTOR_TYPE_CATAPULT: {
      CatapultState *st = actor->state;

      if (actor->flags & ACTOR_HIT_FLAME) {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int a = AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                              ArcTan2(actor->posX - spy->posX,
                                            actor->posY - SPY.posY, ATAN_ANGLE8));

        actor->flags = 0;
        if (st->angle == 0) {
          int m = (0x1770 - d) >> 6;
          int k = (0x28 - a) * 0x46;

          st->timer = 0x5A;
          st->vel = (m * k) >> 12;
        }
        if (st->vel > 0x50) {
          st->vel = 0x50;
        }
      }
      st->angle += st->vel;
      if (st->angle > 0x120) {
        st->angle = 0x120;
      }
      if (st->angle < 0) {
        st->angle = 0;
      }
      st->vel -= 4;
      if (st->vel < -0x20) {
        st->vel = -0x20;
      }
      actor->frame = st->angle >> 5;
      actor->nextFrame = st->angle >> 5;
      if (st->link >= 0) {
        Actor *t = &g_pActorListBase[st->link];

        if (t->mode >= ACTOR_MODE_DORMANT) {
          st->link = -1;
          break;
        }
        if (st->angle != 0) {
          CollectibleState *fs;
          int n;
          int step;
          int z;

          PlayActorMeshVoice(actor, CATAPULT_VOICE_SWING, 0);
          fs = t->state;
          CopyVector(launchVel, &t->posX);
          CopyVector(&t->posX, &actor->posX);
          n = 0;
          step = 0xB4;
          z = t->posZ;
          do {
            z += step;
            step -= 10;
            n++;
          } while (step > 0 || launchVel[2] < z);
          SubtractVector(launchVel, launchVel, &t->posX);
          DivideVector3(launchVel, launchVel, n);
          launchVel[2] = 0xB4;
          CopyVector(fs->vel, launchVel);
          fs->mode = COLLREST_STILL;
          fs->timer = 0;
          fs->bounce = 3;
          t->subMode = COLLECT_BOUNCE;
          t->cullRadius = 0x18;
          t->updateDist = 0x40;
          st->flung = t;
          st->link = -1;
          break;
        }
        t->cullRadius = ACTOR_CULL_HIDDEN;
        t->drawn = 0;
        t->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
        t->subMode = COLLECT_INERT;
        break;
      }
      if (st->flung != 0) {
        if (st->flung->mode >= ACTOR_MODE_DORMANT || st->flung->subMode != COLLECT_BOUNCE) {
          st->flung = 0;
          break;
        }
        if (g_dwGamestateFrames & 1) {
          ZeroVector(sparkVel);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->flung->posX, (int)sparkVel);
        }
      }
      break;
    }

    /* Two-path patroller (RunnerState). */
    case ACTOR_TYPE_TWO_PATH_RUNNER: {
      RunnerState *st = actor->state;
      int r;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode != TPRUNNER_KNOCKED) {
        st->knockSpeed = TPRUNNER_KNOCK_SPEED;
        st->knockZSpeed = TPRUNNER_KNOCK_VZ;
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        actor->flags = 0;
        actor->probeDist = TPRUNNER_PROBE_DIST;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, TPRUNNER_KNOCKED);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case TPRUNNER_IDLE:
        if (st->mode == TPARM_GUARD || st->mode == TPARM_RUN) {
          ENTER_POSE(actor, TPRUNNER_GUARD);
        }
        break;

      case TPRUNNER_GUARD: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        if (st->mode == TPARM_RUN) {
          if (ApproxDist2D(&spy->posX, &st->pathA->nodes[st->node].x) <
              st->range << 10) {
            st->which = 1;
            actor->mode = TPRUNNER_RUN;
            continue;
          }
          if (ApproxDist2D(&spy->posX, &st->pathB->nodes[st->node].x) <
              st->range << 10) {
            st->which = 2;
            actor->mode = TPRUNNER_RUN;
            continue;
          }
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && d < TPRUNNER_SWIPE_RANGE &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < TPRUNNER_SWIPE_DZ) {
          st->timer = TPRUNNER_SWIPE_GAP;
          ENTER_POSE(actor, TPRUNNER_SWIPE);
        }
        break;
      }

      case TPRUNNER_KNOCKED:
        r = DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed,
                          TPRUNNER_KNOCK_DRAG, TPRUNNER_KNOCK_GRAVITY);
        if (r == MOVED_LANDED && st->cried == 0) {
          st->cried = 1;
          PlayActorMeshVoice(actor, TPRUNNER_VOICE_LAND, 0);
        }
        if (r == MOVED_BLOCKED && st->knockZSpeed > -TPRUNNER_WALL_DROP) {
          st->knockZSpeed = -TPRUNNER_WALL_DROP;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, TPRUNNER_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case TPRUNNER_REACT:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, TPRUNNER_GUARD);
        }
        break;

      case TPRUNNER_SWIPE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, TPRUNNER_GUARD);
        }
        break;

      case TPRUNNER_WALK_BACK:
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->pathA->nodes[st->pathA->cur].x -
                                        actor->posX,
                                    st->pathA->nodes[st->pathA->cur].y -
                                        actor->posY,
                                    ATAN_ANGLE8),
                      4, 0, YAW_HOLD_IN_BAND);
        MoveActorAlongHeading(actor, TPRUNNER_WALK_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        if (ApproxDist2D(&actor->posX,
                          &st->pathA->nodes[st->pathA->cur].x) < TPRUNNER_PIVOT_THRESH) {
          ENTER_POSE(actor, TPRUNNER_GUARD);
        }
        break;

      case TPRUNNER_RUN:
        ANIM_GO(actor, TPRUNNER_ANIM_RUN);
        if (st->which == 1) {
          if (PivotActorAlongPath(actor, st->pathA, TPRUNNER_PIVOT_THRESH,
                            actor->drawn == 0 ? TPRUNNER_OFFSCREEN_SPEED : TPRUNNER_RUN_SPEED, 0, 4, 0x14, PIVOT_TURN_ANY,
                            MOVE_LIFT | MOVE_FOLLOW_FLOOR) == 0) {
            break;
          }
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= TPRUNNER_CATCH_RANGE &&
              st->pathA->cur != st->node) {
            break;
          }
          st->mode = TPARM_GUARD;
          ENTER_POSE(actor, TPRUNNER_GUARD);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= TPRUNNER_CATCH_RANGE &&
            PivotActorAlongPath(actor, st->pathB, TPRUNNER_PIVOT_THRESH, TPRUNNER_B_SPEED, 0, 4, 0x14, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
                0) {
          break;
        }
        if (st->pathB->cur != st->node) {
          break;
        }
        st->mode = TPARM_GUARD;
        ENTER_POSE(actor, TPRUNNER_GUARD);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Lookout (LookoutState): watches over the guard it is linked to, rouses
     * it when Spyro comes into range, and swipes at him up close. */
    case ACTOR_TYPE_GUARD_LOOKOUT: {
      LookoutState *st = actor->state;
      Actor *link;
      int d;
      int h;
      int mode;

      link = 0;
      if (st->link != -1) {
        link = &g_pActorListBase[st->link];
      }
      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != GLOOK_KNOCKED) {
        st->knockSpeed = GLOOK_KNOCK_SPEED;
        st->knockHeading = SPY.bodyRotZ;
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, GLOOK_KNOCKED);
      }
      if (TICK_DOWN(st->camTimer) == 0) {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FREEZE_INPUT);
      }
      if (!(g_dwActiveCameraOptions & CAM_OPT_PASSIVE_FOLLOW) && TICK_DOWN(st->camTimer2) == 0) {
        int cd = ApproxDist2D(&actor->posX, &spy->posX) + GLOOK_CAM_BACK;

        if (st->camDist < cd) {
          st->camDist = cd;
        }
        if (st->camTimer != 0) {
          g_dwSpyroRequestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        } else {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        }
        SPY.pCamAnchor = &actor->posX;
        SPY.pCamTarget = &g_anCameraActiveTargetParams;
        g_anCameraActiveTargetParams.w[0] = -ArcTan2(SPY.posX - actor->posX,
                                         SPY.posY - actor->posY, ATAN_ANGLE8)
                          << 4;
        g_nCameraActiveTargetPitch = 0;
        g_nCameraActiveTargetRoll = 0;
        g_nCameraActiveTargetPitchBias = 0;
        g_nCameraActiveTargetYawBias = 0;
        g_nCameraActiveTargetDistance = st->camDist;
      }

      mode = actor->mode;
      switch (mode) {
      case GLOOK_WATCH:
        d = ApproxDist2D(&actor->posX, &spy->posX);
        if (st->init == 0) {
          st->init = 1;
          st->homeHeading = actor->rotZ;
        }
        if (link != 0 && actor->subMode == 0 && link->mode == TPRUNNER_IDLE &&
            d < st->range << 10 && actor->posZ < SPY.posZ &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < GLOOK_ROUSE_DZ) {
          actor->subMode = 1;
          ANIM_GO(actor, GLOOK_ANIM_STAND);
          actor->mode = GLOOK_ROUSE;
          continue;
        }
        if (d > GLOOK_IDLE_RANGE) {
          TurnActorYawTowardAngle(actor, st->homeHeading, 4, 0, YAW_HOLD_IN_BAND);
          if (actor->anim == GLOOK_ANIM_IDLE && g_nActorAnimNewFrame != 0 && actor->frame == GLOOK_IDLE_LOOP_END &&
              --st->count > 0) {
            actor->frame = GLOOK_IDLE_LOOP_START;
            actor->nextFrame = GLOOK_IDLE_LOOP_START + 1;
            actor->frameProgress = 0;
          }
          if (g_nActorAnimDone != 0) {
            if (st->count == 0) {
              st->count = RandomIntInRange(3, 5);
              if (RandomIntInRange(0, 0x64) > 0x50) {
                ANIM_GO(actor, GLOOK_ANIM_FIDGET);
              } else {
                ANIM_GO(actor, GLOOK_ANIM_STAND);
              }
            } else {
              ANIM_GO(actor, GLOOK_ANIM_IDLE);
            }
          }
        } else {
          h = WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8),
                            5);
          if (actor->nextAnim == GLOOK_ANIM_STAND) {
            ANIM_GO(actor, GLOOK_WATCH);
          }
          if (actor->nextAnim == GLOOK_ANIM_IDLE && actor->nextFrame < GLOOK_IDLE_LOOP_END) {
            actor->frame = GLOOK_IDLE_LOOP_END;
            actor->nextFrame = GLOOK_IDLE_LOOP_END + 1;
            actor->frameProgress = 0;
          }
          if (actor->anim < 2 || actor->anim == GLOOK_ANIM_WATCH_ALT) {
            TurnActorYawTowardAngle(actor, h, 4, 0, YAW_HOLD_IN_BAND);
          }
          if (g_nActorAnimDone != 0) {
            if (actor->nextAnim == GLOOK_ANIM_IDLE) {
              ANIM_SET(actor, GLOOK_WATCH);
            } else if (--st->count <= 0) {
              st->count = RandomIntInRange(3, 5);
              if (RandomIntInRange(0, 0x64) > 0x50) {
                if (actor->nextAnim != GLOOK_ANIM_FIDGET) {
                  ANIM_GO(actor, GLOOK_ANIM_FIDGET);
                } else {
                  ANIM_GO(actor, GLOOK_WATCH);
                }
              }
            } else {
              ANIM_GO(actor, GLOOK_ANIM_WATCH_ALT);
            }
          }
        }
        if (d < GLOOK_GUARD_SWIPE_RANGE && ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < GLOOK_SWIPE_DZ &&
            actor->posZ - actor->floorOffset - SPY.posZ < 0) {
          ENTER_POSE(actor, GLOOK_SWIPE);
        }
        break;

      case GLOOK_SWIPE:
        TurnActorYawTowardAngle(actor,
                      WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                  spy->posY - actor->posY, ATAN_ANGLE8),
                                    5),
                      6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimNewFrame != 0 && actor->frame == GLOOK_SWIPE_FRAME &&
            ApproxDist2D(&actor->posX, &spy->posX) < GLOOK_SWIPE_HIT_RANGE) {
          SubtractVector(y288, &spy->posX, &actor->posX);
          ScaleVector3ByRatio(y288, VectorLength(y288, VEC_LEN_XY), GLOOK_GUARD_SWIPE_PUSH);
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY);
          g_anSpyroRequestVelocity.x = y288[0];
          g_nSpyroRequestVelocityZ = GLOOK_GUARD_SWIPE_PUSH_VZ;
          g_nSpyroRequestState = mode;
          g_nSpyroRequestVelocityY = y288[1];
          st->camTimer = GLOOK_CAM_TICKS;
          st->camTimer2 = GLOOK_CAM_TICKS;
          st->camDist = GLOOK_CAM_DIST;
          if (g_nHitRumbleTimer < HIT_RUMBLE_FRAMES) {
            g_nHitRumbleTimer = HIT_RUMBLE_FRAMES;
          }
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, GLOOK_WATCH);
        }
        break;

      case GLOOK_KNOCKED:
        if (st->knockSpeed > 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, GLOOK_KNOCK_CONTACT, GLOOK_MOVE_RADIUS, MOVE_LIFT);
          st->knockSpeed -= GLOOK_KNOCK_DRAG;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, GLOOK_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case GLOOK_ROUSE:
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(link->posX - actor->posX,
                                        link->posY - actor->posY, ATAN_ANGLE8),
                          8, 6, YAW_STEP_IN_BAND) != 0) {
          ANIM_GO(actor, GLOOK_ANIM_CALL);
        }
        if (actor->anim == GLOOK_ANIM_CALL && g_nActorAnimNewFrame != 0 && actor->frame == GLOOK_CALL_FRAME) {
          if (link->nextAnim != TPRUNNER_ANIM_RUN) {
            ANIM_ADVANCE(link, TPRUNNER_ANIM_RUN);
          }
          link->mode = TPRUNNER_WALK_BACK;
        }
        if (g_nActorAnimDone != 0 && actor->anim == GLOOK_ANIM_CALL) {
          ANIM_GO(actor, GLOOK_ANIM_STAND);
          actor->mode = GLOOK_WATCH;
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Swooping guard (SwoopState). */
    case ACTOR_TYPE_SWOOPER: {
      SwoopState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != SWOOP_KNOCKED) {
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->speed = SWOOP_KNOCK_SPEED;
        if (actor->mode != SWOOP_PERCH) {
          *st->alert = 0;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        if (actor->flags & ACTOR_HITS_SPYRO) {
          int i;

          for (i = 0; i < 7; i++) {
            Actor *p;

            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_BOUNCE_DEBRIS, actor);
            p = g_pfnLevelSpawnActor(ACTOR_TYPE_BOUNCE_DEBRIS + 1, actor);
            if (GetRandomU32() & 1) {
              p->mode = 1;
              ANIM_RESET(p, 1);
            }
          }
        }
        actor->mode = SWOOP_KNOCKED;
        ANIM_RESET(actor, SWOOP_ANIM_KNOCKED);
        continue;
      }

      switch (actor->mode) {
      case SWOOP_PERCH: {
        int d = ApproxDist2D(&actor->posX, &SPY.posX);
        int dz = st->home[2] - actor->posZ;

        if (dz != 0) {
          if (dz > -SWOOP_RETURN_CLIMB_STEP) {
            actor->posZ = st->home[2];
          } else {
            actor->posZ -= SWOOP_RETURN_CLIMB_STEP;
          }
        }
        if (st->heading == -1) {
          st->heading = actor->rotZ;
        }
        TurnActorYawTowardAngle(actor, st->heading, 4, 0, YAW_HOLD_IN_BAND);
        if (*st->alert == 0 && d < st->range << 10 && d >= SWOOP_MIN_RANGE &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < SWOOP_SIGHT_DZ) {
          CopyVector(y288, &actor->posX);
          y288[0] += g_anCosineLut[actor->rotZ] >> 2;
          y288[1] += g_anSineLut[actor->rotZ] >> 2;
          if (actor->drawn != 0 &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, ATAN_ANGLE8)) < SWOOP_SPY_FACING_CONE &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8)) < SWOOP_FACING_CONE &&
              CheckSpyroLineOfSightTo(y288) != 0) {
            if (TICK_DOWN(st->timer) == 0) {
              break;
            }
            st->swayDir = 1;
            st->sway = 0;
            st->speed = 0;
            st->weaving = 0;
            if (st->kind == SWOOP_KIND_RANDOM) {
              if (((int)GetRandomU32() & 0xFF) >= 0x80) {
                st->side = 1;
              } else {
                st->side = 0;
              }
            } else {
              st->side = st->kind;
            }
            *st->alert = 1;
            st->count = 0;
            if (actor->anim != SWOOP_ANIM_LAUNCH) {
              actor->frameProgress = 8;
              actor->frameRate = 8;
              actor->anim = actor->nextAnim;
              actor->nextAnim = SWOOP_ANIM_LAUNCH;
              actor->frame = actor->nextFrame;
              actor->nextFrame = 0;
              OnActorAnimQueued(actor);
            }
            actor->mode = SWOOP_DIVE;
            continue;
          }
          st->timer = SWOOP_SIGHT_FRAMES;
        }
        break;
      }

      case SWOOP_DIVE: {
        int d;
        int step;
        int a;
        int target;
        int dz;
        int g1;
        int g2;

        d = ApproxDist2D(&actor->posX, &spy->posX);
        a = ArcTan2(spy->posX - actor->posX, SPY.posY - actor->posY, ATAN_ANGLE8);
        step = SWOOP_SWAY_STEP;

        if (g_nActorAnimDone == 0 && actor->nextAnim == SWOOP_ANIM_LAUNCH && actor->nextFrame < 0xB) {
          break;
        }
        st->count += g_nFrameStep;
        actor->posZ += SWOOP_DIVE_PROBE_RAISE;
        target = FindGroundHeightBelow(&actor->posX, SWOOP_DIVE_PROBE_DEPTH) + SWOOP_DIVE_CLEARANCE;
        actor->posZ -= SWOOP_DIVE_PROBE_RAISE;
        if (st->kind == SWOOP_KIND_STRAIGHT) {
          target = SPY.posZ + SWOOP_STRAIGHT_CLEARANCE;
        }
        if (d < SWOOP_SWAY_NEAR_RANGE) {
          step = SWOOP_SWAY_STEP_NEAR;
        } else if (d < SWOOP_SWAY_MID_RANGE) {
          step = SWOOP_SWAY_STEP_MID;
        }
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, SWOOP_ANIM_FLY);
        }
        if (st->speed < SWOOP_DIVE_SPEED) {
          st->speed += SWOOP_DIVE_ACCEL;
        }
        if (d < SWOOP_WEAVE_RANGE && st->side != SWOOP_KIND_STRAIGHT) {
          st->weaving = 1;
        }
        if (st->weaving != 0) {
          if (st->swayDir == 1) {
            st->sway += step;
            if (st->sway >> 4 >= SWOOP_SWAY_MAX) {
              st->swayDir = -st->swayDir;
            }
          } else if (st->swayDir == -1) {
            st->sway -= step;
            if (st->sway >> 4 <= 0) {
              st->swayDir = 0;
            }
          }
        }
        dz = target - actor->posZ;
        if (ABS(dz) >= SWOOP_CLIMB_SNAP) {
          if (dz < 0) {
            actor->posZ -= SWOOP_CLIMB_STEP;
          } else {
            actor->posZ += SWOOP_CLIMB_STEP;
          }
        } else {
          actor->posZ = target;
        }
        if (d < SWOOP_PASS_RANGE &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, ATAN_ANGLE8)) >= SWOOP_PASS_ARC) {
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, ATAN_ANGLE8)) >= SWOOP_PASS_LOST_ARC &&
            actor->drawn == 0) {
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        if (st->side != 0) {
          actor->rotZ = WrapAddAngle8(a, st->sway >> 4);
        } else {
          actor->rotZ = WrapAddAngle8(a, -(st->sway >> 4));
        }
        if (MoveActorAlongHeading(actor, st->speed, 0, SWOOP_MOVE_RADIUS, MOVE_LIFT) != 0) {
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        if (st->count >= SWOOP_SIGHT_GRACE && d < SWOOP_SIGHT_RANGE &&
            CheckSpyroLineOfSightTo(&actor->posX) == 0) {
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        if (d < SWOOP_HIT_RANGE &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, ATAN_ANGLE8)) < SWOOP_HIT_CONE &&
            ABS2(actor->posZ - actor->floorOffset - spy->posZ) < SWOOP_HIT_DZ) {
          SPY.triggerEvents |= SPYRO_EV_HIT;
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        if (ApproxDist2D(&actor->posX, st->home) > (st->range << 10) + SWOOP_LEASH_SLACK) {
          *st->alert = 0;
          actor->mode = SWOOP_RETURN;
          continue;
        }
        CopyVector(floorProbe, &actor->posX);
        floorProbe[0] += g_anCosineLut[actor->rotZ] >> 3;
        floorProbe[1] += g_anSineLut[actor->rotZ] >> 3;
        floorProbe[2] += GROUND_PROBE_LIFT;
        g1 = FindGroundHeightBelow(floorProbe, SWOOP_PITCH_PROBE_DEPTH);
        CopyVector(floorProbe, &actor->posX);
        floorProbe[0] += (g_anCosineLut[WrapAddAngle8(actor->rotZ, 0x80)] * SWOOP_PITCH_PROBE_BACK) >> 10;
        floorProbe[1] += (g_anSineLut[WrapAddAngle8(actor->rotZ, 0x80)] * SWOOP_PITCH_PROBE_BACK) >> 10;
        floorProbe[2] += GROUND_PROBE_LIFT;
        g2 = FindGroundHeightBelow(floorProbe, SWOOP_PITCH_PROBE_DEPTH);
        if (ABS2(g1 - g2) < SWOOP_PITCH_MAX_STEP) {
          actor->rotY = ArcTan2(SWOOP_PITCH_BASE, g1 - g2, ATAN_ANGLE8);
          actor->rotY = StepAngle8Toward(actor->rotY, 0, SWOOP_PITCH_EASE);
        }
        break;
      }

      case SWOOP_RETURN: {
        int a = ArcTan2(st->home[0] - actor->posX,
                              st->home[1] - actor->posY, ATAN_ANGLE8);
        int d = ApproxDist2D(&actor->posX, st->home);
        int dz = st->home[2] - actor->posZ + SWOOP_RETURN_RAISE;

        if (d > SWOOP_RETURN_FAR) {
          dz = SPY.posZ - actor->posZ + SWOOP_RETURN_FAR_RAISE;
        }
        if (ABS(dz) >= SWOOP_RETURN_SNAP) {
          if (dz >= 0) {
            actor->posZ += SWOOP_RETURN_CLIMB_STEP;
          } else {
            actor->posZ -= SWOOP_RETURN_CLIMB_STEP;
          }
        }
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, SWOOP_ANIM_FLY);
        }
        if (st->kind == SWOOP_KIND_STRAIGHT) {
          TurnActorYawTowardAngle(actor, a, 7, 0, YAW_HOLD_IN_BAND);
        } else {
          TurnActorYawTowardAngle(actor, a, 2, 0, YAW_HOLD_IN_BAND);
        }
        MoveActorAlongHeading(actor, st->speed, 0, 0, 0);
        if (d < SWOOP_RETURN_SLOW_RANGE && st->speed >= SWOOP_RETURN_MIN_SPEED) {
          st->speed -= 5;
        }
        if (d < SWOOP_HOME_ARRIVE) {
          ENTER_POSE(actor, SWOOP_PERCH);
        }
        break;
      }

      case SWOOP_KNOCKED:
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0, 0);
          st->speed -= SWOOP_KNOCK_DRAG;
        }
        CopyVector(y320, &actor->posX);
        y320[2] += SWOOP_KNOCK_PROBE_RAISE;
        if (FindGroundHeightBelow(y320, SWOOP_KNOCK_PROBE_DEPTH) < actor->posZ) {
          actor->posZ -= SWOOP_KNOCK_SINK;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, SWOOP_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_GRID);
      FindGroundHeightBelow(&actor->posX, GROUND_PROBE_FAR);
      EncodeHitNormalToActorGroundCode(actor);
      break;
    }

    /* Wandering critter (RoamingCritterState): roams its patch, and is knocked away
     * (or stunned, when charged) when struck. */
    case ACTOR_TYPE_ROAMING_CRITTER: {
      RoamingCritterState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != ROAMCRIT_KNOCKED && actor->mode != ROAMCRIT_DYING) {
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        if (!(actor->flags & ACTOR_HIT_FLAME)) {
          st->knockSpeed = 0x190;
        } else {
          st->knockSpeed = 0;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        if (st->knockSpeed != 0) {
          ENTER_POSE(actor, ROAMCRIT_KNOCKED);
        }
        actor->mode = ROAMCRIT_DYING;
        ANIM_RESET(actor, ROAMCRIT_DYING);
        continue;
      }

      switch (actor->mode) {
      case ROAMCRIT_WANDER:
        UpdateActorWander(actor, &st->wander);
        break;

      case ROAMCRIT_KNOCKED:
        if (st->knockSpeed >= 0x10) {
          st->knockSpeed -= 0xF;
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, 0, 0x2BC, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }

      case ROAMCRIT_DYING:
        if (actor->anim == ROAMCRIT_DYING && actor->frame < 8) {
          UpdateActorWander(actor, &st->wander);
        }
        if (g_nActorAnimDone != 0) {
          if (actor->anim != ROAMCRIT_DYING) {
            DespawnActorRecord(actor);
            continue;
          }
          ANIM_SET(actor, ROAMCRIT_ANIM_DEAD);
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
          SubtractVector(y320, at, &spy->posX);
          y320[2] = (y320[2] * 3) >> 2;

          if (VectorLength(y320, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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

    /* Bouncing debris (BounceState). */
    case ACTOR_TYPE_BOUNCE_DEBRIS:
    case ACTOR_TYPE_BOUNCE_DEBRIS + 1: {
      BounceState *st = actor->state;

      if (st->life == 0 || actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= 5;
      if (--st->slow == 0) {
        st->vel[0] = 0;
        st->vel[1] = 0;
        st->vel[2] = 0;
      }
      if (actor->type == ACTOR_TYPE_BOUNCE_DEBRIS) {
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
      } else {
        if (st->vel[2] < -0x18) {
          st->vel[2] = -0x18;
        }
        if (st->mode == BOUNCE_SPIN) {
          actor->rotZ += 0xC;
        } else if (st->vel[2] < 0) {
          {
            int u = actor->rotY;
            int sw = st->sway;

            if (u + sw == 0x20 || u + sw == 0xE0) {
              st->sway = -sw;
            }
          }
          actor->rotY += st->sway;
          actor->posY += st->sway;
          if (GetRandomU32() & 1) {
            actor->rotZ += 1;
          }
        }
      }
      actor->posZ += st->vel[2];
      st->life--;
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
        if (ApproxDist2D(&actor->posX, &spy->posX) < WHIRL_WAKE_RANGE &&
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
      SubtractVector(y320, &spy->posX, &actor->posX);
      distance = VectorLength(y320, VEC_LEN_XY);
      if (distance < st->radius && y320[2] >= -WHIRL_BELOW_REACH && y320[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + WHIRL_TOP_MARGIN < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(spy->firstContactPoint, &actor->posX);
        if (st->flags & WHIRL_F_TARGET_CAM) {
          spyro->requestMask |= SPYRO_RQ_TARGET_CAMERA;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamFollowBehind;
          SPY.pCamAnchor = &spy->posX;
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

        y288[0] = (GetRandomU32() & 0xFE) - 0x7F;
        y288[1] = (GetRandomU32() & 0xFE) - 0x7F;
        y288[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(y288, y288, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, y288, 1);
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
    case ACTOR_TYPE_CHECKPOINT + 1:
      switch (actor->mode) {
      case CHECKPT_REARM:
        if (ApproxDist2D(&actor->posX, &SPY.posX) > CHECKPT_REARM_RANGE) {
          actor->mode = CHECKPT_ARMED;
        }
        break;

      case CHECKPT_ARMED:
        if (ApproxDist2D(&actor->posX, &spy->posX) < CHECKPT_TOUCH_RANGE) {
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

        SubtractVector(barrelAxis, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(barrelAxis, VEC_LEN_XY), barrelAxis[2], ATAN_ANGLE8);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < CANNON_LOW_PITCH_SPAN) {
          actor->rotY = CANNON_LOW_PITCH;
        }
        actor->rotZ = ArcTan2(barrelAxis[0], barrelAxis[1], ATAN_ANGLE8);
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
          muzzlePos[0] = 0;
          muzzlePos[1] = 0;
          muzzlePos[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, muzzlePos, muzzlePos);
          AddVector(muzzlePos, muzzlePos, &actor->posX);
          smokeVel[0] = 0;
          smokeVel[1] = 6;
          smokeVel[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, smokeVel, smokeVel);
          smokeVel[0] += (GetRandomU32() & 0xF) - 8;
          smokeVel[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, muzzlePos, (int)smokeVel);
          if (actor->voiceRow != VOICE_ROW_NONE) {
            continue;
          }
          PlayActorMeshVoice(actor, CANNON_VOICE_FUSE, 0);
          continue;
        }
        if (st->timer < CANNON_FIRE_TICK &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          muzzlePos[0] = 0;
          muzzlePos[1] = 0;
          muzzlePos[2] = CANNON_MUZZLE_LEN;
          RotateVectorByMatrix(actor->mtx, muzzlePos, muzzlePos);
          AddVector(muzzlePos, muzzlePos, &actor->posX);
          smokeVel[0] = 0;
          smokeVel[1] = ((st->timer - CANNON_SMOKE_TICKS) >> 3) + 9;
          smokeVel[2] = -0x12 - ((st->timer - CANNON_SMOKE_TICKS) >> 2);
          RotateVectorByMatrix(actor->mtx, smokeVel, smokeVel);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, muzzlePos, (int)smokeVel);
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
            ScaleVector3Sat(y416, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(ballToNode, ballToNode, y416);
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

      actor->soundRange = 0x20;
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
        y416[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        y416[1] = (g_anSineLut[i * 32] >> 8) * 3;
        y416[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)y416);
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
          y416[0] = (g_anCosineLut[bearing] * 3) >> 4;
          y416[1] = (g_anSineLut[bearing] * 3) >> 4;
          y416[2] = 0;
          letterSide[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          letterSide[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterSide, st->count - 1);
          LShiftVector3(letterSide, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          letterSwingRest[0] = y416[0] * g_anCosineLut[arm * 2];
          letterSwingRest[1] = y416[1] * g_anCosineLut[arm * 2];
          letterSwingRest[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterSide, letterSide, st->index);
          SubtractVector(letterSlot, letterSlot, letterSide);
          swing = &g_anCosineLut[bearing];
          actor->posX = y416[0] * *swing;
          actor->posY = y416[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, letterSwingRest);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, y416);
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
    

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_REBOUND_FX:
    case ACTOR_TYPE_HAZARD1_ENTER_FX:
    case ACTOR_TYPE_HAZARD1_DEATH_FX:
    case ACTOR_TYPE_BELL_RIPPLE:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;
}
  }
}
