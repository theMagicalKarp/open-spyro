/* Level19_UpdateActors -- per-frame actor update for level 19 (Terrace
 * Village). 0x8007B5DC, 36,992 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 3, 7,
 * 8, 9, 10, 15, 25, 31 and the level-18 workspace); the level-19 classes are
 * 32 (an egg carried by its thief), 35 (a guard that scatters when hit and
 * fires bolts), 343 (a bead streaming between two attach points), 467 (a
 * bolt-throwing sentry with three guards) and 494 (the bolt).
 *
 * Load-bearing source forms:
 *   - Level 8's loop top (fp = `one`, s7 = `spy`); the frame is laid out in
 *     the original slot order (floorProbe and the o256..o528 slots, shared by arms).
 *   - The `4` hoist group must NOT move: the three pose-4 sites in 466/467
 *     use the split-constant ENTER_POSE_B forms (cookbook A285).
 *   - Case 35 keeps a counter per loop; case 21 indexes g_pActorListBase directly
 *     and reads ->state before the liveness test.
 *   - Case 343's -42 is written `-(x * 10) - x * 32`; case 494's camera nudge
 *     reads the sine tables as unsigned shorts.
 *   - Case 110's first state is shaped like case 331's so they share the
 *     distance test; case 467's mode dispatch is a switch.
 *   - Case 494 (the last arm) gives each of its three despawn tests its own
 *     `DespawnActorRecord(actor); continue;`. jump.c then has to make a new label
 *     for the shared despawn tail, and jumps to a label made during the pass
 *     are never compared with each other. That keeps the duplicated
 *     `g_pfnLevelEmitParticles(...); despawn` tails of arms 14, 67, 250 and 329 apart, as
 *     in the original.
 *   - Case 14 samples the floor through svF (idle) and floorProbe (mode 3).
 *   - Case 467 reads Spyro's heading and damage flags through SPY, so the
 *     loads stay behind the st->speed and bs->mode stores (cookbook A302).
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

/* ENTER_POSE for arms whose fall-out runs a shared exit after the switch. */
#define ENTER_POSE_BRK(a, n)                                                   \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    ANIM_ADVANCE(a, n);                                                        \
  }                                                                            \
  break

/* Type 467 state: a bolt-throwing sentry with three guards (`kids`). */
typedef struct BoltSentryState {
  int mode;      /* 0x00 enum BoltSentryCue */
  PathData *path; /* 0x04 */
  int range;     /* 0x08 in 1024s */
  int timer;     /* 0x0C */
  int pad10;       /* 0x10 */
  Actor *bolt;   /* 0x14 */
  int pad18;       /* 0x18 */
  int heading;   /* 0x1C */
  int speed;     /* 0x20 */
  int unread24;       /* 0x24 */
  int guards;    /* 0x28 */
  Actor *kids[3]; /* 0x2C */
  Actor *fx;     /* 0x38 */
} BoltSentryState;

/* Type 494 state: the bolt spawned by 35 and 467. Mode 0 rides its parent's
 * attach point aimed at Spyro, mode 1 flies and knocks Spyro back, mode 2
 * steers toward `target` and hits actors there. */
typedef struct AimedBoltState {
  Actor *parent; /* 0x00 */
  int target[3]; /* 0x04 */
  short timer;   /* 0x10 */
  short mode;    /* 0x12 AimedBoltMode */
} AimedBoltState;

enum AimedBoltMode {
  AIMBOLT_HELD = 0,     /* rides the thrower's attach point, aimed at Spyro, growing */
  AIMBOLT_AT_SPYRO = 1, /* flies straight on; touching Spyro shoves him and despawns */
  AIMBOLT_AT_LINK = 2,  /* steers toward `target` (the guard's link) and hits actors there */
};

/* An aimed bolt flies at SPEED (contact CONTACT_RADIUS) until its timer,
 * a wall or a hit: AT_SPYRO hits him within HIT_RADIUS (a 1/32-unit shove
 * along its heading), AT_LINK turns at most TURN_STEP a frame in yaw and
 * pitch toward `target` and blasts actors within SPEED of it. */
#define AIMBOLT_SPEED 0x15E
#define AIMBOLT_CONTACT_RADIUS 0x96
#define AIMBOLT_HIT_RADIUS 0xFA
#define AIMBOLT_TURN_STEP 2

