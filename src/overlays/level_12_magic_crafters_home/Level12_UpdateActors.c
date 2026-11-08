/* Level12_UpdateActors -- per-frame actor update for level 12 (Magic
 * Crafters home).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 1,
 * 8, 9, 15, 25, 26 and 31); the level-12 classes are the floor-snapped marker
 * (328), the talking dragon variant (189), the villager (270) and the
 * shepherd (271).
 *
 * Load-bearing source forms (matched 2026-10-01):
 *   - Level 8's loop top with spy carried from g_anSpyroWorldPos (level 10's form);
 *     case 1 stores `oneq` so the byte `= 1` sites stay literal.
 *   - Twenty-four scratch arrays in the original frame order; case 271's
 *     index slot is `int[2]` so it is laid out at its declaration.
 *   - Case 270 declares its spilled pairGone before its voice locals, and
 *     reads the kill bitmap through idx/word/bit temps (word before bit).
 *   - Case 270 state 0x5B makes the distance call before computing the
 *     flee range: the range held across the call would take s0 from st.
 *   - Case 270's camera grab stores g_pSpyroRequestCameraAnchor/C78 first and uses the
 *     absolute symbols (g_dwSpyroRequestMask, g_bSpyroPersistentEulerZ) so the bodyRotZ load can be
 *     scheduled to the top of the block.
 *   - Case 271 keeps 0x4B0 in a local (`near`), steers with unsigned table
 *     reads (`<< 16 >> 19`), and spells the camera look x as `<< 7 >> 12`.
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
#include "level_id.h"
#include "emit_record.h"
#include "cd_stream.h"
#include "actor_cutscene.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. The
 * clones that need a loop-top carrier for 0x10 declare `int sixteen`; here
 * it is the plain constant, so arm bodies read the same in every level. */
#define ANIM_RATE16 sixteen
#define sixteen 0x10

