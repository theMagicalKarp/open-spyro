/* Level0_UpdateActors -- per-frame actor update for level 0 (Artisans Home).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 */

#include "actor.h"
#include "level_actors.h"
#include "vecmath.h"
#include "level_globals.h"
#include "level_id.h"
#include "level_update_actors.h"
#include "level_actor_states.h"
#include "level_update_actors_macros.h"
#include "gamestate.h"
#include "pad_buttons.h"
#include "spyro_state.h"
#include "camera.h"
#include "level_sfx.h"
#include "emit_record.h"
#include "cd_stream.h"
#include "actor_cutscene.h"

/* ANIM_ADVANCE / ENTER_POSE (level_update_actors_macros.h) rate. */
#define ANIM_RATE16 0x10

void Level0_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  int newFrame;

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

    /* Publish this actor's animation flags and the frame delta for the
     * callees. */
    g_nActorAnimDone = actor->animFlags & ACTOR_ANIM_DONE;
    newFrame = actor->animFlags & ACTOR_ANIM_NEW_FRAME;
    g_nActorFrameDelta = g_nFrameStep;
    g_nActorAnimNewFrame = newFrame;

    /* One case per actor class. `break` and `continue` both end this actor's
     * update; `continue` also skips any shared tail inside the case. */
    switch (actor->type) {
    /* Portal name text: shown while the camera faces the portal. */
    case ACTOR_TYPE_PORTAL_NAME: {
      int shouldSpawnText, cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == PORTALNAME_FAR;

        actor->mode = PORTALNAME_NEAR;

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

    /* Path patroller (PathState). */
    case ACTOR_TYPE_PATH_PATROLLER: {
      PathState *st = (PathState *)actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != PATROLLER_KNOCKED) {
        actor->flags = 0; /* cleared first: the FLAME test below never passes */
        st->heading = ArcTan2(actor->posX - SPY.posX,
                                    actor->posY - SPY.posY, ATAN_ANGLE8);

        if ((actor->flags & ACTOR_HIT_FLAME) != 0) {
          st->speed = 0xC8;
        } else {
          st->speed = PATROLLER_KNOCK_SPEED;
        }

        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        actor->mode = PATROLLER_KNOCKED;
        ANIM_RESET(actor, PATROLLER_ANIM_KNOCKED);
        continue;
      }

      switch (actor->mode) {
      case PATROLLER_WAIT: {
        SnapActorToGround(actor);

        if (ApproxDist2D(&actor->posX, &SPY.posX) < PATROLLER_NOTICE_RANGE &&
            ABS2(actor->posZ - SPY.posZ) < PATROLLER_NOTICE_DZ) {
          if (actor->group != 0xFF) {
            Actor **scan = g_apActiveActorList;
            Actor *other;

            while ((other = *scan++) != 0) {
              PathState *ost = (PathState *)other->state;

              if (other->group == actor->group && other->mode == PATROLLER_WAIT) {
                if (ost->path->cur == st->path->cur) {
                  other->mode = PATROLLER_PICK;
                  ost->startDelay = RandomIntInRange(PATROLLER_START_MIN_DELAY, PATROLLER_START_MAX_DELAY);
                }
              }
            }
          }

          actor->mode = PATROLLER_PICK;
          continue;
        }

        if (actor->nextAnim != PATROLLER_ANIM_IDLE) {
          g_nActorAnimDone = 0;
          ANIM_ADVANCE(actor, PATROLLER_ANIM_IDLE);
        }

        break;
      }
      case PATROLLER_PICK: {
        int spyroAngle;
        int angle;
        int angle2;
        int previousAngle;
        int nextNode;
        int previousNode;
        int distance;
        int angleScale;
        int nextAngle;

        spyroAngle =
            ArcTan2(SPY.posX - actor->posX, SPY.posY - actor->posY, ATAN_ANGLE8);
        nextNode = (st->path->cur + 1) % st->path->count;
        previousNode = (st->path->cur - 1 + st->path->count) % st->path->count;

        nextAngle = ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                                  st->path->nodes[nextNode].y - actor->posY, ATAN_ANGLE8),
        previousAngle =
            ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                          st->path->nodes[previousNode].y - actor->posY, ATAN_ANGLE8);

        if (AbsAngleDelta8(spyroAngle, nextAngle) >
            AbsAngleDelta8(spyroAngle, previousAngle)) {
          st->path->cur = nextNode;
        } else {
          st->path->cur = previousNode;
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);
        angle = (angle + RandomIntInRange(-PATROLLER_AIM_JITTER, PATROLLER_AIM_JITTER)) & 0xFF;
        angle2 =
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8);
        angleScale = 108;
        angle2 += (WrapAngleDelta8(angle2, angle) * angleScale) >> 7;
        angle2 &= 0xFF;
        st->heading = angle2;
        st->topSpeed = RandomIntInRange(PATROLLER_MIN_SPEED, PATROLLER_MAX_SPEED);
        st->speed = 0;
        distance = ApproxDist2D(&actor->posX,
                                 (int *)&st->path->nodes[st->path->cur]);
        st->travelTimer = (distance / st->topSpeed) >> 1;

        if (st->travelTimer > PATROLLER_FLEE_MAX_FRAMES) {
          st->travelTimer = PATROLLER_FLEE_MAX_FRAMES;
        }

        actor->mode = PATROLLER_FLEE;
        continue;
      }
      case PATROLLER_FLEE: {
        int distance = ApproxDist2D(&actor->posX,
                                     (int *)&st->path->nodes[st->path->cur]);

        if (TICK_DOWN(st->startDelay) == 0) {
          break;
        }

        if (st->speed < st->topSpeed) {
          st->speed += PATROLLER_ACCEL;
        }

        if (actor->nextAnim != PATROLLER_ANIM_RUN) {
          g_nActorAnimDone = 0;
          ANIM_ADVANCE(actor, PATROLLER_ANIM_RUN);
        }

        if (TICK_DOWN(st->travelTimer) != 0) {
          int angleDelta;
          int turn;

          angleDelta = WrapAngleDelta8(
              ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                            st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8),
              st->heading);
          turn = 1;

          if (ABS2(angleDelta) >= 0x1F) {
            turn = 2;
          }

          if (angleDelta > 0) {
            st->heading -= turn;
            st->heading &= 0xFF;
          } else if (angleDelta < 0) {
            st->heading += turn;
            st->heading &= 0xFF;
          }
        }

        if (TurnActorYawTowardAngle(actor, st->heading, 6, 0x14, YAW_STEP_IN_BAND) != 0) {
          if (MoveActorAlongHeading(actor, st->speed, 0x12C, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT) != 0) {
            st->blockedCount++;
          } else {
            st->blockedCount = 0;
          }
          if (st->blockedCount >= 4 && g_nActorAnimDone != 0) {
            if (distance < PATROLLER_NOTICE_RANGE) {
              actor->mode = PATROLLER_WAIT;
              continue;
            }
            st->heading = WrapAddAngle8(st->heading, RandomSignedInRange(PATROLLER_BLOCKED_VEER_MIN, PATROLLER_BLOCKED_VEER_MAX));
          }
        }
        if (distance < PATROLLER_ARRIVE_RANGE && g_nActorAnimDone != 0) {
          actor->mode = PATROLLER_WAIT;
          continue;
        }

        break;
      }

      case PATROLLER_KNOCKED: {
        if (st->speed > PATROLLER_KNOCK_DRAG) {
          st->speed -= PATROLLER_KNOCK_DRAG;
          MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0x2BC, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        }

        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }

        break;
      }
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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

    /* Pressure switch (SwitchState). */
    case ACTOR_TYPE_PRESSURE_SWITCH: {
      SwitchState *st = (SwitchState *)actor->state;
      int active = g_anLevelKillBitmapTable[0x78] & 1;

      switch (actor->subMode) {
      case PSWITCH_CLEAR: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x500 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x280) {
          if ((g_pActorListBase[st->idx].subMode >> st->shift) & 1) {
            g_pActorListBase[st->idx].subMode = 0;
          } else {
            g_pActorListBase[st->idx].subMode |= 1 << st->shift;
          }
          actor->subMode = PSWITCH_STOOD_ON;
        }
        break;
      }

      case PSWITCH_STOOD_ON: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x601 ||
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) >= 0x2C1) {
          actor->subMode = PSWITCH_CLEAR;
        }
        break;
      }
      }

      if (g_pActorListBase[st->idx].mode == SEQPROP_RUN) {
        if ((actor->mode & 1) == 0) {
          actor->mode = PSWITCH_DOWN;
          ANIM_RESET(actor, PSWITCH_DOWN);
        }
        break;
      }

      if (g_pActorListBase[st->idx].mode == SEQPROP_RUN_ALT) {
        if ((actor->mode & 1) != 0) {
          actor->mode = PSWITCH_UP;
          ANIM_RESET(actor, PSWITCH_UP);
        }
        break;
      }

      if (((g_pActorListBase[st->idx].subMode >> st->shift) & 1) != 0 && active != 0 &&
          g_pActorListBase[st->idx].mode < SEQPROP_DONE) {
        if ((actor->mode & 1) == 0) {
          g_nSfxOverrideFlags = SFX_OVERRIDE_PITCH;
          g_nSfxOverridePitch.v =
              SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]) +
              g_anSwitchSfxPitchOffsets[st->shift];
          PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[PSWITCH_VOICE_PRESS], actor, SFX_MODE_3D,
                        &actor->voiceRow);
        }
        actor->mode = PSWITCH_DOWN;
        ANIM_RESET(actor, PSWITCH_DOWN);
      } else if ((actor->mode & 1) != 0) {
        actor->mode = PSWITCH_UP;
        ANIM_RESET(actor, PSWITCH_UP);
      }
      break;
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
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
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
          CopyVector(svA, &SPY.posX);
          {
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int s = LookupSine(SPY.bodyRotZ << 4);
            int cr = c * EGG_APEX_DIAG;
            int sr = s * EGG_APEX_DIAG;
            svA[0] = svA[0] + ((sr + cr) >> 12);
          }
          {
            int s = LookupSine(SPY.bodyRotZ << 4);
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int sr = s * EGG_APEX_DIAG;
            int cr = c * EGG_APEX_DIAG;
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
          CopyVector(svD, &SPY.posX);
          svD[0] = svD[0] + (LookupCosine(SPY.bodyRotZ << 4) >> 3);
          svD[1] = svD[1] + (LookupSine(SPY.bodyRotZ << 4) >> 3);
          svD[2] = svD[2] + EGG_APEX_HEIGHT;
          VectorToSVectorShr2(st->centre, svD);
          st->angle = SPY.bodyRotZ - 0x40;
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

    /* Sequenced prop gated on the exit vortices (AnimState). */
    case ACTOR_TYPE_EXIT_GATED_PROP: {
      AnimState *st = actor->state;
      int marker;

      if (st->started == 0) {
        if (g_abLevelVisitedFlag[LEVEL_SLOT(LEVEL_TOASTY)] != 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          SetWorldChunkAnimStep(st->chunkAnim, 0x3B, 0);
        }
        st->started = 1;
      }

      marker = CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim));

      switch (marker) {
      case 0:
        if (g_abLevelExited[LEVEL_SLOT(LEVEL_STONE_HILL)] || g_abLevelExited[LEVEL_SLOT(LEVEL_DARK_HOLLOW)] ||
            g_abLevelExited[LEVEL_SLOT(LEVEL_TOWN_SQUARE)] || g_abLevelExited[LEVEL_SLOT(LEVEL_TOASTY)]) {
          /* Wake only once the linked actor is dormant and Spyro is near. */
          if (g_pActorListBase[st->idx].mode >= ACTOR_MODE_DORMANT &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            actor->soundRange = 0x30;
            PlaySoundEffect(g_pLevelSampleBankHeader[0x41], actor, SFX_MODE_3D, &actor->voiceRow);
          }
        }
        break;

      case 0x3C:
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x4801) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        break;
      }
      break;
    }

    /* Bouncing chest fragments (ChestShardState). */
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
        svA[2] += GROUND_PROBE_LIFT;
        FindGroundHeightBelow(svA, GROUND_PROBE_FAR);
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
            svF[2] += GROUND_PROBE_LIFT;

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
          bouncePos[2] += COLLECT_RADIUS;

          if (CollideSphereWithWorldAndActors(bouncePos, COLLECT_RADIUS, COLLECT_RADIUS)) {
            if (GetGroundTriggerRegionType() == 0) {
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
            CopyVector(&actor->posX, bouncePos);
            FindGroundHeightBelow(bouncePos, GROUND_PROBE_FAR);
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

          SubtractVector(homingStep, &SPY.posX, st->vel);
          RShiftVector3(homingStep, 5);

          if (VectorLength(homingStep, VEC_LEN_XYZ) > COLLECT_HOMING_SNAP_STEP) {
            CopyVector(&actor->posX, &SPY.posX);
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

          SubtractVector(svF, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (svF[0] * cosDelta) / totalWeight;
          actor->posY += ((svF[1] * cosDelta) / totalWeight);
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

    /* Path-following enemy (WalkerState). */
    case ACTOR_TYPE_PATH_ENEMY: {
      WalkerState *st = (WalkerState *)actor->state;
      int distance;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 &&
          (unsigned int)(actor->mode - PENEMY_FLAMED) >= 3) { /* not FLAMED, CHARGED or FALL */
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);

        if ((actor->flags & ACTOR_HIT_CHARGE) != 0) {
          st->moveSpeed = PENEMY_CHARGE_SPEED;
          st->zSpeed = PENEMY_CHARGE_VZ;
          st->heading = StepAndLerpAngle8(
              ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8),
              SPY.bodyRotZ, 0x28, 0x80);
          ENTER_POSE(actor, PENEMY_CHARGED);
        }

        PlaySoundEffect(g_pLevelSampleBankHeader[0x27], actor, SFX_MODE_3D, &actor->voiceRow);
        st->zSpeed = PENEMY_FLAME_VZ;
        st->moveSpeed = PENEMY_FLAME_SPEED;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8),
            SPY.bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        ENTER_POSE(actor, PENEMY_FLAMED);
      }

      if (st->tgtIdx >= 0 && g_pActorListBase[st->tgtIdx].mode >= ACTOR_MODE_DORMANT) {
        st->tgtIdx = -1;
      }

      switch (actor->mode) {
      case PENEMY_IDLE: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < PENEMY_QUICK_RANGE &&
            ApproxDist2D(&actor->posX, &SPY.posX) < PENEMY_SPOT_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < PENEMY_SPOT_DZ) {
          ENTER_POSE(actor, PENEMY_SPOT);
        }

        if (st->tgtIdx >= 0 && TICK_DOWN(st->timer) != 0) {
          if (ApproxDist2D(&actor->posX, &g_pActorListBase[st->tgtIdx].posX) <
              PENEMY_PARTNER_RANGE) {
            ENTER_POSE(actor, PENEMY_FACE_PARTNER);
          }

          ENTER_POSE(actor, PENEMY_FIDGET);
        }

        SnapActorToGround(actor);
        break;
      }
      case PENEMY_FIDGET: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = g_pActorListBase[st->tgtIdx].posX - actor->posX;
        deltaY = g_pActorListBase[st->tgtIdx].posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = g_pActorListBase[st->tgtIdx].posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;

        if (distance < PENEMY_PARTNER_QUICK &&
            ApproxDist2D(&actor->posX, &g_pActorListBase[st->tgtIdx].posX) <
                PENEMY_PARTNER_RANGE) {
          ENTER_POSE(actor, PENEMY_FACE_PARTNER);
        }

        if (g_nActorAnimDone != 0) {
          st->timer = (GetRandomU32() & 0x3F) + 0x20;
          ENTER_POSE(actor, PENEMY_IDLE);
        }

        SnapActorToGround(actor);
        break;
      }

      case PENEMY_FACE_PARTNER: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < PENEMY_QUICK_RANGE &&
            ApproxDist2D(&actor->posX, &SPY.posX) < PENEMY_SPOT_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < PENEMY_SPOT_DZ) {
          ENTER_POSE(actor, PENEMY_SPOT);
        }

        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->tgtIdx].posX - actor->posX,
                                    g_pActorListBase[st->tgtIdx].posY - actor->posY,
                                    ATAN_ANGLE8),
                      8, 0x10, YAW_STEP_IN_BAND);

        if (g_nActorAnimDone != 0) {
          st->timer = (GetRandomU32() & 0x3F) + 0x20;
          ENTER_POSE(actor, PENEMY_IDLE);
        }

        SnapActorToGround(actor);
        break;
      }

      case PENEMY_RETURN: {
        int angle;
        int angleDelta;
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < PENEMY_QUICK_RANGE &&
            ApproxDist2D(&actor->posX, &SPY.posX) < PENEMY_SPOT_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < PENEMY_SPOT_DZ) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          }
          ENTER_POSE(actor, PENEMY_SPOT);
        }

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < PENEMY_NODE_ARRIVE) {
          if (st->path->cur == 0) {
            ENTER_POSE(actor, PENEMY_IDLE);
          }
          st->path->cur--;
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);
        angleDelta = (angle - actor->rotZ) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < PENEMY_WALK_CONE) {
          MoveActorAlongHeading(actor, PENEMY_RETURN_SPEED, 0x200, 0x200, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        }

        if (angleDelta < -PENEMY_TURN_STEP) {
          angleDelta = -PENEMY_TURN_STEP;
        }
        if (angleDelta > PENEMY_TURN_STEP) {
          angleDelta = PENEMY_TURN_STEP;
        }

        actor->rotZ += angleDelta;
        break;
      }

      case PENEMY_SPOT: {
        if (g_nActorAnimDone != 0) {
          if (SPY.state == SPYRO_ST_CHARGE || SPY.state == SPYRO_ST_CHARGE_AIR) {
            if (actor->anim != PENEMY_HIDE) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, PENEMY_HIDE);
            }
            actor->mode = PENEMY_COWER;
            continue;
          }

          ENTER_POSE(actor, PENEMY_FLEE);
        }

        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        break;
      }

      case PENEMY_FLEE: {
        int angle;
        int angleDelta;

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < PENEMY_NODE_ARRIVE) {
          if (st->path->cur < st->path->count - 1) {
            st->path->cur++;
          } else {
            st->timer = PENEMY_HIDE_FRAMES;
            ENTER_POSE(actor, PENEMY_HIDE);
          }
        }

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);
        angleDelta = (angle - actor->rotZ) & 0xFF;
        if (angleDelta > 0x80) {
          angleDelta -= 0x100;
        }

        if (ABS2(angleDelta) < PENEMY_WALK_CONE) {
          MoveActorAlongHeading(actor, PENEMY_FLEE_SPEED, 0x200, 0x200, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        }

        if (angleDelta < -PENEMY_TURN_STEP) {
          angleDelta = -PENEMY_TURN_STEP;
        }
        if (angleDelta > PENEMY_TURN_STEP) {
          angleDelta = PENEMY_TURN_STEP;
        }

        actor->rotZ += angleDelta;
        break;
      }

      case PENEMY_HIDE: {
        int xyDistance;
        int deltaY;
        int deltaZ;

        xyDistance = SPY.posX - actor->posX;
        deltaY = SPY.posY - actor->posY;

        xyDistance = ABS(xyDistance);
        xyDistance += ABS(deltaY);
        deltaZ = SPY.posZ - actor->posZ;
        deltaZ = ABS(deltaZ);
        distance = xyDistance + deltaZ;
        if (distance < PENEMY_QUICK_RANGE &&
            ApproxDist2D(&actor->posX, &SPY.posX) < PENEMY_SPOT_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < PENEMY_SPOT_DZ) {
          TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
          st->timer = PENEMY_HIDE_FRAMES;
          break;
        }

        if (TICK_DOWN(st->timer) != 0) {
          st->path->cur = st->path->count - 1;
          ENTER_POSE(actor, PENEMY_RETURN);
        }

        break;
      }
      case PENEMY_FLAMED: {
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, PENEMY_FALL);
        }

        actor->rotY += 4;
        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, YAW_STEP_IN_BAND);
        DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC, 0xC);
        break;
      }

      case PENEMY_CHARGED: {
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x1C);
          DespawnActorRecord(actor);
          continue;
        }

        DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC, 0xC);
        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, YAW_STEP_IN_BAND);
        break;
      }

      case PENEMY_FALL: {
        if (DriveActorTrackedMove(actor, &st->moveSpeed, st->heading, &st->zSpeed, 0xC,
                          0x10) == MOVED_LANDED) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }

        TurnActorYawTowardAngle(actor, (st->heading + 0x80) & 0xFF, 0x10, 0x20, YAW_STEP_IN_BAND);
        break;
      }

      case PENEMY_COWER: {
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);

        if (SPY.state != SPYRO_ST_CHARGE && SPY.state != SPYRO_ST_CHARGE_AIR) {
          ENTER_POSE(actor, PENEMY_FLEE);
        }

        break;
      }

      default: {
        break;
      }
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Sparx (SparxState). */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = (SparxState *)actor->state;
      char pitchAngle;

      if (SPY.sparxHealth >= SPARX_BLUE) {

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

          if (CollideSphereWithWorldAndActors(&actor->posX, SPARX_COLLIDE_RADIUS, SPARX_COLLIDE_RADIUS)) {
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

      if (ApproxDist2D(&actor->posX, &SPY.posX) < KEY_TOUCH_RANGE) {
        dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
        if (dz > 0 ? dz < KEY_TOUCH_DZ
                   : SPY.posZ - (actor->posZ - actor->floorOffset) < KEY_TOUCH_DZ) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          for (i = 0; i < 6; i++) {
            g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (0x8080 + 0x1000000 * i));
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

    /* Talking dragon: when Spyro faces it up close, start its dialogue. */
    case ACTOR_TYPE_HOME_TALKER: {
      if (actor->mode == TALKER_REARM) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= TALKER_REARM_RANGE) {
          actor->mode = TALKER_READY;
        }
      } else if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
                  SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_CHARGE) &&
                 ApproxDist2D(&actor->posX, &SPY.posX) < TALKER_TALK_RANGE &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - SPY.posX,
                                             actor->posY - SPY.posY, ATAN_ANGLE8)) <
                     TALKER_SPYRO_FACING_ANGLE &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, ATAN_ANGLE8)) <
                     TALKER_FACING_ANGLE) {
        actor->mode = TALKER_REARM;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ResetSpyroLinearMotion();
        ZeroVector(&SPY.headLook);
        g_pfnLevelBeginActorCutscene(actor);

        if (g_nTotalDragonsRescued < 10) {
          if (g_abWorldBalloonistState[1] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(0, BPAGE_LOCKED_FIRST);
            g_abWorldBalloonistState[1] = BALLOONIST_TALKED;
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(0, BPAGE_LOCKED_AGAIN);
          }
        } else if (g_abWorldBalloonistState[1] < BALLOONIST_OPEN) {
          if (g_abWorldBalloonistState[1] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(0, BPAGE_UNLOCK_FIRST);
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(0, BPAGE_UNLOCK_AGAIN);
          }
          g_abWorldBalloonistState[1] = BALLOONIST_OPEN;
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

    /* Crystal dragon statue (DragonStatueState). */
    case ACTOR_TYPE_DRAGON_STATUE: {
      DragonStatueState *st = (DragonStatueState *)actor->state;

      if (actor->mode == STATUE_INIT) {
        int padIdx = st->partner;
        if (padIdx != -1) {
          g_pActorListBase[padIdx].mode = CHECKPT_HELD;
        }
        actor->mode = STATUE_IDLE;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
      } else if (actor->mode == STATUE_IDLE) {
        st->phase += g_nFrameStep;
        if (st->phase > STATUE_CYCLE_TICKS) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if (st->phase >= STATUE_SHAKE_TICK) {

          if (!GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[STATUE_VOICE_RATTLE])) {
            PlayActorMeshVoice(actor, STATUE_VOICE_RATTLE, 0);
          }
          actor->rotX =
              (signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].a + st->rotX;
          actor->rotY =
              (signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(st->phase - STATUE_SHAKE_TICK) >> 1].b)) *
                  6;
        } else if (st->phase >= STATUE_SETTLE_TICK) {
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = STATUE_SETTLE_TICK;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else {
          if ((st->phase >> 1) == 16 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
          } else if ((st->phase >> 1) == 48 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
          } else if ((st->phase >> 1) == 80 && (GetRandomU32() & 3) == 0) {
            SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) < STATUE_NEAR_RANGE) {

          SubtractVector(svK, &actor->posX, &SPY.posX);
          svK[2] = (svK[2] * 3) >> 2;
          if (VectorLength(svK, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
            int rotZ;
            int cutsceneId;
            if (st->partner != -1) {
              rotZ = g_pActorListBase[st->partner].rotZ;
            } else {
              rotZ = st->facing;
            }
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, rotZ);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIndex]++;
              g_nTotalDragonsRescued++;
              StubRewardHook(st->reward, 0);
              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = CHECKPT_ARMED;
              }
              DespawnActorRecord(actor);
              continue;
            } else if (cutsceneId = st->cutsceneId, cutsceneId == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIndex]++;
              g_nTotalDragonsRescued++;
              if (st->partner != cutsceneId) {
                g_pActorListBase[st->partner].mode = CHECKPT_ARMED;
              }
              DespawnActorRecord(actor);
              continue;
            } else {
              actor->mode = STATUE_RESCUING;
              actor->rotX = st->rotX;
              actor->rotY = st->rotY;
              actor->posZ = st->restZ;
              BeginDragonRescue(actor);
              ZeroVector(&SPY.headLook);
              SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT);
              if (SPY.airborneFrames != 0) {
                SPY.requestState = SPYRO_ST_FALL;
                SPY.requestVelX = SPY.motionX >> 8;
                SPY.requestVelY = SPY.motionY >> 8;
                if (SPY.motionZ > 0) {
                  SPY.requestVelZ = 0;
                } else {
                  SPY.requestVelZ = SPY.motionZ >> 6;
                }
              } else {
                SPY.requestState = SPYRO_ST_STOP;
                CopyVector(&SPY.requestVelX, &SPY.motionX);
                RShiftVector3(&SPY.requestVelX, 6);
                SPY.requestVelZ = 0;
                if (SPY.requestVelX != 0 || SPY.requestVelY != 0) {
                  ScaleVector3ByRatio(&SPY.requestVelX, VectorLength(&SPY.requestVelX, VEC_LEN_XY), 0x60);
                }
                SPY.requestVelZ = 0;
              }
            }
          }
        }
      } else {
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
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

    /* Ambient sound source (AmbientSoundBytesState). */
    case ACTOR_TYPE_AMBIENT_SOUND: {
      AmbientSoundBytesState *st = actor->state;
      int snd;

      actor->soundRange = st->soundRange;
      snd = -1;

      switch (st->mode) {
      case 0:
        snd = 6;
        break;

      case 1:
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          snd = (GetRandomU32() & 7) + 7;
        }
        break;

      case AMBIENT_MODE_PATH:
        switch (st->sub) {
        case AMBIENT_STEP_WAIT:
          if (TICK_DOWN(st->timer) != 0) {
            st->timer = 0x200 - (GetRandomU32() & 0x7F);
            snd = (GetRandomU32() & 1) + 0xD;
            st->sub = AMBIENT_STEP_TRAVEL;
          }
          break;

        case AMBIENT_STEP_TRAVEL:
          if (st->count-- > 0) {
            /* Coasting along the current leg. */
            actor->posX += st->vel[0];
            actor->posY += st->vel[1];
            actor->posZ += st->vel[2];
          } else {
            /* Leg done: snap to the next node, then start the next leg. */
            actor->posX = PATH_X(st->path, (st->path[1] + 1) % st->path[0]);
            actor->posY = PATH_Y(st->path, (st->path[1] + 1) % st->path[0]);
            actor->posZ = PATH_Z(st->path, (st->path[1] + 1) % st->path[0]);
            st->sub = AMBIENT_STEP_WAIT;
            st->path[1] = (st->path[1] + 1) % st->path[0];
            {
              int offA = (st->path[1] << 4) + 8;
              int offB = (((st->path[1] + 1) % st->path[0]) << 4) + 8;

              st->count =
                  ComputeStepTowardPoint((int *)(st->path + offA),
                                (int *)(st->path + offB), st->vel, st->speed);
            }
          }

          if (actor->voiceRow == VOICE_ROW_NONE && st->count % 48 == 0) {
            snd = 0xE;
          }
          break;
        }
        break;

      case 3:
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = 0x270 - (GetRandomU32() & 0x7F);
          snd = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TICK_DOWN(st->timer) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          snd = (int)GetRandomU32() % 4 + 0x15;
        }
        break;

      case 5:
        snd = 0x18;
        break;

      case 6:
        snd = 0x19;
        break;

      case 7:
        snd = 0x12;
        break;

      case AMBIENT_MODE_PORTAL:
        snd = SFX_PORTAL_HUM;
        break;

      case AMBIENT_MODE_WHIRLWIND:
        snd = SFX_WHIRLWIND;
        break;
      }

      if (snd < 0) {
        break;
      }
      PlaySoundEffect(g_pLevelSampleBankHeader[snd], actor, SFX_MODE_3D, &actor->voiceRow);
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
      SubtractVector(svK, &SPY.posX, &actor->posX);
      distance = VectorLength(svK, VEC_LEN_XY);
      if (distance < st->radius && svK[2] >= -WHIRL_BELOW_REACH && svK[2] < st->spanZ &&
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

    /* Respawner (RespawnState). */
    case ACTOR_TYPE_RESPAWNER: {
      RespawnState *st = actor->state;
      Actor *p;

      CopyVector(&actor->posX, &SPY.posX);
      if (TICK_DOWN(st->timer) == 0) {
        break;
      }
      st->timer = st->period;

      for (p = g_pActorListBase; p < g_pActorPoolDynBase; p++) {
        if (p->type != st->type) {
          continue;
        }
        if (p->mode < ACTOR_MODE_DORMANT) {
          continue;
        }
        if (ApproxDist2D(&SPY.posX, p->state) <= RESPAWN_RANGE) {
          continue;
        }

        CopyVector(&p->posX, p->state);
        InsertActorIntoSpatialGrid(p);
        p->killGroup &= ACTOR_KILL_GROUP_MASK;
        p->flags = 0;
        p->mode = ACTOR_SPAWN_MODE;
        p->frameProgress = 0;
        p->frameRate = TYPE_DESC(p, ACTOR_SPAWN_ANIM);
        p->anim = ACTOR_SPAWN_ANIM;
        p->nextAnim = ACTOR_SPAWN_ANIM;
        p->frame = 0;
        p->nextFrame = 1;
      }
      break;
    }

    /* Checkpoint: Spyro reaching it sets the respawn anchor
     * (SetSpawnAnchorFromActor); it then plays its anim for 0x30 frames. */
    case ACTOR_TYPE_CHECKPOINT: {
      switch (actor->mode) {
      case CHECKPT_REARM: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) > CHECKPT_REARM_RANGE) {
          actor->mode = CHECKPT_ARMED;
        }
        break;
      }

      case CHECKPT_ARMED: {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < CHECKPT_TOUCH_RANGE &&
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < CHECKPT_TOUCH_DZ) {

          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = CHECKPT_FLASH;
          actor->anim = CHECKPT_ANIM_FLASH;
          actor->subMode = 0;
        }
        break;
      }

      case CHECKPT_FLASH: {
        actor->subMode += g_nFrameStep;

        if (actor->subMode >= CHECKPT_FLASH_FRAMES) {
          actor->mode = CHECKPT_REARM;
          actor->anim = CHECKPT_ANIM_REST;
        }
        break;
      }
      }

      break;
    }

    /* Animated scenery: re-randomised speed; plays its hit animation when
     * damaged. */
    case ACTOR_TYPE_ANIMATED_SCENERY:
    case ACTOR_TYPE_ANIMATED_SCENERY_B:
      if (g_nActorAnimDone != 0) {
        /* Base speed comes from the animation that is playing. */
        int poseBase = TYPE_DESC(actor, actor->nextAnim);

        actor->frameRate = poseBase + RandomIntInRange(-4, 4);
      }

      if ((actor->flags & ACTOR_HIT_FLAME) != 0 && actor->nextAnim == SCENERY_ANIM_IDLE) {
        if (actor->anim != SCENERY_ANIM_HIT) {
          g_nActorAnimDone = 0;
          ANIM_RESET(actor, SCENERY_ANIM_HIT);
        }
        break;
      }

      if (actor->nextAnim != SCENERY_ANIM_HIT) {
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);

      if (g_nActorAnimDone != 0 && actor->anim != SCENERY_ANIM_AFTER_HIT) {
        g_nActorAnimDone = 0;
        ANIM_RESET(actor, SCENERY_ANIM_AFTER_HIT);
      }
      break;

    /* Flock leader (FlockState). */
    case ACTOR_TYPE_FLOCK_LEADER: {
      FlockState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode < FLOCK_BOLT) {
        while (st->chainA >= 0 && g_pActorListBase[st->chainA].mode >= ACTOR_MODE_DORMANT) {
          st->chainA = *(int *)g_pActorListBase[st->chainA].state;
        }

        if (st->chainA >= 0) {
          Actor *p = &g_pActorListBase[st->chainA];
          int *link = p->state;

          CopyVector(&p->posX, &actor->posX);
          SpawnActorDeathDrop(p, DROP_SCATTER, 0, 0);
          MarkActorLiveKill(p);
          st->chainA = *link;
          DespawnActorRecord(p);
          actor->flags = 0;
          ENTER_POSE(actor, FLOCK_BOLT);
        } else {
          int n = 0;

          while (st->chainB >= 0) {
            Actor *p = &g_pActorListBase[st->chainB];
            int *link = p->state;

            if (p->mode < ACTOR_MODE_DORMANT) {
              CopyVector(&p->posX, &actor->posX);
              st->slots[n][0] = actor->posX + (LookupCosine(n * FLOCK_PRIZE_ANGLE_STEP) >> 2);
              st->slots[n][1] = actor->posY + (LookupSine(n * FLOCK_PRIZE_ANGLE_STEP) >> 2);
              st->slots[n][2] = actor->posZ;
              SpawnActorDeathDrop(p, DROP_AT_POINT, 0, st->slots[n]);
              n++;
              MarkActorLiveKill(p);
              DespawnActorRecord(p);
            }

            st->chainB = *link;
          }

          st->slots[n][0] = actor->posX + (LookupCosine(n * FLOCK_PRIZE_ANGLE_STEP) >> 2);
          st->slots[n][1] = actor->posY + (LookupSine(n * FLOCK_PRIZE_ANGLE_STEP) >> 2);
          st->slots[n][2] = actor->posZ;
          SpawnActorDeathDrop(actor, DROP_AT_POINT, 0, st->slots[n]);
          MarkActorLiveKill(actor);
          st->knockSpeed = FLOCK_KNOCK_SPEED;
          st->knockHeading = StepAndLerpAngle8(
              ArcTan2(actor->posX - SPY.posX, actor->posY - SPY.posY, ATAN_ANGLE8),
              SPY.bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
          actor->mode = FLOCK_KNOCKED;
          ANIM_RESET(actor, FLOCK_KNOCKED);
          continue;
        }
      }

      switch (actor->mode) {
      case FLOCK_IDLE: {
        int distance = ApproxDist2D(&actor->posX, &SPY.posX);

        if (distance < FLOCK_ROUSE_RANGE &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < FLOCK_ROUSE_DZ) {
          ENTER_POSE(actor, FLOCK_ROUSE);
        }

        if (distance < FLOCK_WATCH_RANGE) {
          TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);
        }
        break;
      }

      case FLOCK_ROUSE: {
        TrackEntityYawTowardSpyro(actor, 8, 0x10, YAW_STEP_IN_BAND);

        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, FLOCK_RUN);
        }
        break;
      }

      case FLOCK_RUN:
      case FLOCK_RUN_ALT:
      case FLOCK_BOLT: {
        int spyroAngle;
        int angle;
        int previousAngle;
        int nextNode;
        int previousNode;
        int nextAngle;

        if (actor->mode == FLOCK_BOLT) {
          actor->flags = 0;
        }

        if (ApproxDist2D(&actor->posX,
                          (int *)&st->path->nodes[st->path->cur]) < FLOCK_NODE_RANGE) {
          if (ApproxDist2D(&actor->posX, &SPY.posX) > FLOCK_ROUSE_RANGE ||
              ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) > FLOCK_LOSE_DZ) {
            ENTER_POSE(actor, FLOCK_IDLE);
          } else {
            spyroAngle =
                ArcTan2(SPY.posX - actor->posX, SPY.posY - actor->posY, ATAN_ANGLE8);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;

            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, ATAN_ANGLE8),
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

        angle = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                              st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);
        previousAngle = (angle - actor->rotZ) & 0xFF;
        if (previousAngle > 0x80) {
          previousAngle -= 0x100;
        }
        nextAngle = previousAngle;

        if (previousAngle > FLOCK_TURN_STEP) {
          previousAngle = FLOCK_TURN_STEP;
        }
        if (previousAngle < -FLOCK_TURN_STEP) {
          previousAngle = -FLOCK_TURN_STEP;
        }
        actor->rotZ += previousAngle;

        if (ABS2(nextAngle) < FLOCK_MOVE_FACING) {
          if (actor->mode == FLOCK_BOLT) {
            MoveActorAlongHeading(actor, FLOCK_BOLT_SPEED, FLOCK_MOVE_RADIUS, FLOCK_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
          } else {
            MoveActorAlongHeading(actor, FLOCK_RUN_SPEED, FLOCK_MOVE_RADIUS, FLOCK_MOVE_RADIUS, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
          }
        }

        if (g_nActorAnimDone != 0) {
          if (actor->mode == FLOCK_RUN) {
            actor->mode = FLOCK_RUN_ALT;
            ANIM_RESET(actor, FLOCK_RUN_ALT);
          } else {
            actor->mode = FLOCK_RUN;
            if (actor->nextAnim != FLOCK_RUN) {
              ANIM_ADVANCE(actor, FLOCK_RUN);
            }
          }
          continue;
        }
        break;
      }
      case FLOCK_KNOCKED: {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);

        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, FLOCK_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }

        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, 0, FLOCK_KNOCK_DRAG, 0);
        break;
      }
      }
      break;
    }

    /* Sequenced prop (PropAnimState). */
    case ACTOR_TYPE_SEQUENCED_PROP: {
      PropAnimState *st = actor->state;

      /* First tick: if already unlocked, skip to the finished state. */
      if (st->primed == 0) {
        if (g_abLevelVisitedFlag[LEVEL_SLOT(LEVEL_SUNNY_FLIGHT)] != 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          SetWorldChunkAnimStep(st->chunkAnim, 0x77, 0);
          actor->mode = SEQPROP_DONE;
        }
        st->primed = 1;
      }

      switch (actor->mode) {
      case SEQPROP_WAIT:
        if (actor->subMode == 0x1F) {
          actor->mode = SEQPROP_START;
        }
        break;

      case SEQPROP_START:
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        actor->mode = SEQPROP_RUN;
        st->timer = 0x1E;
        actor->soundRange = 0x30;
        PlaySoundEffect(g_pLevelSampleBankHeader[0x41], actor, SFX_MODE_3D, &actor->voiceRow);
        break;

      case SEQPROP_RUN:
        if ((GetWorldChunkAnimState(st->chunkAnim) & CHUNK_ANIM_HALTED) != 0) {
          actor->mode = SEQPROP_DONE;
          break;
        }
        if (TICK_DOWN(st->timer) == 0) {
          break;
        }
        actor->mode = SEQPROP_RUN_ALT;
        st->timer = 0x1E;
        break;

      case SEQPROP_RUN_ALT:
        if ((GetWorldChunkAnimState(st->chunkAnim) & CHUNK_ANIM_HALTED) != 0) {
          actor->mode = SEQPROP_DONE;
          break;
        }
        if (TICK_DOWN(st->timer) == 0) {
          break;
        }
        actor->mode = SEQPROP_RUN;
        st->timer = 0x1E;
        break;

      case SEQPROP_DONE:
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        break;
      }
      break;
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

    /* Collision-triangle rider (PatchRideState). */
    case ACTOR_TYPE_PATCH_RIDER: {
      PatchRideState *st = actor->state;
      int tri[3][3];

      if (st->triId < 0) {
        /* Unattached: probe just ahead and attach to that triangle. */
        tri[0][0] = g_anCosineLut[actor->rotZ] >> 1;
        tri[0][1] = g_anSineLut[actor->rotZ] >> 1;
        tri[0][2] = 0;
        AddVector(tri[0], tri[0], &actor->posX);

        if (CastRayWorldAndActors(tri[0], &actor->posX) == 0) {
          break;
        }

        st->triId = g_nCollisionHitTriIndex;
        UnpackWorldCollisionTri(st->triId, tri[0]);
        AddVector(tri[0], tri[0], tri[1]);
        AddVector(tri[0], tri[0], tri[2]);
        tri[0][0] /= 3;
        tri[0][1] /= 3;
        tri[0][2] /= 3;
        SubtractVector(st->offset, &actor->posX, tri[0]);
        break;
      }

      UnpackWorldCollisionTri(st->triId, tri[0]);
      AddVector(tri[0], tri[0], tri[1]);
      AddVector(tri[0], tri[0], tri[2]);
      tri[0][0] /= 3;
      tri[0][1] /= 3;
      tri[0][2] /= 3;
      AddVector(&actor->posX, tri[0], st->offset);

      if ((g_dwGamestateFrames & 3) != 0) {
        break;
      }
      tri[0][0] = g_anCosineLut[actor->rotZ] >> 5;
      tri[0][1] = g_anSineLut[actor->rotZ] >> 5;
      tri[0][2] = 0x20;
      g_pfnLevelEmitParticles(1, EMIT_SLOWING_SPARK, &actor->posX, (int)tri[0]);
      break;
    }

    /* Portal guide (PortalState). */
    case ACTOR_TYPE_PORTAL_GUIDE: {
      PortalState *st = actor->state;
      PortalCamPath *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case PORTALGUIDE_ENTER: {
        int vec[3];

        SPY.requestMask =
            (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_NO_CLIP |
             SPYRO_RQ_FORCE_STATE | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_VEL_Z |
             SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_X);
        SPY.camPathReverse = actor->subMode;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = 0x60;
        SPY.requestState = SPYRO_ST_JUMP;
        SPY.otBinBias = 0x7F;

        p = st->desc;
        AddVector(vec, p->point[0].pos, p->point[1].pos);
        RShiftVector3(vec, 1);

        SubtractVector(&SPY.requestVelX, vec, &SPY.posX);
        AddVector(&SPY.requestVelX, &SPY.requestVelX, st->offset);

        mag = VectorLength(&SPY.requestVelX, VEC_LEN_XYZ);
        if (mag > 128) {
          ScaleVector3ByRatio(&SPY.requestVelX, mag, 128);
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

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
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
        SPY.requestMask = SPYRO_RQ_PENDING | 0x4000 | 0x2000 | 0x100;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = 0x60;
        SPY.requestState = SPYRO_ST_GLIDE;
        SPY.otBinBias = 0x7F;

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
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
        ((CollectibleState *)prize->state)->mode = COLLREST_SNAP;
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
        int vec[3];

        vec[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        vec[1] = (g_anSineLut[i * 32] >> 8) * 3;
        vec[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)vec);
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

    /* Floating-word letters (NameLetterState). */
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
        if (ApproxDist2D(&st->sign->posX, &SPY.posX) < LEVELEXIT_ENTER_RANGE) {
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
          anglespyro = ArcTan2(SPY.posX - st->sign->posX,
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

    default: /* 77..82, 88..109, 258..259 and everything unlisted */
      break;
    }
  }
}