/* ENTER_POSE_BRK with the pose constant split from the queue store (A285). */
#define ENTER_POSE_BRK_B(a, n)                                                 \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    ANIM_ADVANCE(a, n);                                                        \
  }                                                                            \
  break

void Level19_UpdateActors(void) {
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
  int floorProbe[3];
  int acc[3][3];
  int bouncePos[3];
  int bounceNormal[3];
  int homingStep[3];
  int toTargetAhead[3];
  int sparxToTarget[3];
  int o288[3];
  int sparkleTwinkle[3];
  int shardTwinkle[3];
  int o336[3];
  int o352[3];
  int podSparkVel[3];
  int o384[3];
  int toPartner[3];
  int barrelAxis[3];
  int muzzlePos[3];
  int smokeVel[3];
  int ballToNode[3];
  int o480[3];
  int letterSide[3];
  int letterSlot[3];
  int o528[3];

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

    /* Type 32: rides attach point 0 of the bolt sentry that spawned it and
     * despawns once the sentry leaves BSENTRY_THROW. It then shares the dragon
     * egg arm, where its mode 0 (EGG_CARRIED) does nothing. */
    case ACTOR_TYPE_CARRIED_EGG: {
      Actor **st = actor->state;

      if (actor->subMode != 0 || (*st)->mode != BSENTRY_THROW) {
        DespawnActorRecord(actor);
        continue;
      }
      GetActorAttachPoint(*st, 0, &actor->posX);
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

    case ACTOR_TYPE_BOLT_GUARD: {
      BoltGuardState *st = actor->state;
      int *v = svD;
      int d;

      SubtractVector(v, &spy->posX, &actor->posX);
      d = ApproxDist2D(&actor->posX, &spy->posX);
      if (d < EXACT_DIST_RANGE) {
        d = VectorLength(v, VEC_LEN_XY);
      }
      TickActorFlameGlow(actor);
      if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER) && actor->mode != BGUARD_KNOCKED) {
        int i;

        st->zSpeed = BGUARD_KNOCK_VZ;
        st->speed = BGUARD_KNOCK_SPEED;
        st->knockHeading =
            StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                        actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8),
                          g_bSpyroPersistentEulerZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        actor->flags = 0;
        st->spinX = RandomSignedInRange(6, 0xA);
        st->spinY = RandomSignedInRange(6, 0xA);
        for (i = 0; i < BGUARD_SHARDS; i++) {
          if (ACTOR_POOL_LOW()) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_GROUND_BOUNCE_SHARD, actor);
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        ANIM_SET(actor, BGUARD_ANIM_KNOCKED);
        PlayActorMeshVoice(actor, BGUARD_VOICE_KNOCKED, 0);
        actor->mode = BGUARD_KNOCKED;
        continue;
      }
      actor->flags = 0;
      switch (actor->mode) {
      case BGUARD_WATCH:
        ANIM_GO(actor, BGUARD_WATCH);
        if (st->mode == one /* BGUARD_CUE_RUN */ &&
            (IsPointInRotatedBox(&spy->posX, &st->box) != 0 || d < st->range << 10) &&
            ABS2(SPY_DZ(actor)) < BGUARD_CUE_DZ) {
          if (actor->group != 0xFF) {
            SetActorGroupStateExcept(actor, BGUARD_RUN, BGUARD_KNOCKED);
          }
          actor->mode = BGUARD_RUN;
          continue;
        }
        if (st->mode == BGUARD_CUE_LINK && st->link != -1 && d < st->range << 10 &&
            ABS2(SPY_DZ(actor)) < BGUARD_CUE_DZ) {
          st->timer2 = BGUARD_FACE_LINK_TICKS;
          actor->mode = BGUARD_FACE_LINK;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && d < BGUARD_FIRE_RANGE &&
            ABS2(SPY_DZ(actor)) < BGUARD_FIRE_DZ && TICK_DOWN(st->countdown) != 0) {
          ENTER_POSE(actor, BGUARD_WIND_UP);
        }
        break;

      case BGUARD_ROUSE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, BGUARD_WIND_UP);
        }
        break;

      case BGUARD_FIRE:
        TrackEntityYawTowardSpyro(actor, 0xE, 0, YAW_STEP_IN_BAND);
        if (TICK_DOWN(st->timer) != 0) {
          int i;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          for (i = 0; i < 2; i++) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
            AimedBoltState *cs;

            c->scale = BGUARD_BOLT_SCALE;
            c->rotZ = actor->rotZ;
            cs = c->state;
            cs->mode = one; /* AIMBOLT_AT_SPYRO */
            cs->timer = BGUARD_BOLT_LIFE;
            GetActorAttachPoint(actor, i, &c->posX);
          }
          st->timer = BGUARD_VOLLEY_GAP;
        }
        if (TICK_DOWN(st->timer2) != 0 && g_nActorAnimDone != 0) {
          st->timer = BGUARD_RELOAD_TICKS;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        break;

      case BGUARD_KNOCKED:
        actor->rotX += st->spinX;
        actor->rotY += st->spinY;
        if (DriveActorTrackedMove(actor, &st->speed, st->knockHeading, &st->zSpeed, BGUARD_KNOCK_DRAG,
                          BGUARD_KNOCK_GRAVITY) == MOVED_LANDED) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, BGUARD_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case BGUARD_WIND_UP:
        if (g_nActorAnimDone != 0) {
          st->timer2 = BGUARD_FIRE_TICKS;
          ENTER_POSE(actor, BGUARD_FIRE);
        }
        break;

      case BGUARD_RUN:
        ANIM_GO(actor, BGUARD_ANIM_RUN);
        if (PivotActorAlongPath(actor, st->path, BGUARD_PIVOT_THRESH, BGUARD_RUN_SPEED, 0, 8, 0x10, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            PATH_ARRIVED(0)) {
          st->mode = BGUARD_CUE_NONE;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < BGUARD_RUN_STOP_RANGE) {
          st->mode = BGUARD_CUE_NONE;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        break;

      case BGUARD_FACE_LINK:
        ANIM_GO(actor, BGUARD_WATCH);
        if (g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT ||
            ApproxDist2D(&actor->posX, &SPY.posX) < BGUARD_LINK_ABORT_RANGE || st->link == -1) {
          st->mode = BGUARD_CUE_NONE;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->link].posX - actor->posX,
                                    g_pActorListBase[st->link].posY - actor->posY, ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer2) != 0) {
          st->timer2 = BGUARD_LINK_FIRE_TICKS;
          actor->mode = BGUARD_FIRE_AT_LINK;
          continue;
        }
        break;

      case BGUARD_FIRE_AT_LINK:
        ANIM_GO(actor, BGUARD_FIRE);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->link].posX - actor->posX,
                                    g_pActorListBase[st->link].posY - actor->posY, ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0) {
          int i;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          for (i = 0; i < 2; i++) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
            AimedBoltState *cs;

            c->scale = BGUARD_BOLT_SCALE;
            cs = c->state;
            c->rotZ = actor->rotZ;
            cs->mode = AIMBOLT_AT_LINK;
            cs->timer = BGUARD_LINK_BOLT_LIFE;
            CopyVector(cs->target, &g_pActorListBase[st->link].posX);
            cs->target[2] += BGUARD_LINK_AIM_RAISE;
            GetActorAttachPoint(actor, i, &c->posX);
          }
          st->timer = BGUARD_VOLLEY_GAP;
        }
        if (TICK_DOWN(st->timer2) != 0 && g_nActorAnimDone != 0) {
          RoamingRamState *ts = g_pActorListBase[st->link].state;

          if (g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT || st->link == -1) {
            st->mode = BGUARD_CUE_NONE;
            ENTER_POSE(actor, BGUARD_WATCH);
          }
          st->link = ts->partner; /* on along the rams' partner chain */
          st->timer2 = BGUARD_FACE_LINK_TICKS;
          actor->mode = BGUARD_FACE_LINK;
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Explosion shard with a ground bounce (TumbleState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_GROUND_BOUNCE_SHARD:
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
        svA[0] = GetRandomU32() & 3;
        svA[1] = GetRandomU32() & 3;
        svA[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, (int)svA);
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
          CopyVector(svF, &actor->posX);
          svF[2] += GROUND_PROBE_LIFT;
          FindGroundHeightBelow(svF, GROUND_PROBE_FAR);
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
            CopyVector(floorProbe, &actor->posX);
            floorProbe[2] += GROUND_PROBE_LIFT;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(floorProbe, 0x1000) > 0) {
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
        if (ApproxDist2D(&anchor->posX, &SPY.posX) > SAVEHOST_REARM_RANGE) {
          actor->mode = SAVEHOST_WAIT_ARMED;
        }
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

            CopyVector(toTargetAhead, &st->target->posX);

            if (high == 0) {
              toTargetAhead[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              toTargetAhead[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              toTargetAhead[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              toTargetAhead[1] -=
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

            SubtractVector(toTargetAhead, toTargetAhead, &actor->posX);
            dist = VectorLength(toTargetAhead, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(toTargetAhead, dist, limit);
            } else {
              ScaleVector3ByRatio(toTargetAhead, dist, reach - SPARX_CHASE_STEP_SLACK);
            }
            AddVector(&actor->posX, &actor->posX, toTargetAhead);
            TurnActorYawTowardAngle(actor, ArcTan2(toTargetAhead[0], toTargetAhead[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(toTargetAhead, VEC_LEN_XY), toTargetAhead[2], ATAN_ANGLE8);
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

    

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_REBOUND_FX:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

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
          SubtractVector(o288, at, &spy->posX);
          o288[2] = (o288[2] * 3) >> 2;

          if (VectorLength(o288, VEC_LEN_XYZ) < STATUE_TOUCH_RADIUS) {
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
      SubtractVector(o288, &spy->posX, &actor->posX);
      distance = VectorLength(o288, VEC_LEN_XY);
      if (distance < st->radius && o288[2] >= -WHIRL_BELOW_REACH && o288[2] < st->spanZ &&
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
          ZeroVector(o336);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, (int)o336);
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
            CopyVector(o352, &st->prize->posX);
            o352[0] += g_anCosineLut[j * 43] >> 6;
            o352[1] += g_anSineLut[j * 43] >> 6;
            o352[2] += 0x28;
            podSparkVel[0] = g_anCosineLut[j * 43] >> 7;
            podSparkVel[1] = g_anSineLut[j * 43] >> 7;
            podSparkVel[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, o352, (int)podSparkVel);
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

    case ACTOR_TYPE_ATTACH_BEAD: {
      AttachBeadState *st = actor->state;

      switch (actor->mode) {
      case BEAD_INIT:
        st->t = -(actor->subMode * 10) - actor->subMode * 32;
        actor->mode = BEAD_DELAY;
        actor->cullRadius = ACTOR_CULL_HIDDEN;
        continue;

      case BEAD_DELAY:
        st->t += g_nFrameStep;
        if (st->t >= 0) {
          actor->mode = BEAD_STREAM;
        }
        continue;

      case BEAD_STREAM: {
        int *a = o352;
        int *b = o384;
        int *c = o336;
        int k;

        actor->cullRadius = CULL_RADIUS(0x2000);
        st->t += g_nFrameStep;
        k = (st->t << 5) & 0x3FF;
        GetActorAttachPoint(st->owner, 0, a);
        GetActorAttachPoint(st->owner, 1, b);
        SubtractVector(c, b, a);
        actor->rotY = ArcTan2(VectorLength(c, VEC_LEN_XY), c[2], ATAN_ANGLE8);
        actor->rotZ = ArcTan2(c[0], c[1], ATAN_ANGLE8);
        a[0] *= k;
        a[1] *= k;
        a[2] *= k;
        b[0] *= 0x400 - k;
        b[1] *= 0x400 - k;
        b[2] *= 0x400 - k;
        AddVector(&actor->posX, a, b);
        RShiftVector3(&actor->posX, 0xA);
        actor->scale = (k >> 5) + 0x18;
        continue;
      }

      case BEAD_DESPAWN:
        DespawnActorRecord(actor);
        continue;
      }
      continue;
    }

    

    /* One-shot effects: removed as soon as their animation ends. */
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
          SubtractVector(toPartner, &a->posX, &actor->posX);
          d = ABS2(toPartner[0]) + ABS2(toPartner[1]);
          if (d < LID_TARGET_PREFILTER &&
              (unsigned int)(VectorLength(toPartner, VEC_LEN_XY) - LID_TARGET_MIN_RANGE) <=
                  LID_TARGET_MAX_RANGE - LID_TARGET_MIN_RANGE &&
              (unsigned int)(toPartner[2] + LID_TARGET_DZ) <= 2 * LID_TARGET_DZ) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, ATAN_ANGLE12),
                              ArcTan2(toPartner[0], toPartner[1], ATAN_ANGLE12));
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

        SubtractVector(barrelAxis, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(barrelAxis, VEC_LEN_XY), barrelAxis[2], ATAN_ANGLE8);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + ANGLE8_QUARTER) & ANGLE8_MASK) < CANNON_LOW_PITCH_SPAN) {
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
        actor->cullRadius = ACTOR_CULL_MAX;
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
        actor->cullRadius = CULL_RADIUS(0x4000);
        actor->updateDist = UPDATE_DIST(0x10000);
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
            ScaleVector3Sat(o480, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(ballToNode, ballToNode, o480);
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
        o480[0] = (g_anCosineLut[i * 32] >> 8) * 3;
        o480[1] = (g_anSineLut[i * 32] >> 8) * 3;
        o480[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, (int)o480);
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
          o480[0] = (g_anCosineLut[bearing] * 3) >> 4;
          o480[1] = (g_anSineLut[bearing] * 3) >> 4;
          o480[2] = 0;
          letterSide[0] = (unsigned short)g_anCosineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          letterSide[1] = (unsigned short)g_anSineLut[(bearing + ANGLE8_QUARTER) & ANGLE8_MASK] << 16 >> 21;
          letterSide[2] = 0;
          bearing = (bearing + ANGLE8_HALF) & ANGLE8_MASK;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(letterSlot, letterSide, st->count - 1);
          LShiftVector3(letterSide, 1);
          arm = st->count - 1;
          letterSlot[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          o528[0] = o480[0] * g_anCosineLut[arm * 2];
          o528[1] = o480[1] * g_anCosineLut[arm * 2];
          o528[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->index * 4)) & 0xFF;
          ScaleVector3Sat(letterSide, letterSide, st->index);
          SubtractVector(letterSlot, letterSlot, letterSide);
          swing = &g_anCosineLut[bearing];
          actor->posX = o480[0] * *swing;
          actor->posY = o480[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, o528);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, o480);
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

    

    /* Type 461: war drum. Beats on a timer (with its own voice), and is
     * knocked away when hit. */
    case ACTOR_TYPE_WAR_DRUM: {
      WarDrumState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && (unsigned int)(actor->mode - WARDRUM_KNOCKED) >= 2) {
        actor->flags = 0;
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, KNOCK_BEND_STEP, KNOCK_BEND_FRAC);
        st->zSpeed = WARDRUM_FLAME_KNOCK_VZ;
        st->life = WARDRUM_KNOCK_LIFE;
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->speed = WARDRUM_FLAME_KNOCK_SPEED;
        } else {
          st->speed = WARDRUM_KNOCK_SPEED;
          st->zSpeed = WARDRUM_KNOCK_VZ;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorLiveKill(actor);
        PlayActorMeshVoice(actor, WARDRUM_VOICE_KNOCKED, 0);
        ENTER_POSE(actor, WARDRUM_KNOCKED);
      }
      if (TICK_DOWN(st->beat) == 2) {
        st->fade = WARDRUM_FADE_TICKS;
        st->wait = st->reload;
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
      }
      if (st->beat == 0 && CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim)) < 2) {
        g_anLevelTriggerStateSlots[st->chunkAnim] = 0;
      }
      if (TICK_DOWN(st->fade) == 0) {
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_KEEP(CHUNK_ANIM_BRANCH), CHUNK_ANIM_BRANCH);
      }

      switch (actor->mode) {
      case WARDRUM_WAIT:
        if (TICK_DOWN(st->wait) == 2) {
          ENTER_POSE(actor, WARDRUM_BEAT);
        }
        break;

      case WARDRUM_BEAT:
        if (actor->nextFrame >= WARDRUM_BEAT_FRAME && st->beat == 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          st->beat = WARDRUM_BEAT_TICKS;
          g_anLevelTriggerStateSlots[st->chunkAnim] = 1;
          PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[WARDRUM_VOICE_BEAT], actor, SFX_MODE_3D,
                        (unsigned char *)&st->voice);
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, WARDRUM_WAIT);
        }
        break;

      case WARDRUM_KNOCKED:
        actor->rotY += g_nFrameStep * 2;
        if (TICK_DOWN(st->life) != 0 ||
            DriveActorTrackedMove(actor, &st->speed, st->knockHeading, &st->zSpeed, WARDRUM_KNOCK_DRAG, WARDRUM_KNOCK_GRAVITY) ==
                MOVED_LANDED) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, WARDRUM_BURST_SPARK_LIFE);
          actor->cullRadius = ACTOR_CULL_HIDDEN;
          actor->mode = WARDRUM_GONE;
          continue;
        }
        break;

      case WARDRUM_GONE:
        if (GetWorldChunkAnimState(st->chunkAnim) & CHUNK_ANIM_HALTED) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 466: roaming ram. Wanders its waypoint ring away from Spyro,
     * charges down its path once he is close, and is knocked back by hits
     * or by its partner. */
    case ACTOR_TYPE_ROAMING_RAM: {
      RoamingRamState *st = actor->state;

      if ((actor->flags & ACTOR_HIT_ANY) && actor->mode != RAM_KNOCKED) {
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
        ENTER_POSE_B(actor, RAM_TRAPPED);
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

    case ACTOR_TYPE_BOLT_SENTRY: {
      BoltSentryState *st = actor->state;
      int i;

      if (actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER) && ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_HIT_RANGE &&
          actor->mode != BSENTRY_KNOCKED) {
        st->speed = BSENTRY_KNOCK_SPEED;
        st->heading = SPY.bodyRotZ;
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorLiveKill(actor);
        StopSoundVoicesByOwner(actor, VOICE_STOP_LOOPING);
        ENTER_POSE(actor, BSENTRY_KNOCKED);
      }
      if (st->guards != 0) {
        for (i = 0; i < 3; i++) {
          if (st->kids[i] == 0) {
            Actor *k = g_pfnLevelSpawnActor(ACTOR_TYPE_ATTACH_BEAD, actor);

            st->kids[i] = k;
            k->subMode = i;
          }
        }
      }
      switch (actor->mode) {
      case BSENTRY_IDLE:
        if (st->mode != BSENTRY_CUE_NONE) {
          ENTER_POSE(actor, BSENTRY_CUE);
        }
        if (st->guards == 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < st->range << 10) {
          ENTER_POSE(actor, BSENTRY_SUMMON);
        }
        ANIM_GO(actor, BSENTRY_IDLE);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_ROUSE_RANGE &&
            ABS2(actor->posZ - SPY.posZ) < BSENTRY_ROUSE_DZ) {
          ENTER_POSE_BRK(actor, BSENTRY_ROUSE);
        }
        break;

      case BSENTRY_PATROL:
        if (PivotActorAlongPath(actor, st->path, BSENTRY_PIVOT_THRESH, BSENTRY_PATROL_SPEED, 0, 8, 0x28, PIVOT_TURN_ANY, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            PATH_ARRIVED(0)) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_ROUSE_RANGE &&
              ABS2(actor->posZ - SPY.posZ) < BSENTRY_ROUSE_DZ) {
            ENTER_POSE_BRK(actor, BSENTRY_ROUSE);
          }
          ENTER_POSE_BRK(actor, BSENTRY_IDLE);
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_THROW_RANGE &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8)) < BSENTRY_THROW_FACING) {
          ENTER_POSE_BRK_B(actor, BSENTRY_THROW);
        }
        break;

      case BSENTRY_ROUSE:
        if (g_nActorAnimDone != 0) {
          actor->mode = BSENTRY_WATCH;
          ANIM_RESET(actor, BSENTRY_WATCH);
        }
        break;

      case BSENTRY_WATCH: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        if (st->guards == 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10) {
          ENTER_POSE(actor, BSENTRY_SUMMON);
        }
        TrackEntityYawTowardSpyro(actor, 3, 0, YAW_HOLD_IN_BAND);
        if (TICK_DOWN(st->timer) != 0 && d < BSENTRY_WATCH_THROW_RANGE &&
            ABS2(actor->posZ - SPY.posZ) < BSENTRY_ROUSE_DZ) {
          ENTER_POSE_BRK_B(actor, BSENTRY_THROW);
        }
        if (d > BSENTRY_LOWER_RANGE) {
          ENTER_POSE_BRK(actor, BSENTRY_LOWER);
        }
        break;
      }

      case BSENTRY_THROW:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (st->fx == 0 && (unsigned)(actor->nextFrame - BSENTRY_FX_FRAME) < BSENTRY_FX_FRAMES) {
          st->fx = g_pfnLevelSpawnActor(ACTOR_TYPE_CARRIED_EGG, actor);
        }
        if (st->bolt == 0 && (unsigned)(actor->nextFrame - BSENTRY_BOLT_FRAME) < BSENTRY_BOLT_FRAMES &&
            ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_THROW_RANGE) {
          AimedBoltState *bs;

          st->bolt = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
          st->bolt->rotZ = actor->rotZ;
          bs = st->bolt->state;
          bs->mode = AIMBOLT_HELD;
          g_dwSpyroTriggerEventFlags = SPY.triggerEvents | SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT;
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, &st->bolt->posX);
          bs->parent = actor;
        }
        if (actor->nextFrame >= BSENTRY_RELEASE_FRAME && st->bolt != 0) {
          st->bolt->subMode = 1;
          st->bolt = 0;
          if (st->fx != 0) {
            if (st->fx->mode < ACTOR_MODE_DORMANT) {
              st->fx->subMode = 1;
            }
            st->fx = 0;
          }
        }
        if (g_nSpyroState == SPYRO_ST_HURT_TINT) {
          st->timer = BSENTRY_RECOVER_TICKS;
        }
        if (g_nActorAnimDone != 0) {
          if (st->timer != 0) {
            actor->mode = BSENTRY_RECOVER;
            ANIM_RESET(actor, BSENTRY_RECOVER);
            break;
          }
          st->timer = BSENTRY_THROW_GAP;
          actor->mode = BSENTRY_IDLE;
          ANIM_RESET(actor, BSENTRY_IDLE);
        }
        break;

      case BSENTRY_LOWER:
        if (g_nActorAnimDone != 0) {
          actor->mode = BSENTRY_IDLE;
          ANIM_RESET(actor, BSENTRY_IDLE);
        }
        break;

      case BSENTRY_KNOCKED:
        if (st->bolt != 0) {
          st->bolt->subMode = 1;
          st->bolt = 0;
        }
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, BSENTRY_KNOCK_CONTACT, BSENTRY_MOVE_RADIUS, one);
          st->speed -= BSENTRY_KNOCK_DRAG;
        }
        if (g_nActorAnimDone != 0) {
          for (i = 0; i < 3; i++) {
            if (st->kids[i] != 0) {
              st->kids[i]->mode = BEAD_DESPAWN;
            }
          }
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, BSENTRY_BURST_SPARK_LIFE);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case BSENTRY_RECOVER:
        if (g_nActorAnimDone != 0 && TICK_DOWN(st->timer) != 0) {
          ENTER_POSE_BRK(actor, BSENTRY_IDLE);
        }
        break;

      case BSENTRY_CUE:
        switch (st->mode) {
        case BSENTRY_CUE_LEAP:
          if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
              ABS2(SPY_DZ(actor)) < BSENTRY_LEAP_DZ) {
            st->mode = BSENTRY_CUE_NONE;
            st->speed = ApproxDist2D(&actor->posX, &st->path->nodes[1].x) / BSENTRY_LEAP_STEPS;
            st->unread24 = 0x6E;
            ENTER_POSE_BRK(actor, BSENTRY_LEAP);
          }
          break;
        case BSENTRY_CUE_PATROL:
          if (ApproxDist2D(&actor->posX, &spy->posX) < BSENTRY_PATROL_CUE_RANGE &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8)) < BSENTRY_THROW_FACING) {
            st->mode = BSENTRY_CUE_NONE;
            ENTER_POSE_BRK(actor, BSENTRY_PATROL);
          }
          break;
        }
        break;

      case BSENTRY_LEAP: {
        int h = ArcTan2(st->path->nodes[1].x - actor->posX,
                              st->path->nodes[1].y - actor->posY, ATAN_ANGLE8);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[1].x);

        ProbeActorGroundHeight(actor);
        TrackEntityYawTowardSpyro(actor, 3, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          if (st->guards == 0) {
            ANIM_QUEUE(actor, BSENTRY_SUMMON);
          }
          ANIM_QUEUE(actor, BSENTRY_IDLE);
        }
        if ((unsigned)(actor->nextFrame - BSENTRY_LEAP_FRAME) < BSENTRY_LEAP_FRAMES && st->speed < d) {
          MoveActorAlongAngle(actor, h, st->speed, 0, 0, 0);
        }
        break;
      }

      case BSENTRY_SUMMON:
        if (actor->nextFrame >= BSENTRY_SUMMON_FRAME) {
          st->guards = one;
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE_BRK(actor, BSENTRY_IDLE);
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
      continue;
    }

    /* Type 494: bolt thrown by 35 and 467. */
    case ACTOR_TYPE_AIMED_BOLT: {
      AimedBoltState *st = actor->state;

      if (actor->subMode != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (st->mode == AIMBOLT_HELD) {
        int *v = o528;
        int k;

        RefreshActorDerivedState(st->parent, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(st->parent, 0, &actor->posX);
        actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                     spy->posY - actor->posY, ATAN_ANGLE8);
        actor->rotY = ArcTan2(ApproxDist2D(&actor->posX, &spy->posX),
                                     SPY.posZ - actor->posZ, ATAN_ANGLE8);
        SubtractVector(v, &spy->posX, &actor->posX);
        k = 0x14 - VectorLength(v, VEC_LEN_XYZ) * 0x1A / 1400;
        if (k < -0x14) {
          k = -0x14;
        }
        actor->scale = k + 0x20;
        continue;
      }
      if (st->mode == AIMBOLT_AT_LINK) {
        int t = (ArcTan2(st->target[0] - actor->posX,
                               st->target[1] - actor->posY, ATAN_ANGLE8) -
                 actor->rotZ) &
                ANGLE8_MASK;

        if (t > ANGLE8_HALF) {
          t -= ANGLE8_TURN;
        }
        if (t > AIMBOLT_TURN_STEP) {
          t = AIMBOLT_TURN_STEP;
        }
        if (t < -AIMBOLT_TURN_STEP) {
          t = -AIMBOLT_TURN_STEP;
        }
        actor->rotZ += t;
        t = (ArcTan2(ApproxDist2D(&actor->posX, st->target),
                           st->target[2] - actor->posZ, ATAN_ANGLE8) -
             actor->rotY) &
            ANGLE8_MASK;
        if (t > ANGLE8_HALF) {
          t -= ANGLE8_TURN;
        }
        if (t > AIMBOLT_TURN_STEP) {
          t = AIMBOLT_TURN_STEP;
        }
        if (t < -AIMBOLT_TURN_STEP) {
          t = -AIMBOLT_TURN_STEP;
        }
        actor->rotY += t;
      }
      CopyVector(o480, &actor->posX);
      if (st->mode == one /* AIMBOLT_AT_SPYRO */ && RaiseSpyroTriggerInRadius(&actor->posX, AIMBOLT_HIT_RADIUS, SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT) != 0) {
        g_anSpyroRequestVelocity.x = (short)((unsigned short *)g_anCosineLut)[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityY = (short)((unsigned short *)g_anSineLut)[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityZ = 0;
        DespawnActorRecord(actor);
        continue;
      }
      if (st->mode == AIMBOLT_AT_LINK &&
          DispatchActorContactAtSphere(&actor->posX, AIMBOLT_SPEED, 0, ACTOR_HIT_BLAST, actor, CONTACT_STOP_AT_SOLID) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TICK_DOWN(st->timer) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TryMoveActorForward(actor, AIMBOLT_SPEED, AIMBOLT_CONTACT_RADIUS, 0, 0) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (CastRayWorldAndActors(o480, &actor->posX) != 0) {
        DespawnActorRecord(actor);
      }
      continue;
    }
}
  }
}