void Level12_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  short oneq;
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
  int chestToSpyro[3];
  int keySlotOffset[3];
  int tgt271[3];
  int nearestNode[2];
  int shardTwinkle[3];
  int mat2[3][3];
  int x352[3];
  int x368[3];
  int col2[3];
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

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    oneq = 1;
    one = 1;
    spy = (SpyroObj *)&g_anSpyroWorldPos;

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
      int shouldSpawnText, cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == PORTALNAME_FAR;

        actor->mode = oneq;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ, ArcTan2(CAM.posX - actor->posX,
                                        CAM.posY - actor->posY, ATAN_ANGLE8));

        if (cameraAngleDiff <= 56) {
          if (actor->subMode == NAMESIDE_BACK) {
            shouldSpawnText = 1;
          }
          actor->subMode = NAMESIDE_FRONT;
        } else if (cameraAngleDiff > 71) {
          if (actor->subMode == NAMESIDE_FRONT) {
            shouldSpawnText = 1;
          }
          actor->subMode = one;
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

    /* Particle emitter on a random timer. */
    case ACTOR_TYPE_TIMED_EMITTER: {
      int *st;

      st = (int *)actor->state;
      if (TICK_DOWN(*st) == 0) {
        break;
      }
      g_pfnLevelEmitParticles(1, EMIT_DRIFTING_MOTE, &actor->posX, 0);
      *st = GetRandomU32() & 0xE;
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
        {
          char *base = (char *)&g_nSpyroWorldPosZ;

          if (*(int *)base - st->path->nodes[10].z < -THIEF_CLIMB_DOWN_DZ &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < THIEF_CLIMB_DOWN_RANGE) {
            st->vz = THIEF_CLIMB_DOWN_VZ;
            st->path->cur = 9;
            actor->mode = THIEF_CLIMB_HOP_DOWN;
            continue;
          }
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

    /* Path walker (PathState): waits for Spyro, then paces its path, turning
     * back at either end and choosing the node that leads away from him. */
    case ACTOR_TYPE_PATH_WALKER: {
      PathState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != PATHWALK_DYING) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        actor->mode = PATHWALK_DYING;
        ANIM_RESET(actor, PATHWALK_DYING);
        continue;
      }

      switch (actor->mode) {
      case PATHWALK_WAIT:
        if (ApproxDist2D(&actor->posX, &spy->posX) < PATHWALK_NOTICE_RANGE) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < PATHWALK_NOTICE_DZ : (SPY.posZ - height) < PATHWALK_NOTICE_DZ) {
            ENTER_POSE4(actor, PATHWALK_PACE);
          }
        }
        break;

      case PATHWALK_PACE:
        if (ApproxDist2D(&actor->posX,
                          &st->path->nodes[st->path->cur].x) < 0x400) {
          if (ApproxDist2D(&actor->posX, &spy->posX) <= PATHWALK_NOTICE_RANGE) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop > PATHWALK_NOTICE_DZ : (SPY.posZ - height) > PATHWALK_NOTICE_DZ) {
              goto pose0;
            }
            goto walk;
          }
        pose0:
          ENTER_POSE4(actor, PATHWALK_WAIT);
        walk:
          if (st->path->cur == 0 && st->loops == 0) {
            st->path->cur = 1;
          } else if (st->path->cur == st->path->count - 1 && st->loops == 0) {
            st->path->cur--;
          } else {
            int spyroAngle;
            int nextNode;
            int previousNode;
            int nextAngle;
            int previousAngle;

            spyroAngle = ArcTan2(spy->posX - actor->posX,
                                       spy->posY - actor->posY, ATAN_ANGLE8);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;
            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, ATAN_ANGLE8);
            previousAngle =
                ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                              st->path->nodes[previousNode].y - actor->posY, ATAN_ANGLE8);
            if (AbsAngleDelta8(spyroAngle, nextAngle) >
                AbsAngleDelta8(spyroAngle, previousAngle)) {
              st->path->cur = nextNode;
            } else {
              st->path->cur = previousNode;
            }
          }
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[st->path->cur].x -
                                            actor->posX,
                                        st->path->nodes[st->path->cur].y -
                                            actor->posY,
                                        ATAN_ANGLE8),
                          8, 0x20, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongHeading(actor, 0x60, 0x200, 0x200, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        }
        break;

      case PATHWALK_DYING:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
          continue;
        }
        break;

      case SPLITSHOT_FLY: {
        int i;
        int a;

        CopyVector(svD, &actor->posX);
        if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
          DespawnActorRecord(actor);
          continue;
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x80, SPYRO_EV_HIT_TINT | SPYRO_EV_UNREAD_20000) != 0) {
          i = 0;
          a = GetRandomU32() & 0x3F;
          for (; i < 4; i++, a += 0x40) {
            if (ACTOR_POOL_LOW()) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor)->rotX = a;
          }
          DespawnActorRecord(actor);
          continue;
        }
        if (TICK_DOWN(*timer) != 0 ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(svD, &actor->posX) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if ((*timer & 7) == 0) {
          g_pfnLevelEmitParticles(1, EMIT_DRIFT_SPARKLE, &actor->posX, 0x10);
        }
        break;
      }
      }
      break;
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
          chestToSpyro[0] = LOCKCHEST_LOCK_FWD;
          chestToSpyro[1] = 0;
          chestToSpyro[2] = LOCKCHEST_LOCK_UP;
          RotateVectorByMatrix(actor->mtx, chestToSpyro, chestToSpyro);
          AddVector(chestToSpyro, chestToSpyro, &actor->posX);
          SubtractVector(chestToSpyro, chestToSpyro, &spy->posX);
          chestToSpyro[2] -= 0x100;
          RShiftVector3(chestToSpyro, 5);
          ScaleVector3Sat(chestToSpyro, chestToSpyro, st->shake);
          DivideVector3(chestToSpyro, chestToSpyro, 6);
          AddVector(chestToSpyro, chestToSpyro, &spy->posX);
          chestToSpyro[2] += 0x100;
          chestToSpyro[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, chestToSpyro);
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
          keySlotOffset[0] = LOCKCHEST_LOCK_FWD - (st->shake - LOCKCHEST_INSERT_TICK) * 8;
          keySlotOffset[1] = 0;
          keySlotOffset[2] = LOCKCHEST_LOCK_UP;
          RotateVectorByMatrix(actor->mtx, keySlotOffset, keySlotOffset);
          AddVector(&key->posX, keySlotOffset, &actor->posX);
        }

        if (st->shake >= LOCKCHEST_BURST_TICK ||
            !(SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
              SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          int i;

          key->cullRadius = ACTOR_CULL_HIDDEN;
          key->updateDist = ACTOR_UPDATE_WHEN_DRAWN;
          key->drawn = 0;
          g_nCinematicActive = 0;
          actor->soundRange = 0x20;
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

    /* Type 189: talking dragon. When Spyro faces it up close, start its
     * dialogue; the first visit and the post-boss visit pick their own
     * lines. */
    case ACTOR_TYPE_HOME_TALKER + 2: {
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

        if (g_nTotalEggsRecovered < 5) {
          if (g_abWorldBalloonistState[3] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_LOCKED_FIRST);
            g_abWorldBalloonistState[3] = BALLOONIST_TALKED;
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_LOCKED_AGAIN);
          }
        } else if (g_abWorldBalloonistState[3] < BALLOONIST_OPEN) {
          if (g_abWorldBalloonistState[3] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_UNLOCK_FIRST);
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_UNLOCK_AGAIN);
          }
          g_abWorldBalloonistState[3] = BALLOONIST_OPEN;
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
          SubtractVector(tgt271, at, &spy->posX);
          tgt271[2] = (tgt271[2] * 3) >> 2;

          if (VectorLength(tgt271, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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

    /* Type 270: villager (VillagerState). */
    case ACTOR_TYPE_VILLAGER: {
      VillagerState *st = actor->state;
      int pairGone;
      int animState;

      if (st->init == 0) {
        int gone;
        int idx;
        int word;
        int bit;

        st->init = one;
        gone = 0;
        idx = actor - g_pActorListBase;
        word = idx >> 5;
        bit = idx & 0x1F;
        if (g_anCheckpointKillBitmap[word] & (one << bit)) {
          gone = actor->dropKind == DROP_GEM_ROLL;
        }
        if (st->mode == VROLE_PAIR) {
          Actor *pair = &g_pActorListBase[st->link];

          pairGone = 0;
          idx = pair - g_pActorListBase;
          word = idx >> 5;
          bit = idx & 0x1F;
          if ((g_anCheckpointKillBitmap[word] & (one << bit)) &&
              (pair->mode >= ACTOR_MODE_DORMANT || pair->dropKind == DROP_GEM_ROLL)) {
            pairGone = 1;
          }
        }
        if (st->mode == VROLE_PAIR && pairGone == 0) {
          continue;
        }
        if (gone) {
          if (st->mode == VROLE_PAIR && pairGone) {
            if (g_pActorListBase[st->link].mode < ACTOR_MODE_DORMANT) {
              DespawnActorRecord(&g_pActorListBase[st->link]);
            }
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
          StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
          DespawnActorRecord(actor);
          continue;
        }
      }

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != VILLAGER_KNOCKED) {
        st->speed = VILLAGER_KNOCK_SPEED;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ENTER_POSE(actor, VILLAGER_KNOCKED);
      }

      switch (actor->mode) {
      case VILLAGER_IDLE: {
        int dist;
        int animState;
        int marker;
        int range;

        dist = ApproxDist2D(&actor->posX, &SPY.posX);
        if (st->friend >= 0 && (st->mode != VROLE_PAIR || actor->subMode == 0) &&
            (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, actor, (int)(&g_pActorListBase[st->friend]));
        }
        if (st->chunkAnim != -1) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          marker = CHUNK_ANIM_STEP(animState);
        }
        if (dist < st->camDist << 10 && (st->camAlways != 0 || marker >= 2) &&
            SPY.state != SPYRO_ST_CHARGE && SPY.state != SPYRO_ST_CHARGE_AIR &&
            AbsAngleDelta8(SPY.bodyRotZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, ATAN_ANGLE8)) < VILLAGER_CAM_FACING_CONE) {
          g_pSpyroRequestCameraAnchor = &spy->posX;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
          g_nCameraActiveTargetDistance = VILLAGER_CAM_DISTANCE;
          g_nCameraActiveTargetPitch = 0x80;
          g_nCameraActiveTargetPitchBias = -0xB0;
          g_nCameraActiveTargetRoll = 0;
          g_nCameraActiveTargetYawBias = 0;
          g_anCameraActiveTargetParams.w[0] = 0x800 - g_bSpyroPersistentEulerZ * 16;
        }
        if (st->chunkAnim == -1) {
          break;
        }
        if (st->chunkAnim == 0 && st->timer < 3 && (animState & CHUNK_ANIM_HALTED) && g_nActiveLevelId == LEVEL_ALPINE_RIDGE) {
          Actor *f = &g_pActorListBase[st->friend];

          PlaySoundEffect(g_pLevelSampleBankHeader[0x2A], f, SFX_MODE_3D, &f->voiceRow);
        }
        if (st->mode == VROLE_CHAT) {
          range = st->talkDist << 10;

          if (SPY.state != SPYRO_ST_CHARGE || st->noBoost == 0) {
            range += VILLAGER_TALK_BOOST;
          }
          SnapActorToGround(actor);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          if (marker < 2) {
            ANIM_GO(actor, VILLAGER_IDLE);
            if (dist < range) {
              if (st->timer == 0) {
                st->timer = st->timerLong;
              } else if (TICK_DOWN(st->timer) == 2 && (animState & CHUNK_ANIM_HALTED)) {
                ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              }
            } else {
              TICK_DOWN(st->timer);
            }
          } else {
            TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
            if (TICK_DOWN(st->idleTimer)) {
              st->idleTimer = (GetRandomU32() & 0x2F) + 0x3C;
              PlayActorMeshVoice(actor, (GetRandomU32() & 1) | 4, 0);
            }
            ANIM_GO(actor, VILLAGER_ANIM_CHATTER);
            if (range + VILLAGER_TALK_SLACK < dist && (animState & CHUNK_ANIM_HALTED)) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              st->timer = 0x1E;
            }
          }
        } else if (st->mode == VROLE_FACE) {
          if (st->friend > 0) {
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->friend].posX - actor->posX,
                                        g_pActorListBase[st->friend].posY - actor->posY, ATAN_ANGLE8),
                          6, 0, YAW_HOLD_IN_BAND);
          }
          SnapActorToGround(actor);
          if (TICK_DOWN(st->faceTimer) == 0) {
            break;
          }
          if ((SPY.state == SPYRO_ST_SUPERCHARGE || SPY.state == SPYRO_ST_SUPERCHARGE_AIR ||
               (SPY.state == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) &&
              g_nActiveLevelId == LEVEL_MAGIC_CRAFTERS && marker < 2) {
            break;
          }
          if ((st->fleeDist << 10) < dist && TICK_DOWN(st->timer)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            if (marker >= 2) {
              st->timer = st->timerLong;
            } else {
              st->timer = st->timerShort;
            }
          } else if (dist < st->fleeDist << 10) {
            if (actor->group == 0xFF) {
              actor->mode = VILLAGER_PANIC;
              continue;
            }
            SetActorGroupStateExcept(actor, VILLAGER_PANIC, VILLAGER_KNOCKED);
          }
        } else if (st->mode == VROLE_PAIR) {
          if (actor->subMode == 0 &&
              (ApproxDist2D(&actor->posX, &SPY.posX) < VILLAGER_PAIR_WAKE_RANGE ||
               g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            actor->subMode = 1;
            st->timer = 0x46;
          } else if (TICK_DOWN(st->timer) == 2) {
            g_pActorListBase[st->link].subMode = 1;
          }
          if ((g_pActorListBase[st->link].nextAnim == VILLAGER_KNOCKED || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) &&
              ApproxDist2D(&actor->posX, &SPY.posX) <
                  (st->fleeDist << 10) + VILLAGER_PAIR_FLEE_SLACK) {
            actor->mode = VILLAGER_PANIC;
            continue;
          }
        } else if (st->mode == VROLE_LEADER) {
          Actor *lead = &g_pActorListBase[st->link];
          SorcererState *ls = lead->state;

          SnapActorToGround(actor);
          TurnActorYawTowardAngle(actor,
                        ArcTan2(lead->posX - actor->posX,
                                      lead->posY - actor->posY, ATAN_ANGLE8),
                        6, 0, YAW_HOLD_IN_BAND);
          if (marker < 3) {
            if (TICK_DOWN(st->timer) && ls->path->cur == 7) {
              st->timer = 0x12C;
              ModifyWorldChunkAnimFlags(VILLAGER_CHUNK_LEADER_CUE, CHUNK_ANIM_RESUME, 0);
              ls->timer = 8;
              ls->sub = SORCERER_STEP_REST;
            }
            if (ANIM_AT_FRAME(lead, 4, 4) &&
                (int)(GetRandomU32() & 0xFF) >= 0xD3) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              st->timer = 0x5A;
            }
            if (ANIM_AT_FRAME(lead, 4, 8) &&
                lead->frameProgress < 0x11) {
              ANIM_GO(actor, VILLAGER_ANIM_REACT);
            }
            if (NEXT_ANIM_AT_FRAME(actor, VILLAGER_ANIM_REACT, 0x14) &&
                g_nActorAnimNewFrame != 0) {
              ANIM_GO(actor, VILLAGER_IDLE);
            }
          } else if (TICK_DOWN(st->timer)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        break;
      }

      case VILLAGER_KNOCKED:
        if (st->speed != 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0x2BC, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          st->speed -= VILLAGER_KNOCK_DRAG;
          if (st->speed < 0) {
            st->speed = 0;
          }
        }
        if (g_nActorAnimDone != 0) {
          actor->cullRadius = ACTOR_CULL_HIDDEN;
        }
        if (st->chunkAnim != -1 && st->chunkAnimDone == 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if (animState & CHUNK_ANIM_HALTED) {
            if (CHUNK_ANIM_STEP(animState) >= 2) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            } else {
              st->chunkAnimDone = one;
            }
          }
        } else {
          st->chunkAnimDone = one;
        }
        if (actor->cullRadius == 0 && st->chunkAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case VILLAGER_PANIC:
        StopSoundVoiceByIndex(actor->voiceRow, VOICE_RELEASE);
        st->idleTimer = 0;
        SnapActorToGround(actor);
        if (st->stopVoice != 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if ((animState & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(animState) >= 2) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        ANIM_GO(actor, VILLAGER_ANIM_COWER);
        if (g_nActorAnimDone != 0) {
          st->speed = VILLAGER_PANIC_SPEED;
          st->timer = RandomIntInRange(VILLAGER_RUN_MIN_FRAMES, VILLAGER_RUN_MAX_FRAMES);
          actor->mode = VILLAGER_RUN;
          continue;
        }
        break;

      case VILLAGER_RUN:
        if (TICK_DOWN(st->idleTimer)) {
          st->idleTimer = (GetRandomU32() & 0x2F) + 0x5A;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 4, 0);
        }
        SnapActorToGround(actor);
        if (st->stopVoice != 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if ((animState & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(animState) >= 2) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        ANIM_GO(actor, VILLAGER_ANIM_RUN);
        if (ApproxDist2D(&actor->posX, PATH_PT(st->path, st->path->cur)) < VILLAGER_NODE_ARRIVE) {
          int next;

          do {
            next = RandomIntInRange(0, st->path->count - 1);
          } while (next == st->path->cur);
          st->path->cur = next;
          st->speed = RandomIntInRange(VILLAGER_RUN_MIN_SPEED, VILLAGER_RUN_MAX_SPEED);
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(PATH_X(st->path, st->path->cur) - actor->posX,
                                        PATH_Y(st->path, st->path->cur) - actor->posY,
                                        ATAN_ANGLE8),
                          7, 0x50, one)) {
          MoveActorAlongHeading(actor, st->speed, 0, 0, one);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >
                (st->fleeDist << 10) + VILLAGER_CALM_SLACK &&
            (st->mode != VROLE_PAIR || g_pActorListBase[st->link].nextAnim != VILLAGER_KNOCKED)) {
          ENTER_POSE(actor, VILLAGER_IDLE);
        }
        if (TICK_DOWN(st->timer)) {
          st->idleTimer = 0x1E;
          st->runFrom = ApproxDist2D(&actor->posX, &SPY.posX);
          ANIM_BLEND(actor, VILLAGER_ANIM_CHATTER);
          actor->mode = VILLAGER_STOP;
          continue;
        }
        break;

      case VILLAGER_STOP:
        if (TICK_DOWN(st->idleTimer)) {
          st->idleTimer = (GetRandomU32() & 0x2F) + 0x3C;
          PlayActorMeshVoice(actor, (GetRandomU32() & 1) | 4, 0);
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &SPY.posX) < st->runFrom - VILLAGER_STOP_SLACK) {
          actor->mode = VILLAGER_PANIC;
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 271: shepherd (ShepherdState). */
    case ACTOR_TYPE_SHEPHERD: {
      ShepherdState *st = actor->state;
      int near = SHEPHERD_SWING_RANGE;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) != 0 && actor->mode != SHEPHERD_KNOCKED) {
        st->knockSpeed = SHEPHERD_KNOCK_SPEED;
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->knockZSpeed = SHEPHERD_KNOCK_VZ;
        actor->flags = 0;
        ((CollectibleState *)SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0)->state)->mode = COLLREST_SNAP;
        MarkActorLiveKill(actor);
        PlayActorMeshVoice(actor, SHEPHERD_VOICE_KNOCKED, 0);
        ENTER_POSE(actor, SHEPHERD_KNOCKED);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case SHEPHERD_IDLE:
        switch (st->post) {
        case SHEPHERD_POST_GUARD: {
          char *base = (char *)&g_nSpyroWorldPosZ;
          int h = actor->posZ - actor->floorOffset - *(int *)base;

          ANIM_GO(actor, SHEPHERD_IDLE);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          if (TICK_DOWN(st->timer) &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < SHEPHERD_GUARD_SWING_RANGE &&
              h < 0 && h > -SHEPHERD_GUARD_SWING_ABOVE) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_RUN_PATH:
          ANIM_GO(actor, SHEPHERD_ANIM_RUN);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_RUN_PATH_WAKE_RANGE) {
            st->done = 0;
            *st->doneFlag = 0;
            SetActorGroupStateExcept(actor, SHEPHERD_CHASE, SHEPHERD_KNOCKED);
          }
          break;

        case SHEPHERD_POST_LOITER:
          if (actor->nextAnim != SHEPHERD_ANIM_LOITER) {
            g_nActorAnimDone = 0;
            ANIM_ADVANCE(actor, SHEPHERD_ANIM_LOITER);
            actor->nextFrame = RandomIntInRange(0, 0xF);
          }
          if (st->heading == -1) {
            st->heading = actor->rotZ;
          }
          TurnActorYawTowardAngle(actor, st->heading, 4, 0, YAW_HOLD_IN_BAND);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_WAKE_RANGE) {
            SetActorGroupStateExcept(actor, SHEPHERD_CHASE, SHEPHERD_KNOCKED);
          }
          break;

        case SHEPHERD_POST_GUARD_LEVEL:
          ANIM_GO(actor, SHEPHERD_IDLE);
          TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
          SnapActorToGround(actor);
          if (TICK_DOWN(st->timer) &&
              ApproxDist2D(&actor->posX, &spy->posX) < near &&
              ABS2(SPY_DZ(actor)) < SHEPHERD_SWING_DZ) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;

        case SHEPHERD_POST_THROWER:
          if (st->heading == -1) {
            st->heading = actor->rotZ;
          }
          TurnActorYawTowardAngle(actor, st->heading, 4, 0, YAW_HOLD_IN_BAND);
          ANIM_GO(actor, SHEPHERD_ANIM_THROW);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_THROWER_WAKE_RANGE) {
            Actor *partner = &g_pActorListBase[st->partner];
            ShepherdState *ps;

            partner->mode = SHEPHERD_CHASE;
            ps = partner->state;
            ps->vz = 0x14;
            ps->side = 0;
            ANIM_GO(actor, SHEPHERD_ANIM_SIGNAL);
            actor->mode = SHEPHERD_WATCH;
            continue;
          }
          if (g_nActorAnimNewFrame != 0 && actor->nextFrame == SHEPHERD_THROW_FRAME) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
            CrookState *cs = c->state;

            GetActorAttachPoint(actor, 0, &c->posX);
            c->rotZ = actor->rotZ;
            cs->owner = actor;
            cs->live = one;
            cs->heading = actor->rotZ;
            cs->life = SHEPHERD_CROOK_LIFE_SHORT;
            cs->speed = SHEPHERD_CROOK_SPEED;
          }
          break;

        case SHEPHERD_POST_LEAPER:
          if (actor->subMode != 0) {
            TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
            if (ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_WAKE_RANGE) {
              if (actor->nextAnim == SHEPHERD_SWING) {
                g_nActorAnimDone = 0;
                actor->frameProgress = sixteen;
                actor->frameRate = sixteen;
                actor->nextAnim = SHEPHERD_ANIM_RUN;
                actor->nextFrame = 0;
                OnActorAnimQueued(actor);
              }
              actor->mode = SHEPHERD_CHASE;
              continue;
            }
            ANIM_GO(actor, SHEPHERD_ANIM_WATCH);
          } else {
            ANIM_GO(actor, SHEPHERD_ANIM_THROW);
            if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
                g_nSpyroAirborneFrames == 0 && SPY.posZ - PATH_Z(st->path, 0) > 0) {
              Actor *partner = &g_pActorListBase[st->partner];

              partner->mode = SHEPHERD_ROUSE;
              st->vz = SHEPHERD_LEAP_VZ;
              ENTER_POSE(actor, SHEPHERD_LEAP_ON);
            }
          }
          if (g_nActorAnimNewFrame != 0 && actor->nextFrame == SHEPHERD_THROW_FRAME) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
            CrookState *cs = c->state;

            GetActorAttachPoint(actor, 0, &c->posX);
            c->rotZ = actor->rotZ;
            cs->owner = actor;
            cs->live = one;
            cs->heading = actor->rotZ;
            cs->life = SHEPHERD_CROOK_LIFE;
            cs->speed = SHEPHERD_CROOK_SPEED;
          }
          break;

        case SHEPHERD_POST_PAIR_PATROL: {
          Actor *partner = &g_pActorListBase[st->partner];

          if (TICK_DOWN(st->flankTimer) &&
              ApproxDist2D(&actor->posX, &spy->posX) < SHEPHERD_FLANK_RANGE) {
            int mine = ArcTan2(actor->posX - spy->posX,
                                     actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
            int theirs =
                ArcTan2(partner->posX - spy->posX, partner->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
            ShepherdState *ps = partner->state;

            if (WrapAngleDelta8(mine, theirs) < 0) {
              st->side = 2;
              ps->side = one;
            } else {
              st->side = one;
              ps->side = 2;
            }
            SetActorGroupStateExcept(actor, SHEPHERD_ALERTED, SHEPHERD_KNOCKED);
          }
          if (st->sub == SHEPHERD_PAIR_WALK) {
            ANIM_GO(actor, SHEPHERD_ANIM_RUN);
            PivotActorAlongPath(actor, st->path, 0x100, 0x48, 0xC8, 3, 8,
                          actor->group, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (TICK_DOWN(st->timer) && partner->mode < ACTOR_MODE_DORMANT &&
                ApproxDist2D(&actor->posX, &partner->posX) < 0x8FC) {
              st->timer = 0x5A;
              st->sub = one;
            }
          } else if (st->sub == one) {
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(partner->posX - actor->posX,
                                            partner->posY - actor->posY, ATAN_ANGLE8),
                              4, 0xA, one)) {
              st->sub = SHEPHERD_PAIR_FACE;
            }
          } else if (st->sub == SHEPHERD_PAIR_FACE) {
            ANIM_GO(actor, SHEPHERD_ANIM_FACE_PARTNER);
            if (g_nActorAnimDone != 0) {
              st->sub = SHEPHERD_PAIR_WALK;
            }
          }
          break;
        }

        case SHEPHERD_POST_PATROL:
          if (TICK_DOWN(st->flankTimer) &&
              ABS2(SPY_DZ(actor)) < SHEPHERD_NOTICE_DZ &&
              ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_WAKE_RANGE) {
            actor->mode = SHEPHERD_CHASE;
            continue;
          }
          ANIM_GO(actor, SHEPHERD_ANIM_RUN);
          PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0xC8, 6, 0x28, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          break;

        case SHEPHERD_POST_WATCH:
          TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
          if (g_nSpyroAirborneFrames == 0 && ABS2(SPY_DZ(actor)) < SHEPHERD_NOTICE_DZ &&
              ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_WATCH_WAKE_RANGE) {
            actor->mode = SHEPHERD_CHASE;
            continue;
          }
          break;

        case SHEPHERD_POST_COVER: {
          Actor *partner = &g_pActorListBase[st->partner];
          Actor *q = &g_pActorListBase[st->partner2];

          if (partner->nextAnim == SHEPHERD_KNOCKED || q->nextAnim == SHEPHERD_KNOCKED) {
            st->post = SHEPHERD_POST_WATCH;
            ENTER_POSE(actor, SHEPHERD_RECOVER);
          }
          switch (actor->subMode) {
          case 0: {
            int ang = ArcTan2(partner->posX - actor->posX,
                                    partner->posY - actor->posY, ATAN_ANGLE8);

            ANIM_GO(actor, SHEPHERD_ANIM_THROW);
            TurnActorYawTowardAngle(actor, ang, 4, 0, YAW_HOLD_IN_BAND);
            if (ApproxDist2D(&actor->posX, &SPY.posX) < SHEPHERD_COVER_GUARD_RANGE) {
              actor->subMode = 1;
            }
            if (g_nActorAnimNewFrame != 0 && actor->nextFrame == SHEPHERD_THROW_FRAME) {
              Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
              CrookState *cs = c->state;

              GetActorAttachPoint(actor, 0, &c->posX);
              c->rotZ = actor->rotZ;
              cs->owner = actor;
              cs->live = one;
              cs->heading = actor->rotZ;
              cs->life = SHEPHERD_CROOK_LIFE;
              cs->speed = SHEPHERD_CROOK_SPEED;
            }
            break;
          }
          case 1: {
            int *pos = &actor->posX;
            int d = ApproxDist2D(pos, &spy->posX);

            ANIM_GO(actor, SHEPHERD_IDLE);
            TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
            if (d > SHEPHERD_COVER_RELEASE_RANGE) {
              actor->subMode = 0;
            }
            if (TICK_DOWN(st->timer) &&
                ApproxDist2D(pos, &spy->posX) < near &&
                ABS2(SPY_DZ(actor)) < SHEPHERD_SWING_DZ) {
              ENTER_POSE(actor, SHEPHERD_SWING);
            }
            break;
          }
          }
          break;
        }
        }
        break;

      case SHEPHERD_CHASE:
        ANIM_GO(actor, SHEPHERD_ANIM_RUN);
        switch (st->post) {
        case SHEPHERD_POST_RUN_PATH:
          if (st->done == 0 &&
              PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 0x10, 0x80, PIVOT_TURN_ANY,
                            MOVE_LIFT | MOVE_FOLLOW_FLOOR) == PATH_ARRIVED(0)) {
            st->done = one;
            *st->doneFlag = one;
            st->post = SHEPHERD_POST_GUARD;
            ENTER_POSE(actor, SHEPHERD_IDLE);
          }
          break;

        case SHEPHERD_POST_LOITER:
        case SHEPHERD_POST_PAIR_PATROL:
        case SHEPHERD_POST_PATROL: {
          int side = st->side;
          int ang;

          if (side != 0 && SPY_FACING(actor) > SHEPHERD_TURNED_AWAY) {
            side = 3 - side;
          }
          CopyVector(tgt271, &spy->posX);
          if (side == one) {
            tgt271[0] += (unsigned short)g_anCosineLut[WrapAddAngle8(g_bSpyroPersistentEulerZ, -0x40)] << 16 >> 19;
            tgt271[1] += (unsigned short)g_anSineLut[WrapAddAngle8(SPY.bodyRotZ, -0x40)] << 16 >> 19;
          } else if (side == 2) {
            tgt271[0] += (unsigned short)g_anCosineLut[WrapAddAngle8(g_bSpyroPersistentEulerZ, 0x40)] << 16 >> 19;
            tgt271[1] += (unsigned short)g_anSineLut[WrapAddAngle8(SPY.bodyRotZ, 0x40)] << 16 >> 19;
          }
          ang = ArcTan2(tgt271[0] - actor->posX, tgt271[1] - actor->posY,
                              ATAN_ANGLE8);
          if (TICK_DOWN(st->timer) &&
              TurnActorYawTowardAngle(actor, ang, 6, 0x14, one) &&
              SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
              MoveActorAlongHeading(actor, SHEPHERD_CHASE_SPEED, 0x12C, 0x1F4, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_LOITER && SPY_FACING(actor) > SHEPHERD_TURNED_AWAY) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_LOITER &&
              ApproxDist2D(&actor->posX, st->home) > SHEPHERD_LOITER_LEASH) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_PAIR_PATROL || st->post == SHEPHERD_POST_PATROL) {
            if (FindNearestPathPointToActor(actor, st->path, nearestNode) > SHEPHERD_PATH_LEASH) {
              if (st->post == SHEPHERD_POST_PATROL) {
                st->path->cur = nearestNode[0];
                CopyVector(st->home, PATH_PT(st->path, nearestNode[0]));
              }
              actor->mode = SHEPHERD_GO_HOME;
              continue;
            }
          }
          if (TICK_DOWN(st->timer) && SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
              ApproxDist2D(&actor->posX, &spy->posX) < near &&
              g_nSpyroAirborneFrames == 0 && SPY_DZ(actor) < SHEPHERD_SWING_ABOVE &&
              ABS2(SPY_DZ(actor)) < SHEPHERD_SWING_CHASE_DZ) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_LEAPER: {
          int *pos;

          if (TrackEntityYawTowardSpyro(actor, 6, 0x14, YAW_STEP_IN_BAND)) {
            if (TICK_DOWN(st->timer) == 0) {
              break;
            }
            if (SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
                MoveActorAlongHeading(actor, SHEPHERD_CHASE_SPEED, 0, 0x1F4, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE)) {
              actor->mode = SHEPHERD_GO_HOME;
              continue;
            }
          }
          pos = &actor->posX;
          if (ApproxDist2D(pos, st->home) > SHEPHERD_LEAPER_LEASH) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (TICK_DOWN(st->timer) && SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
              ApproxDist2D(pos, &spy->posX) < near && SPY_DZ(actor) < SHEPHERD_SWING_ABOVE &&
              ABS2(SPY_DZ(actor)) < SHEPHERD_SWING_CHASE_DZ) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_WATCH: {
          int *pos;

          if (TrackEntityYawTowardSpyro(actor, 6, 0x14, YAW_STEP_IN_BAND) && SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
              MoveActorAlongHeading(actor, SHEPHERD_CHASE_SPEED, 0x12C, 0x1F4, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          pos = &actor->posX;
          if (ApproxDist2D(pos, st->home) > SHEPHERD_WATCH_LEASH) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (TICK_DOWN(st->timer) && SPY_FACING(actor) < SHEPHERD_FACING_BAND &&
              ApproxDist2D(pos, &spy->posX) < near && SPY_DZ(actor) < SHEPHERD_SWING_ABOVE) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }
        }
        break;

      case SHEPHERD_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed,
                      0x12, 0xC);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case SHEPHERD_SWING:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        SnapActorToGround(actor);
        if (actor->nextFrame >= SHEPHERD_SWING_END_FRAME) {
          if (st->post == SHEPHERD_POST_LOITER) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          st->timer = SHEPHERD_SWING_COOLDOWN;
          actor->mode = SHEPHERD_IDLE;
          continue;
        }
        CAM_LOOK.x = (g_anSineLut[actor->rotZ] << 7) >> 12;
        CAM_LOOK.y = -(g_anCosineLut[actor->rotZ] << 7) >> 12;
        CAM_LOOK.z = 0;
        break;

      case SHEPHERD_WATCH: {
        Actor *partner = &g_pActorListBase[st->partner];

        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, SHEPHERD_IDLE);
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0xC00 &&
            ABS2(SPY_DZ(actor)) < 0x258) {
          st->post = SHEPHERD_POST_GUARD;
          actor->mode = SHEPHERD_IDLE;
          continue;
        }
        if (partner->anim == SHEPHERD_KNOCKED) {
          ENTER_POSE(actor, SHEPHERD_RECOVER);
        }
        break;
      }

      case SHEPHERD_RECOVER:
        if (g_nActorAnimDone != 0) {
          st->post = SHEPHERD_POST_GUARD;
          ENTER_POSE(actor, SHEPHERD_IDLE);
        }
        break;

      case SHEPHERD_ROUSE:
        if (TrackEntityYawTowardSpyro(actor, 8, 0x23, YAW_STEP_IN_BAND) == 0) {
          break;
        }
        ANIM_GO(actor, SHEPHERD_RECOVER);
        if (g_nActorAnimDone == 0) {
          break;
        }
        /* fall through */
      case SHEPHERD_ALERTED:
        st->flankTimer = 0xB4;
        actor->mode = SHEPHERD_CHASE;
        continue;

      case SHEPHERD_LEAP_ON: {
        Actor *partner = &g_pActorListBase[st->partner];
        int d;

        ((ShepherdState *)partner->state)->timer = 0xE;
        RefreshActorDerivedState(partner, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(partner, 0, tgt271);
        if (st->vz < -SHEPHERD_LEAP_MAX_FALL) {
          st->vz = -SHEPHERD_LEAP_MAX_FALL;
        }
        if (partner->mode == SHEPHERD_KNOCKED) {
          st->vz = SHEPHERD_THROWN_VZ;
          st->knockSpeed = SHEPHERD_THROWN_SPEED;
          st->knockHeading = ArcTan2(actor->posX - spy->posX,
                                           actor->posY - spy->posY, ATAN_ANGLE8);
          actor->mode = SHEPHERD_THROWN;
          continue;
        }
        if (st->vz < 0 &&
            actor->posZ - actor->floorOffset - tgt271[2] <= ABS(st->vz)) {
          partner->mode = SHEPHERD_ALERTED;
          actor->mode = SHEPHERD_RIDE;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= SHEPHERD_GRAVITY;
        d = ApproxDist2D(&actor->posX, tgt271);
        if (d > 0xC8) {
          MoveActorAlongAngle(actor,
                        ArcTan2(tgt271[0] - actor->posX,
                                      tgt271[1] - actor->posY, ATAN_ANGLE8),
                        0xC8, 0, 0, 0);
        } else {
          TurnActorYawTowardAngle(actor, partner->rotZ, 6, 0, YAW_HOLD_IN_BAND);
          actor->posX = partner->posX;
          actor->posY = partner->posY;
        }
        if (d < 0x5DC) {
          TurnActorYawTowardAngle(actor, partner->rotZ, 4, 0, YAW_HOLD_IN_BAND);
        }
        break;
      }

      case SHEPHERD_RIDE: {
        Actor *partner = &g_pActorListBase[st->partner];

        ((ShepherdState *)partner->state)->timer = 0xA;
        ANIM_GO(actor, SHEPHERD_ANIM_RIDE);
        if (partner->nextAnim == SHEPHERD_KNOCKED) {
          st->vz = SHEPHERD_THROWN_VZ;
          st->knockSpeed = SHEPHERD_THROWN_SPEED;
          st->knockHeading = ArcTan2(actor->posX - spy->posX,
                                           actor->posY - spy->posY, ATAN_ANGLE8);
          actor->mode = SHEPHERD_THROWN;
          continue;
        }
        RefreshActorDerivedState(partner, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(partner, 0, &actor->posX);
        TurnActorYawTowardAngle(actor, partner->rotZ, 6, 0, YAW_HOLD_IN_BAND);
        break;
      }

      case SHEPHERD_THROWN: {
        int floor = ProbeActorGroundHeight(actor);
        int z = actor->posZ + st->vz;

        if (floor + actor->floorOffset < z) {
          actor->posZ = z;
          st->vz -= SHEPHERD_GRAVITY;
          if (st->vz < -SHEPHERD_THROWN_MAX_FALL) {
            st->vz = -SHEPHERD_THROWN_MAX_FALL;
          }
        } else {
          actor->posZ = floor + actor->floorOffset;
        }
        if (st->knockSpeed != 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, 0, 0x2BC, one);
          st->knockSpeed -= SHEPHERD_THROWN_DRAG;
          if (st->knockSpeed < 0) {
            st->knockSpeed = 0;
          }
          if (st->knockSpeed != 0) {
            break;
          }
        }
        if (actor->posZ == floor + actor->floorOffset) {
          CopyVector(st->home, PATH_PT(st->path, 0));
          st->timer = 0x14;
          actor->subMode = 1;
          actor->mode = SHEPHERD_CHASE;
          continue;
        }
        break;
      }

      case SHEPHERD_GO_HOME: {
        int ang = ArcTan2(st->home[0] - actor->posX,
                                st->home[1] - actor->posY, ATAN_ANGLE8);

        ANIM_GO(actor, SHEPHERD_ANIM_RUN);
        if (TurnActorYawTowardAngle(actor, ang, 4, 0x14, one)) {
          MoveActorAlongHeading(actor, SHEPHERD_HOME_SPEED, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (ApproxDist2D(&actor->posX, st->home) < SHEPHERD_HOME_ARRIVE) {
            actor->subMode = 1;
            st->timer = SHEPHERD_HOME_REST;
            st->vz = 0;
            ENTER_POSE(actor, SHEPHERD_IDLE);
          }
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
          st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
                                    spy->bodyRotZ, 0x20, 0x60);
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
            if (ApproxDist2D(&actor->posX, &spy->posX) < PATHGUARD_THROW_RANGE) {
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
          if (ApproxDist2D(&actor->posX, &spy->posX) < PATHGUARD_THROW_RANGE) {
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
                            st->heading, 8, PATHGUARD_RING_SPEED, 0xE, 0x80, STEER_NO_BLOCK, STEER_NO_BLOCK, 0x180,
                            0x15C, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
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
        MoveActorAlongHeading(actor, PATHGUARD_WALK_SPEED, 0x180, 0x180, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
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
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, 0, 0xC, 0);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
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

        tgt271[0] = (GetRandomU32() & 0xFE) - 0x7F;
        tgt271[1] = (GetRandomU32() & 0xFE) - 0x7F;
        tgt271[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(tgt271, tgt271, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, tgt271, 1);
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

    /* Type 328: floor-snapped marker. Drops onto the floor once, then sits at
     * the centroid of the triangle it landed on. */
    case ACTOR_TYPE_FLOOR_MARKER: {
      int *tri = actor->state;

      if (*tri < 0) {
        int floor;
        int d;

        actor->posZ += 0x200;
        floor = FindGroundHeightBelow(&actor->posX, 0x400);
        d = actor->posZ - floor;
        if (ABS(d) < 0x400) {
          actor->posZ = floor;
          *tri = g_nCollisionHitTriIndex;
        } else {
          actor->posZ -= 0x200;
        }
        if (*tri < 0) {
          break;
        }
      }
      UnpackWorldCollisionTri(*tri, mat2[0]);
      CopyVector(&actor->posX, mat2[0]);
      AddVector(&actor->posX, &actor->posX, mat2[1]);
      AddVector(&actor->posX, &actor->posX, mat2[2]);
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
          ZeroVector(x352);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)x352);
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
            CopyVector(x368, &st->prize->posX);
            x368[0] += g_anCosineLut[j * 43] >> 6;
            x368[1] += g_anSineLut[j * 43] >> 6;
            x368[2] += 0x28;
            col2[0] = g_anCosineLut[j * 43] >> 7;
            col2[1] = g_anSineLut[j * 43] >> 7;
            col2[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, x368, (int)col2);
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:
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
          SubtractVector(x352, &a->posX, &actor->posX);
          d = ABS2(x352[0]) + ABS2(x352[1]);
          if (d < LID_TARGET_PREFILTER &&
              (unsigned int)(VectorLength(x352, VEC_LEN_XY) - LID_TARGET_MIN_RANGE) <=
                  LID_TARGET_MAX_RANGE - LID_TARGET_MIN_RANGE &&
              (unsigned int)(x352[2] + LID_TARGET_DZ) <= 2 * LID_TARGET_DZ) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, ATAN_ANGLE12),
                              ArcTan2(x352[0], x352[1], ATAN_ANGLE12));
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
        AddVector(x368, p->point[0].pos, p->point[1].pos);
        RShiftVector3(x368, 1);

        SubtractVector(&SPY.requestVelX, x368, &SPY.posX);
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
