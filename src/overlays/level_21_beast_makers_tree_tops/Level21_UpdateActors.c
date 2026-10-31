/* Level21_UpdateActors -- per-frame actor update for level 21 (Tree
 * Tops). 0x8007B698, 46,240 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 7/10/12/13/16/26/33; the level-21 classes
 * are the boulder and its shards (57), the boulder thrower (100/102), the
 * gatekeeper (101) and the path runner (293/294) that leads Spyro to a goal
 * and raises a per-goal flag. The roaming ram (453) is level 18/20's 466
 * without the voice channel.
 *
 * Load-bearing source forms:
 *   - Level 7's frame base (actor in s4, spy = base - 8 in fp). The scratch
 *     arrays are declared in the original's slot order. A slot one arm owns
 *     is named by its use; slots several arms share are svA .. svJ
 *     (sp offsets 56 .. 528, in order).
 *   - Case 100: `st->rock = 0` before the heading store, vz stored after vy,
 *     the camera z through CAM_LOOK, and the state-2 exit test as one &&
 *     chain (the branchy ?: range test only survives jump.c there).
 *   - Case 293: the goal-arrival `last` index as a ternary (cse must not keep
 *     st->mode across it), the spark loop's angle as an int `& 0xFF`, and the
 *     mode-1 arrival test written flag-clear-first.
 *   - Case 57: the floor delta through two temps (fold reassociates the
 *     one-expression form into `(h + 0x50) - z`).
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


/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */


/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */


/* Glow node owned by Sparx; follows its anchor position. */

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */


/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = sixteen;                                                        \
  (a)->frameRate = sixteen;                                                        \
  (a)->anim = (a)->nextAnim;                                                     \
  (a)->nextAnim = (next);                                                         \
  (a)->frame = (a)->nextFrame;                                                     \
  (a)->nextFrame = 0;                                                              \
  OnActorAnimQueued(a);

/* ENTER_POSE sets the state byte to `n`, advances to animation `n` unless it
 * is already queued, and moves on to the next actor. It ends in a bare
 * `continue`, so use it only inside braces. */
#define ENTER_POSE(a, n)                                                       \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    (a)->frameProgress = sixteen;                                                      \
    (a)->frameRate = sixteen;                                                      \
    (a)->anim = (a)->nextAnim;                                                   \
    (a)->nextAnim = (n);                                                          \
    (a)->frame = (a)->nextFrame;                                                   \
    (a)->nextFrame = 0;                                                            \
    OnActorAnimQueued(a);                                                          \
  }                                                                            \
  continue

#define SPY g_anSpyroWorldPosBlock

/* ==== level_30 per-class state blocks ====
 *
 * Gnorc Gnexus is the hub for Gnasty's world: the balloonist, the vortex
 * pads, the two boss-door gnorcs and the shared fodder/debris classes. Each
 * block below is the ->state record of one actor class, named after the case
 * that uses it. */


/* Case 328 -- floor-snapped marker. */

/* ==== Camera and Spyro globals used by the camera arms ==== */

#define sixteen 0x10

/* Types 288/289: bouncing debris. Flies on its velocity until its life runs
 * out; type 288 tumbles, 289 sways as it falls. */

/* ==== Level 7 classes ==== */


/* ==== Level 21 classes ==== */


extern int g_anLevelRunnerArrived[]; /* per-goal "runner arrived" flags */
extern int g_nSpawnAnchorValid[];

/* ==== The function ==== */
#define one 1
void Level21_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int dt;

  /* Scratch vectors, in the original's slot order (shared slots svA..svJ). */
  int svA[3];
  int eggCentre[3];
  int eggRise[3];
  int svB[3];
  int eggFxPos[3];
  int svC[3];
  int throwGripMid[3];
  int svD[3];
  int throwHand1[3];
  int throwCarryMid[3];
  int svE[3];
  int svF[3];
  int keepHand1[3];
  int keepCarryMid[3];
  int svG[3];
  int keepCarryHand1[3];
  int pickupTri[3][3];
  int pickupFloorDir[3];
  int pickupToSpyro[3];
  int sparxToTarget[3];
  int cageAnchor[3];
  int cageShakeAnchor[3];
  int svH[3];
  int runnerSpark[7];
  int triggerToSpyro[3];
  int shardSparkPos[3];
  int svI[3];
  int svJ[3];
  int podSparkVel[3];
  int orbitDir[3];
  int orbitSwing[3];
  int orbitSway[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, 0);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apActiveActorList;
  base = (char *)&g_nSpyroWorldPosZ;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
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

      if (actor->mode != LEVELEXIT_FAR ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == LEVELEXIT_FAR) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = LEVELEXIT_NEAR;
      } else {
        actor->mode = LEVELEXIT_FAR;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, EMIT_RISING_SPIRAL_SMEAR, &actor->posX, (void *)tick);
        }
      }

      if (dist < 0x400) {
        int top = g_nSpyroWorldPosZ;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == SPYRO_ST_LIFT || top + 0xC00 < z + depth) {
            if (g_abHudCounterDisplayState == 2) {
              RefreshHudLevelGemCounter();
            }
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_TARGET_CAMERA);
            g_pScriptedCameraActor = actor;
            g_dwSpyroTriggerEventFlags |= SPYRO_EV_LIFT;
            g_nSpyroRequestTargetZ = ACTOR_RAW(actor, int, posZ) + 0x4000;
            CopyVector(SPY_HELD_FIELD(base, int, firstContactPoint), &actor->posX);

            if (depth < drop) {
              int yaw = g_nCameraEulerPitch & 0xFFF;

              if (yaw > 0x800) {
                yaw -= 0x1000;
              }
              if (yaw < -0x200) {
                g_abLevelExited[g_nLevelIndex] = 1;
                g_nCdStreamState = 0;
                g_nEnteredPortalIdx = -1;
                g_nPortalTransitionActive = 1;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = GS_LEVEL_LOAD;
                g_nGamePaused = 1;
                g_nReturnPortalLevelId = g_nActiveLevelId;
                g_nCurrentLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(0);
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
          actor->updateDist = 0xFF;
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

          actor->mode = 0;
          ANIM_RESET(actor, 0);
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
          if (TickCountdownByFrameDelta(&st->release, 4) != 0) {
            int *vel = st->pos;

            if (st->trackPos != 0) {
              AddVector(vel, vel, &actor->posX);
            } else {
              CopyVector(&actor->posX, &parent->posX);
            }

            st->pos[2] += 0x400;

            if (st->dropToSpyro != 0) {
              SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              SpawnActorDeathDrop(actor, DROP_AT_POINT, 0, st->pos);
            } else {
              SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
            }
            MarkActorCommittedKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->voiceRow);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= ACTOR_MODE_DORMANT) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < ACTOR_MODE_DORMANT && parent->type == ACTOR_TYPE_FOLLOWER &&
                ((FollowState *)parent->state)->parent == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->voiceRow);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case ACTOR_TYPE_BUTTERFLY: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.sparxHealth < SPARX_GOLD && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pSparxActor == 0) {
            if (SPY.sparxHealth >= SPARX_GONE) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, 0, 0);

              SPY.sparxHealth = SPARX_GREEN;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != SPARX_SUB_PARKED) {
            ChaserState *cs = (ChaserState *)g_pSparxActor->state;

            if (cs->owner == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) <
                  0) {
                st->dir = 1;
              } else {
                st->dir = -1;
              }

              g_pSparxActor->subMode = SPARX_SUB_CHASE;
              cs->owner = actor;
              cs->timer = RandomIntInRange(150, 210);

              actor->mode = BUTTERFLY_CIRCLE;
            }
          }
        }
      }

      if (actor->cullRadius & 0x80) {
        break;
      }

      switch (actor->mode) {
      case BUTTERFLY_FLUTTER: {
        if (TickCountdownByFrameDelta(&st->idleTimer, 2) == 0 || actor->drawn) {
          int refHeight, rotated, moved;

          if (TickCountdownByFrameDelta(&st->headingTimer, 1) != 0) {
            int r = RandomSignedInRange(30, 90);
            if (GetRandomU32() & 1) {
              r = -r;
            }

            st->heading = WrapAddAngle8(st->heading, r);
            st->headingTimer = RandomIntInRange(60, 140);
          }

          if (TickCountdownByFrameDelta(&st->climbTimer, 1) != 0) {
            st->vz = RandomSignedInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (st->vz >= 128) {
            actor->posZ = actor->posZ + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 300) {
            st->vz = RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          if (1024 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(10, 25);
            st->climbTimer = RandomIntInRange(80, 140);
          }

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, 1);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, 0);

            st->heading = angle;
            st->headingTimer = 40;
          }
        } else {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      case BUTTERFLY_CIRCLE: {
        SteerActorAroundPoint(actor, &spy->posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pSparxActor == 0 || g_pSparxActor->subMode == SPARX_SUB_PARKED) {
          actor->mode = BUTTERFLY_FLUTTER;
        } else {
          int refHeight;

          if (TickCountdownByFrameDelta(&st->climbTimer, 1)) {
            st->vz = RandomSignedInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (st->vz >= 128) {
            actor->posZ = (actor->posZ) + (st->vz - 256);
          } else {
            actor->posZ = actor->posZ + st->vz;
          }

          refHeight = actor->posZ - ProbeActorGroundHeight(actor);

          if (refHeight < 200) {
            st->vz = RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
          }

          if (1000 < refHeight && refHeight < 2048) {
            st->vz = -RandomIntInRange(5, 20);
            st->climbTimer = RandomIntInRange(40, 80);
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

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case EGG_RELEASED:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - spy->posX, actor->posY - spy->posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode += 1;
        break;

      case EGG_SPIRAL_UP: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x80) {
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
          x = cb * 0xB5;
          y = cs * 0xB5;
          svA[0] += (y + x) * 4 >> 12;
          svA[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          svA[2] += 0x300;
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

        if (st->phase < 0x40) {
          SVectorToVectorShl2(eggCentre, st->centre);
          SVectorToVectorShl2(eggRise, st->apex);
          SubtractVector(eggRise, eggRise, eggCentre);
          ScaleVector3Sat(eggRise, eggRise, st->phase);
          RShiftVector3(eggRise, 6);
          AddVector(&actor->posX, eggCentre, eggRise);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(svB, &spy->posX);
          svB[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svB[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svB[2] += 0x300;
          VectorToSVectorShr2(st->centre, svB);
          st->radius = 0x200;
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

        if (st->phase < 0x80) {
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
        SubtractVector(eggFxPos, &actor->posX, &CAM.posX);
        WorldToCameraRotate(eggFxPos, eggFxPos);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, eggFxPos, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->owner);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Type 57: boulder. Rolls down to the floor and bursts into shards
     * (more type-57 actors in state 1) that bounce off the scenery. */
    case ACTOR_TYPE_ROLLING_BOULDER: {
      BoulderState *st = actor->state;

      switch (actor->mode) {
      case 0: {
        int n;

        if (actor->subMode == 0) {
          continue;
        }
        {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - 0x50;

          st->vz = h - z;
        }
        if (st->vz > 0x50) {
          st->vz = 0x50;
        }
        if (st->vz < -0x50) {
          st->vz = -0x50;
        }
        if (ABS(st->vz) < 0x32) {
          st->vz = 0;
        }
        if (st->life > 0xA) {
          st->vz = 0;
        }
        CopyVector(svB, &actor->posX);
        actor->posX += st->vx;
        actor->posY += st->vy;
        actor->posZ += st->vz;
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        st->life += g_nFrameStep;
        if (actor->subMode != 2 && st->life < 0x79 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0xDC, 0x86) == 0 &&
            CastRayWorldAndActors(svB, &actor->posX) == 0) {
          continue;
        }
        for (n = 0; n < 8; n++) {
          Actor *o;
          BoulderState *q;

          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          o = g_pfnLevelSpawnActor(ACTOR_TYPE_ROLLING_BOULDER, actor);
          o->mode = 1;
          if (o->anim != 1) {
            ANIM_RESET(o, 1);
          }
          q = o->state;
          if (actor->subMode == 2) {
            int a = (st->heading + RandomIntInRange(-0x28, 0x28)) & 0xFF;
            int s = RandomIntInRange(0x96, 0xFA);

            q->vx = (s * g_anCosineLut[a]) >> 12;
            q->vy = (s * g_anSineLut[a]) >> 12;
          } else {
            q->vx = RandomIntInRange(-0x96, 0x96);
            q->vy = RandomIntInRange(-0x96, 0x96);
          }
          q->vz = RandomIntInRange(-0x78, 0xFA);
          q->life = RandomIntInRange(0x50, 0x78);
          o->rotX = GetRandomU32();
          o->rotY = GetRandomU32();
          o->rotZ = GetRandomU32();
          q->spin[0] = RandomSignedInRange(7, 0xF);
          q->spin[1] = RandomSignedInRange(7, 0xF);
          q->spin[2] = RandomSignedInRange(7, 0xF);
        }
        g_anSpyroRequestVelocity.x = (st->vx * 3) >> 3;
        g_nSpyroRequestVelocityZ = 0x1E;
        g_nSpyroRequestVelocityY = (st->vy * 3) >> 3;
        DespawnActorRecord(actor);
        continue;
      }

      case 1:
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        st->vz -= 0x14;
        if (st->vz < -0xA0) {
          st->vz = -0xA0;
        }
        svA[0] = st->vx;
        svA[1] = st->vy;
        svA[2] = st->vz;
        CopyVector(svB, &actor->posX);
        AddVector(&actor->posX, &actor->posX, svA);
        if (TickCountdownByFrameDelta(&st->life, 2) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) == 0) {
          continue;
        }
        CopyVector(svC, &g_anCollisionProbeVec);
        if (ReflectVectorOffNormal(svA, svC, svA) == 0) {
          continue;
        }
        st->vx = (svA[0] * 7) >> 3;
        st->vy = (svA[1] * 7) >> 3;
        st->vz = (svA[2] * 7) >> 3;
        continue;
      }
      continue;
    }

    /* Type 100 (and 102): boulder thrower. Patrols its path, picks up a
     * boulder (57) and hurls it, rides its partner, and charges Spyro. */
    case ACTOR_TYPE_BOULDER_THROWER: {
      BoulderThrowerState *st = actor->state;
      int f;

      if (actor->type == ACTOR_TYPE_BOULDER_THROWER_B) {
        TickActorFlameGlow(actor);
      }
      f = actor->flags;
      if ((f & 0xF0000) && actor->mode != 3 &&
          (actor->type != ACTOR_TYPE_BOULDER_THROWER_B || f != 0x10000)) {
        Actor *c;

        if (f & 0x40000) {
          st->speed = 0;
          st->zSpeed = 0x28;
        } else {
          if (f == 0x20000) {
            st->speed = 0x15E;
          } else {
            st->speed = 0x118;
          }
          st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
          if (actor->mode != 0x2B) {
            st->zSpeed = 0x1E;
          }
        }
        actor->flags = 0;
        c = st->rock;
        if (c != 0 && c->subMode == 0) {
          c->subMode = 2;
          {
            BoulderState *r = c->state;

            st->rock = 0;
            r->heading = st->heading;
          }
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 3);
      }
      actor->flags = 0;

      if (st->command != 0) {
        if (st->command == 1) {
          st->command = 0;
          actor->mode = 0x28;
          continue;
        }
        if (actor->group != 0 && actor->subMode == 0) {
          continue;
        }
        actor->subMode++;
        st->path->dir = -1;
        while (AbsAngleDelta8(
                   actor->rotZ,
                   ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                                 FLY_Y(st->path, st->path->cur) - actor->posY,
                                 0)) >= 0x40) {
          st->path->cur++;
        }
        st->command = 0;
        actor->mode = 0x32;
        continue;
      }

      if (st->flags == 2) {
        if (st->path->cur != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box0) != 0) {
          st->path->cur = 0;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 0));
          SnapActorToGround(actor);
        } else if (st->path->cur != 1 &&
                   IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box1) != 0) {
          st->path->cur = 1;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 1));
          SnapActorToGround(actor);
        }
      }

      actor->flags = 0;
      switch (actor->mode) {
      case 0:
        if (actor->subMode != 0) {
          ENTER_POSE(actor, actor->subMode);
        }
        if (st->idlePose2 != 0) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 1:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->groupAlert != 0) {
          SetActorGroupSubstate(actor, 1);
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        if (st->flags & 1) {
          ENTER_POSE(actor, 5);
        }
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->flags & 1) &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < st->throwRange) {
          goto pose5;
        }
        if (st->box0HalfWidth != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box0) != 0) {
        pose5:
          ENTER_POSE(actor, 5);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < st->sightRange &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 3:
        if (DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0xC, 0xA) ==
                3 &&
            st->speed == 0 && st->landSoundPlayed == 0) {
          PlayActorMeshVoice(actor, 0, 0);
          st->landSoundPlayed = 1;
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        TrackEntityYawTowardSpyro(actor, 8, 0, 0);
        if ((unsigned)(actor->nextFrame - 3) < 5 && st->rock == 0) {
          st->rock = g_pfnLevelSpawnActor(ACTOR_TYPE_ROLLING_BOULDER, actor);
          st->rock->subMode = 0;
        }
        if (st->rock != 0 && st->rock->subMode == 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, &st->rock->posX);
          st->rock->rotZ = ArcTan2(spy->posX - st->rock->posX,
                                          spy->posY - st->rock->posY, 0);
          if (actor->nextFrame >= 0x10) {
            BoulderState *q = st->rock->state;

            q->life = 0;
            q->vx = (g_anCosineLut[st->rock->rotZ] * 0x4B) >> 10;
            q->vy = (g_anSineLut[st->rock->rotZ] * 0x4B) >> 10;
            q->vz = 0;
            st->rock->subMode = 1;
            st->rock = 0;
            q->spin[0] = RandomSignedInRange(7, 0xF);
            q->spin[1] = RandomSignedInRange(7, 0xF);
            q->spin[2] = RandomSignedInRange(7, 0xF);
          }
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          if (actor->type == ACTOR_TYPE_BOULDER_THROWER_B) {
            st->timer = 0x50;
          }
          st->rock = 0;
          ENTER_POSE(actor, 2);
        }
        break;

      case 5:
        st->flags &= ~1;
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1400) {
          ENTER_POSE(actor, 4);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, 5) ==
            0x100) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 40:
        ANIM_GO(actor, 2);
        if (actor->subMode == 0) {
          break;
        }
        st->orbitRadius = ApproxDist2D(&actor->posX, &g_pActorListBase[st->partner].posX);
        actor->mode = 0x29;
        continue;

      case 41: {
        int h = g_pActorListBase[st->partner].rotZ;

        ANIM_GO(actor, 5);
        if (SteerActorAroundPoint(actor, &g_pActorListBase[st->partner].posX, st->orbitRadius, h, 4,
                          0x64, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0) < 6) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->subMode = 0;
          actor->mode = 0x2A;
          continue;
        }
        break;
      }

      case 42:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->subMode != 0) {
          int d = ApproxDist2D(&actor->posX, &spy->posX);

          ANIM_GO(actor, 2);
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, 0);
          st->speed = 0x15E;
          st->zSpeed = d >> 7;
          if (d < 0x1800 ||
              (d < 0x2800 && !(SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E9
                                                 : -SPY_DZ(actor) < 0x3E9))) {
            st->zSpeed = 0;
          }
          if (st->zSpeed >= 0x6F) {
            st->zSpeed = 0x6E;
          }
          st->jitterA = RandomSignedInRange(8, 0xF);
          st->jitterB = RandomSignedInRange(8, 0xF);
          st->timer = 0xF0;
          actor->mode = 0x2B;
          continue;
        } else {
          int h;

          if (g_pActorListBase[st->partner].subMode == 0) {
            break;
          }
          h = g_pActorListBase[st->partner].rotZ;
          if (AbsAngleDelta8(
                  h, ArcTan2(actor->posX - g_pActorListBase[st->partner].posX,
                                   actor->posY - g_pActorListBase[st->partner].posY,
                                   0)) < 0xD) {
            break;
          }
          g_pActorListBase[st->partner].subMode = 0;
          actor->mode = 0x29;
          continue;
        }

      case 43: {
        int r;

        CopyVector(svC, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, 0x12);
        if (r == 3) {
          st->zSpeed = 0x50;
        }
        actor->updateDist = 0xFF;
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        ANIM_GO(actor, 7);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2BC &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x2BC
                               : -SPY_DZ(actor) < 0x2BC) &&
            g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
          int a = StepAngle8Toward(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                              g_nSpyroWorldPosY - actor->posY, 0),
                                st->heading, 0x20)
                  << 4;

          g_dwSpyroTriggerEventFlags |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
          g_anSpyroRequestVelocity.x = (LookupCosine(a) * 0x6E) >> 12;
          g_nSpyroRequestVelocityY = (LookupSine(a) * 0x6E) >> 12;
          CAM_LOOK.z = 0x28;
          st->heading = ArcTan2(actor->posX - g_anSpyroWorldPos,
                                  actor->posY - g_nSpyroWorldPosY, 0);
          st->speed = 0xDC;
          st->zSpeed = 0x3C;
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          ENTER_POSE(actor, 3);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apActiveActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == ACTOR_TYPE_PLATFORM_POD &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x384 &&
                AbsAngleDelta8(st->heading,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= ACTOR_HIT_BLAST | ACTOR_HIT_SUPER;
            }
          }
        }
        break;
      }

      case 50:
        if (st->partnerSignal != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case 51: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, 5);

        ANIM_GO(actor, 5);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case 52: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->heading = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  0);
          st->speed = 0x5A;
          st->zSpeed = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, 0),
                      6, 0, 0);
        break;
      }

      case 53: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        ANIM_GO(actor, 6);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, svD);
        GetActorAttachPoint(p, 1, throwHand1);
        AddVector(throwGripMid, svD, throwHand1);
        RShiftVector3(throwGripMid, 1);
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, sixteen);
        if (actor->posZ < throwGripMid[2]) {
          CopyVector(&actor->posX, throwGripMid);
          actor->mode++;
        }
        break;
      }

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, svE);
        GetActorAttachPoint(p, 1, svF);
        AddVector(throwCarryMid, svE, svF);
        RShiftVector3(throwCarryMid, 1);
        CopyVector(&actor->posX, throwCarryMid);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->releaseFlag = 0;
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, 0);
          st->speed = 0x1C2;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 101: gatekeeper. Guards a run of keys (g_pActorListBase actors) and
     * releases them as Spyro approaches; rides its partner and charges. */
    case ACTOR_TYPE_GATEKEEPER: {
      GatekeeperState *st = actor->state;
      int f = actor->flags;

      if ((f & 0xD0000) && actor->mode != 5) {
        if (f & 0x40000) {
          st->speed = 0;
          st->zSpeed = 0x28;
        } else {
          st->speed = 0xF0;
          st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        }
        if (actor->mode != 0x2B) {
          st->zSpeed = 0x1E;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 5);
      }

      if (st->command != 0) {
        if (st->command == 1) {
          st->command = 0;
          actor->mode = 0x28;
          continue;
        }
        if (actor->group != 0 && actor->subMode == 0) {
          continue;
        }
        actor->subMode++;
        st->path->dir = -1;
        while (AbsAngleDelta8(
                   actor->rotZ,
                   ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                                 FLY_Y(st->path, st->path->cur) - actor->posY,
                                 0)) >= 0x40) {
          st->path->cur++;
        }
        st->command = 0;
        actor->mode = 0x32;
        continue;
      }

      if (st->mode == 2) {
        if (st->path->cur != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box0) != 0) {
          st->path->cur = 0;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 0));
          SnapActorToGround(actor);
        } else if (st->path->cur != 1 &&
                   IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box1) != 0) {
          st->path->cur = 1;
          CopyVector(&actor->posX,
                        PATH_PT((unsigned char *)st->path, 1));
          SnapActorToGround(actor);
        }
      }

      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0 && st->idlePose2 == 0) {
          break;
        }
        if (actor->type == ACTOR_TYPE_GATEKEEPER_B) {
          actor->mode = 2;
          ANIM_RESET(actor, 2);
          continue;
        }
        ENTER_POSE(actor, 1);

      case 1:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->groupAlert != 0) {
          SetActorGroupSubstate(actor, 2);
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1C00) {
          ENTER_POSE(actor, 3);
        }
        break;

      case 3: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int dz = SPY_DZ(actor);

        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (d > 0x2400) {
          ENTER_POSE(actor, 2);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0xD48 &&
            (unsigned)(dz + 0x577) < 0x4AF) {
          ENTER_POSE(actor, 4);
        }
        break;
      }

      case 4: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if ((unsigned)(actor->nextFrame - 6) < 4) {
          int r = (actor->nextFrame - 5) * 0x3E8;

          if (r < 0x3E8) {
            r = 0x3E8;
          }
          if (r >= 0xDAD) {
            r = 0xDAC;
          }
          if (d < r && SPY_DZ(actor) < -0xC8) {
            g_dwSpyroTriggerEventFlags |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
            g_anSpyroRequestVelocity.x = (LookupCosine(actor->rotZ << 4) * 0x8C) >> 12;
            g_nSpyroRequestVelocityY = (LookupSine(actor->rotZ << 4) * 0x8C) >> 12;
            g_nSpyroRequestVelocityZ = 0x46;
          }
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          ENTER_POSE(actor, 3);
        }
        break;
      }

      case 5:
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0xE, 0xA);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x34);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 40:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (st->idx < st->count &&
            ApproxDist2D(&actor->posX, &spy->posX) <
                (st->range[0] + 6) << 10 &&
            actor->drawn != 0 &&
            (st->testFacing == -1 ||
             AbsAngleDelta8(st->testFacing,
                           ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, 0)) < 0x1C)) {
          g_pActorListBase[st->keys[st->idx]].subMode = 1;
          actor->mode++;
          break;
        }
        if (st->idx < st->count) {
          break;
        }
        ENTER_POSE(actor, 2);

      case 41:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (actor->subMode == 0) {
          break;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) >=
            st->range[st->idx] << 10) {
          break;
        }
        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - SPY.posY, 0)) >= 0x30) {
          break;
        }
        if (actor->drawn == 0) {
          break;
        }
        if (CheckSpyroLineOfSightTo(&actor->posX) == 0) {
          break;
        }
        actor->subMode = 0;
        actor->mode++;
        break;

      case 42: {
        int k;

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->nextFrame < 6) {
          break;
        }
        k = st->keys[st->idx];
        if (k == -1) {
          break;
        }
        g_pActorListBase[k].subMode = 1;
        st->keys[st->idx] = -1;
        if (++st->idx < st->count) {
          g_pActorListBase[st->keys[st->idx]].subMode = 1;
        }
        actor->mode = 0x2C;
        continue;
      }

      case 43: {
        int r;

        CopyVector(svC, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, 0x12);
        actor->updateDist = 0xFF;
        if (r == 3) {
          st->zSpeed = 0x5A;
        }
        ANIM_GO(actor, 8);
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x3E8 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x320
                               : -SPY_DZ(actor) < 0x320)) {
          int a = StepAngle8Toward(ArcTan2(spy->posX - actor->posX,
                                              spy->posY - actor->posY, 0),
                                st->heading, 0x20)
                  << 4;

          spy->triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
          spy->requestVelX = (LookupCosine(a) * 0x6E) >> 12;
          spy->requestVelY = (LookupSine(a) * 0x6E) >> 12;
          spy->requestVelZ = 0x28;
          st->heading = ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, 0);
          st->speed = 0xF0;
          st->zSpeed = 0x3C;
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          ENTER_POSE(actor, 5);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x34);
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apActiveActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == ACTOR_TYPE_PLATFORM_POD &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x4B0 &&
                AbsAngleDelta8(st->heading,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= ACTOR_HIT_BLAST | ACTOR_HIT_SUPER;
            }
          }
        }
        break;
      }

      case 44:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->idx < st->count) {
          actor->mode = 0x29;
          continue;
        }
        ENTER_POSE(actor, 2);

      case 50:
        if (st->partnerSignal != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case 51: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, 5);

        ANIM_GO(actor, 6);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case 52: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->heading = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  0);
          st->speed = 0x5A;
          st->zSpeed = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, 0),
                      6, 0, 0);
        break;
      }

      case 53: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        ANIM_GO(actor, 7);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, svD);
        GetActorAttachPoint(p, 1, keepHand1);
        AddVector(svE, svD, keepHand1);
        RShiftVector3(svE, 1);
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, sixteen);
        if (actor->posZ < svE[2]) {
          CopyVector(&actor->posX, svE);
          actor->mode++;
        }
        break;
      }

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, svG);
        GetActorAttachPoint(p, 1, keepCarryHand1);
        AddVector(keepCarryMid, svG, keepCarryHand1);
        RShiftVector3(keepCarryMid, 1);
        CopyVector(&actor->posX, keepCarryMid);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->releaseFlag = 0;
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, 0);
          st->speed = 0x1F4;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1:
    case ACTOR_TYPE_LIFE_CHEST_SHARD:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 1: {
      ShardState *st = actor->state;

      if (st->life > 0 && actor->drawn != 0) {

      if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
        int *nrm = &g_anCollisionProbeVec;
        int along;
        register int push asm("$7");

        SetVector3Magnitude(nrm, 0x1000);
        along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                (st->vel[2] * g_nCollisionProbeVecZ);
        push = along >> 11;

        if (push < 0) {
          ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - push);
          st->vel[0] += nrm[0];
          st->vel[1] += AS_I32(g_nCollisionProbeVecY);
          st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
        }
      }

      actor->posX += st->vel[0];
      actor->posY += st->vel[1];
      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
      }
      actor->posZ += st->vel[2];
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];

      if (!(st->life & 3)) {
        svC[0] = GetRandomU32() & 3;
        svC[1] = GetRandomU32() & 3;
        svC[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svC);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (int *)0x10);
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

      st->vel[2] -= 6;
      if (st->vel[2] < -0x80) {
        st->vel[2] = -0x80;
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

        if (st->anim != 255) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

          if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset5, g_aRotBillboards[st->anim].pos);
          } else {
            RotateVectorByMatrix(actor->mtx, &g_anRotBillboardOffset3, g_aRotBillboards[st->anim].pos);
          }

          AddVector(g_aRotBillboards[st->anim].pos, g_aRotBillboards[st->anim].pos,
                        &actor->posX);

          if (g_aRotBillboards[st->anim].life < 5) {
            st->anim = 255;
          }
        } else {
          if (st->timer > 0xf7) {
            int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);
            if (handle >= 0 && dist < 0x4000) {
              st->anim = handle;
            }

            st->timer = (GetRandomU32() & 0x1F) + 33;
          }
        }
      }

      if (!actor->drawn && dist > 0x2000 && actor->subMode == COLLECT_REST && st->sub == 0) {
        break;
      }

      switch (actor->subMode) {
      case COLLECT_SETTLE: {
        int *nrm = &g_anCollisionProbeVec;

        CopyVector(svF, &actor->posX);
        svF[2] += 0x400;
        FindGroundHeightBelow(svF, 0x10000);
        st->pitch = -ArcTan2_8bit(
            IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
            g_nCollisionProbeVecY);
        st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);

        if (st->pitch != 0 || st->yaw != 0) {
          actor->rotZ = 0;
        }

        actor->subMode = COLLECT_REST;
        break;
      }
      case COLLECT_REST: {
        if (st->sub != 0) {

          if (st->sub == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              int *nrm = &g_anCollisionProbeVec;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                  g_nCollisionProbeVecY);
              st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
            }

          } else if (st->sub == 2) {

            UnpackWorldCollisionTri(st->groundTri, pickupTri[0]);

            AddVector(pickupTri[0], pickupTri[0], pickupTri[1]);
            AddVector(pickupTri[0], pickupTri[0], pickupTri[2]);

            actor->posX = pickupTri[0][0] / 3;
            actor->posY = pickupTri[0][1] / 3;
            actor->posZ = pickupTri[0][2] / 3;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(svG, &actor->posX);
            svG[2] += 0x400;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(svG, 0x1000) > 0) {
              colIndex = g_nLastCollisionHitChunkId;
              st->sub = 2;
              st->groundTri = colIndex;
            }
          }
        }

        if (dist < 1434) {
          int zDist = (SPY.posZ - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0) {

                int res = CheckLineOfSightSegment(&g_pSparxActor->posX, &actor->posX);

                if (res) {
                  ChaserState *cs = (ChaserState *)g_pSparxActor->state;

                  if (cs->owner == 0) {
                    if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                       g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0])) {
                      g_nSfxOverrideFlags = 1;
                      g_nSfxOverrideVolR = 0x2000;
                      g_nSfxOverrideVolL = 0x2000;
                      PlayActorMeshVoice(g_pSparxActor, 0, 0);
                    }

                    g_pSparxActor->subMode = SPARX_SUB_FETCH;
                    cs->owner = actor;
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

        ScaleVector3Sat(svD, st->vel, g_nFrameStep);
        RShiftVector3(svD, 1);

        speed = VectorLength(svD, VEC_LEN_XYZ);

        if (220 < speed) {
          ScaleVector3ByRatio(svD, speed, 220);
        }

        AddVector(svD, &actor->posX, svD);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (svD[2] < 0) {
          if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          svD[2] += 240;

          if (CollideSphereWithWorldAndActors(svD, 240, 240)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(pickupFloorDir, &g_anCollisionProbeVec);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = FindGroundHeightBelow(svD, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(pickupFloorDir[2], VectorLength(pickupFloorDir, VEC_LEN_XY));

              if ((svD[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = COLLECT_REST;
                CopyVector(&actor->posX, svD);
                actor->posZ = groundHeight;

                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((pickupFloorDir[0] * pickupFloorDir[0]) + (pickupFloorDir[2] * pickupFloorDir[2])),
                      pickupFloorDir[1]);
                  st->yaw = -ArcTan2_8bit(pickupFloorDir[2], pickupFloorDir[0]);

                  if (st->pitch != 0 || st->yaw != 0) {
                    actor->rotZ = 0;
                  }
                }

                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->count++;
              }
            }

            if (st->count != 0) {
              g_nSfxOverrideFlags = 1;
              g_nSfxOverrideVolL = 0x3CCC >> (3 - st->count);
              g_nSfxOverrideVolR = 0x3CCC >> (3 - st->count);

              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);

              if (ReflectVectorOffNormal(st->vel, pickupFloorDir, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            svD[2] -= 240;
            CopyVector(&actor->posX, svD);
            FindGroundHeightBelow(svD, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }
        }

        if (dist < 1434) {
          int zDist = (((int *)&SPY.posX)[2] - 356) - actor->posZ;

          if (ABS2(zDist) < 512) {

            if (g_pSparxActor != 0) {

              if (SPY.airborneFrames == 0 && st->count < 3) {

                ChaserState *cs = (ChaserState *)g_pSparxActor->state;

                if (cs->owner == 0) {
                  if (!GetSoundVoiceStatusByOwner(g_pSparxActor,
                                     g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0])) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pSparxActor, 0, 0);
                  }

                  g_pSparxActor->subMode = SPARX_SUB_FETCH;
                  cs->owner = actor;
                }
              }
            }
          }
        }

        break;
      }
      case COLLECT_HOMING: {
        if (st->timer >= 32) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(pickupToSpyro, &spy->posX, st->vel);
          RShiftVector3(pickupToSpyro, 5);

          if (VectorLength(pickupToSpyro, VEC_LEN_XYZ) > 480) {
            CopyVector(&actor->posX, &spy->posX);
            st->timer = 32;
          } else {
            ScaleVector3Sat(pickupToSpyro, pickupToSpyro, st->timer);
            AddVector(&actor->posX, st->vel, pickupToSpyro);
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
      if ((st->timer > 32 && dist < 512 && (-384 < (SPY.posZ - actor->posZ)) &&
           ((SPY.posZ - actor->posZ) < 512)) ||
          (actor->subMode == COLLECT_HOMING && st->timer > 64)) {

        if (g_pSparxActor != 0) {
          ChaserState *cs = (ChaserState *)g_pSparxActor->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
          PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, (void *)8);
          g_pfnLevelEmitParticles(16, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);

          if (g_nExtraLives < 99) {
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
      SaveHostState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->armer];
      Actor *rider = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY:
        actor->collision = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = SAVEHOST_WAIT_ARMED;
        continue;

      case SAVEHOST_WAIT_ARMED:
        if (trigger->mode >= ACTOR_MODE_DORMANT) {
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 0x10;
          st->baseZ = actor->posZ;
        }
        break;

      case SAVEHOST_HIDDEN:
        if (st->timer != 0) {
          st->timer -= 1;
        } else {
          actor->collision = 0;
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
              if (st->prompted == 0 || rider->mode == 1) {
                int *at = &actor->posX;

                st->prompted = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                             spy->posY - actor->posY, 0) -
                               0x80;
                actor->cullRadius = 0x20;
                actor->mode = SAVEHOST_POP_IN;
                actor->scale = 0x60;
                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, at, (int *)0xC);
                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(at, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= ACTOR_GROUND_VALID;
              }
            }
          }
        }
        break;

      case SAVEHOST_POP_IN:
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ += 8;
        } else {
          actor->mode = SAVEHOST_HOVER;
          st->timer = 0;
          st->wanderTimer = 0x1E;
        }
        break;

      case SAVEHOST_HOVER: {
        int height;
        int drop;
        int close;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = SAVEHOST_POP_OUT;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2401 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             close = drop > 0 ? drop < 0xC01 : (SPY.posZ - height) < 0xC01,
             close == 0)) {
          actor->mode = SAVEHOST_POP_OUT;
          break;
        }

        st->wanderTimer -= 1;
        if (st->wanderTimer == 0) {
          int reach = (GetRandomU32() & 0x3F) + 0x140;
          int bearing = ArcTan2(spy->posX - rider->posX,
                                      spy->posY - rider->posY, 0);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          EBB_BREAK();
          CopyVector(st->wanderTo, &rider->posX);
          aim &= 0xFF;
          aim *= 0x10;
          actor->mode = SAVEHOST_WANDER;
          st->wanderTo[0] -= (LookupCosine(aim) * reach) >> 12;
          st->wanderTo[1] -= (LookupSine(aim) * reach) >> 12;
          st->faceYaw = ArcTan2(spy->posX - st->wanderTo[0],
                                  spy->posY - st->wanderTo[1], 0);
          st->wanderTimer = 0;
        }
        break;
      }

      case SAVEHOST_WANDER: {
        int height;
        int drop;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 ||
             (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = SAVEHOST_POP_OUT;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2801 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop >= 0xC01 : (SPY.posZ - height) >= 0xC01)) {
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
          FindGroundHeightBelow(here, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->wanderTimer == 0x10) {
            actor->mode = SAVEHOST_HOVER;
            st->wanderTimer = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }
        break;
      }

      case SAVEHOST_POP_OUT:
        if (actor->scale < 0x60) {
          actor->scale += 4;
          actor->rotZ += 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 0x10;
          g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (int *)0xC);
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
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, svF, 0);
        svF[0] = 0;
        svF[1] = -0x64;
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
            st->glow->size = 0x40;
            st->glow->offX = 0;
            st->glow->offY = 0;
            st->glow->offZ = 0;
            st->glow->r = 0xC0;
            st->glow->g = 0xC0;
            st->glow->b = 0x60;
            st->glow->kind = 9;
            st->glow->desc = &g_anSparkleGlowDesc;
          }
        } else {
          fx->size += 0x20;
          if (st->glow->size >= 0x401) {
            st->glow->size = 0x400;
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
          ApproxDist2D(&actor->posX, (int *)((char *)g_anSparxHealthBlock - 0x164)) >=
              0x2001) {
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

            CopyVector(svG, &st->target->posX);

            if (high == 0) {
              svG[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              svG[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              svG[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              svG[1] -=
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

            SubtractVector(svG, svG, &actor->posX);
            dist = VectorLength(svG, VEC_LEN_XYZ);

            if (high == 0 &&
                (TickCountdownByFrameDelta((void *)st, 4) != 0 ||
                 (st->timer < 0x50 &&
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
              ScaleVector3ByRatio(svG, dist, limit);
            } else {
              ScaleVector3ByRatio(svG, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, svG);
            TurnActorYawTowardAngle(actor, ArcTan2(svG[0], svG[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(svG, VEC_LEN_XY), svG[2], 0);
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

            if (dist < 0x200) {
              pk->timer = 0;
              pk->lean = GetRandomU32() & 0xE;
              pk->roll = GetRandomU32() & 0xE;
              pk->spin = GetRandomU32() & 0xE;
              CopyVector((int *)pk, &st->target->posX);
              st->target->subMode = COLLECT_HOMING;

              if (st->target->type == ACTOR_TYPE_EXTRA_LIFE) {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorType14);
              } else if (st->target->type == ACTOR_TYPE_SPARX_FOOD) {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_dwBurstColorType15);
              } else {
                g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, g_adwActorBurstColors[st->target->type]);
              }
              st->target = 0;
              EBB_BREAK();

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[SFX_GEM_BOUNCE]) == 0) {
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(sparxToTarget, dist, lim);
              } else {
                ScaleVector3ByRatio(sparxToTarget, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, sparxToTarget);
              TurnActorYawTowardAngle(actor, ArcTan2(sparxToTarget[0], sparxToTarget[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(sparxToTarget, VEC_LEN_XY), sparxToTarget[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* No target: hover at a jittered offset beside Spyro. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            svF[0] = -0x258;
            svF[1] = (GetRandomU32() & 0x310) - 0x188;
            svF[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + 0x64
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
              if (svF[2] < 0x64) {
                svF[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                svF[0] = st->drift[0] - 0x200;
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

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (svF[2] >= 0x21) {
              svF[2] = 0x20;
            }
            if (svF[2] < -0x20) {
              svF[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = svF[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, 0);
          }

          switch (SPY.sparxHealth) {
          case SPARX_GREEN:
            if (actor->nextAnim != 0 && actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
            break;

          case SPARX_BLUE:
            if (actor->nextAnim != 2 && actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
            break;

          case SPARX_GOLD:
            if (actor->nextAnim != 4 && actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
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

        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
            int i;

            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
            RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)(0x8080 + (i << 24)));
            }

            g_nLevelKeyState = KEYSTATE_CARRIED;
            actor->scale = 0x40;
            actor->cullRadius = 0;
            actor->updateDist = 0;
            actor->drawn = 0;
            actor->groundCode = 0;
            actor->visZone = 0xFF;
            actor->subMode = KEY_TAKEN;
          }
        }

        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          RotateVectorByMatrix(&actor->mtx[0], &g_anRotBillboardOffset4,
                        g_aRotBillboards[st->attach].pos);
          AddVector(g_aRotBillboards[st->attach].pos, g_aRotBillboards[st->attach].pos,
                        &actor->posX);
          if (g_aRotBillboards[st->attach].life < 5) {
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset4);

          if (handle >= 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x4000) {
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
        st->shake = 0xD2;
      }

      if (actor->subMode < LOCKCHEST_OPENING) {
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
          key->visZone = 0xFF;
          key->subMode = KEY_TAKEN;
        }
        break;

      case LOCKCHEST_KEY_READY: {
        int height;
        int drop;

        if (g_nLevelKeyState == KEYSTATE_CARRIED &&
            (SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
             SPY.state == SPYRO_ST_RUN) &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x600) {
          height = actor->posZ - actor->floorOffset;
          drop = height - SPY.posZ;
          if ((drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
              AbsAngleDelta8(spy->bodyRotZ,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - spy->posY, 0)) < 0x30 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0)) < 0x30) {
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
          st->shake = 0xD2;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        key->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nGameplayBlocked = 1;

        if (st->shake < 0xC0) {
          cageAnchor[0] = 0x200;
          cageAnchor[1] = 0;
          cageAnchor[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, cageAnchor, cageAnchor);
          AddVector(cageAnchor, cageAnchor, &actor->posX);
          SubtractVector(cageAnchor, cageAnchor, &spy->posX);
          cageAnchor[2] -= 0x100;
          RShiftVector3(cageAnchor, 5);
          ScaleVector3Sat(cageAnchor, cageAnchor, st->shake);
          DivideVector3(cageAnchor, cageAnchor, 6);
          AddVector(cageAnchor, cageAnchor, &spy->posX);
          cageAnchor[2] += 0x100;
          cageAnchor[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, cageAnchor);
          g_nCameraScriptStickRequestY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

          if (st->shake < 0x40) {
            int rot;

            key->rotY = -st->shake;
            rot = g_bSpyroPersistentEulerZ + 0x80;
            key->rotZ = rot + st->shake * 4;
          } else if (st->shake < 0x80) {
            int rot;

            key->rotY = 0xC0;
            rot = g_bSpyroPersistentEulerZ + 0x80;

            key->rotZ = rot + (st->shake - 0x40) * 3;
          } else {
            int turn = AbsAngleDelta8(actor->rotZ,
                                     (unsigned char)(g_bSpyroPersistentEulerZ + 0x40)) *
                       (st->shake - 0x80);
            int rot = g_bSpyroPersistentEulerZ + 0x40;

            key->rotZ = rot + turn / 64;
          }
        } else if (st->shake < 0xD0) {
          key->rotZ = actor->rotZ;
          cageShakeAnchor[0] = 0x200 - (st->shake - 0xC0) * 8;
          cageShakeAnchor[1] = 0;
          cageShakeAnchor[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, cageShakeAnchor, cageShakeAnchor);
          AddVector(&key->posX, cageShakeAnchor, &actor->posX);
        }

        if (st->shake >= 0xD0 ||
            !(SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
              SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          int i;

          key->cullRadius = 0;
          key->updateDist = 0;
          key->drawn = 0;
          g_nGameplayBlocked = 0;
          actor->soundRange = 0x20;
          PlayActorMeshVoice(actor, 0, 0);

          for (i = 0; i < 4; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
          }

          for (i = 0; i < 10; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          }

          g_pfnLevelEmitParticles(0x20, EMIT_STREAK_BURST, &actor->posX, (void *)0x18);
          g_pLockedChestReward = SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          g_nLevelKeyState = KEYSTATE_CHEST_OPENED;
          g_nCameraScriptStickRequestX = 0;
          g_nCameraScriptStickRequestY[0] = 0;
          g_nSpyroSwingEulerPitchTarget = 0;
        }

        RefreshActorDerivedState(key, ACTOR_REFRESH_MATRIX);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)key, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, (int *)key, (void *)0x5008080);
        }
        break;
      }

      st->handle = SelectActorLodBand(actor, st->handle);
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

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 1, actor);
      }
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 2, actor);
      }

      g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
      SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
      MarkActorCommittedKill(actor);
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

        if (phase < 0x40) {
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
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
        }

        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);

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
          g_pActorListBase[st->partner].mode = 3;
        }
        actor->mode = STATUE_IDLE;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->mode == STATUE_IDLE) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x101) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if (st->phase >= 0xC0) {
          int dx;
          int dy;

          if (GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0]) == 0) {
            PlayActorMeshVoice(actor, 0, 0);
          }

          actor->rotX = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b + st->rotY;

          dx = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].a;
          dy = (signed char)g_aShakeRotOffsets[(st->phase - 0xC0) >> 1].b;
          if (dx < 0) {
            dx = -dx;
          }
          if (dy < 0) {
            dy = -dy;
          }
          dx += dy;
          actor->posZ = st->restZ + dx * 6;
        } else if (st->phase >= 0xBC) {
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        } else if (actor->flags != 0) {
          st->phase = 0xBC;
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        } else if ((st->phase >> 1) == 0x10 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset0);
        } else if ((st->phase >> 1) == 0x30 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset1);
        } else if ((st->phase >> 1) == 0x50 && !(GetRandomU32() & 3)) {
          SpawnActorRotBillboard(actor, &g_anRotBillboardOffset2);
        }

        at = &actor->posX;
        if (ApproxDist2D(at, &spy->posX) < 0x800) {
          SubtractVector(svH, at, &spy->posX);
          svH[2] = (svH[2] * 3) >> 2;

          if (VectorLength(svH, VEC_LEN_XYZ) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->facing;
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, facing);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIndex] += 1;
              g_nWorldDragonsRescued += 1;
              StubRewardHook(st->reward, 0);

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->cutsceneId == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIndex] += 1;
              g_nWorldDragonsRescued += 1;

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
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
    case ACTOR_TYPE_STATUE_SHARD:      TickStatueBurstShard(actor);
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

      if (!(actor->cullRadius & 0x80) && (actor->cullRadius != 0)) {

        if (st->life >= 1) {

          actor->rotZ += 1;

          st->vel[2] -= 6;
          if (st->vel[2] < -0x80) {
            st->vel[2] = -0x80;
          }

          actor->posX += st->vel[0];
          actor->posY += st->vel[1];
          actor->posZ += st->vel[2];

          if (actor->posZ > 1023) {
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
              int *nrm = &g_anCollisionProbeVec;
              int dot;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);

              SetVector3Magnitude(nrm, 0x1000);

              dot = (st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionProbeVecY +
                     st->vel[2] * g_nCollisionProbeVecZ) >>
                    11;

              if (dot < 0) {
                ScaleVector3ByRatio(nrm, 0x1000, -dot);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionProbeVecY);
                st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
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

    /* Types 293/294: path runner. Runs its path ahead of Spyro and raises
     * its goal flag on arrival; struck, it is knocked away. */
    case ACTOR_TYPE_PATH_RUNNER:
    case ACTOR_TYPE_PATH_RUNNER + 1: {
      PathRunnerState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode >= 3 &&
          actor->mode != 4) {
        st->knock = 0x3C;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knock = 0x55;
        }
        st->lift = 0x3C;
        st->knockAng = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                   actor->posY - spy->posY, 0),
                                     spy->bodyRotZ, 0x20, 0x40);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }
      actor->flags = 0;

      if (st->init == 0) {
        st->init = 1;
        if (IsActorInRunningKillSet(actor) != 0) {
          if (st->mode == 0) {
            actor->group = 0;
            SetActorGroupSubstate(actor, 2);
            actor->group = 1;
            SetActorGroupSubstate(actor, 2);
          } else if (st->mode == 1) {
            actor->group = 2;
            SetActorGroupSubstate(actor, 2);
          }
          actor->group = 0xFF;
          DespawnActorRecord(actor);
          continue;
        }
      }

      if (st->mode == 0 && st->pathInit == 0) {
        st->pathInit = 1;
        if (g_nSpawnAnchorValid[0] != 0) {
          st->camTimer = 0;
        }
        if (IsPointInRotatedBox(&spy->posX, st->volB) != 0) {
          actor->group = 0;
          SetActorGroupSubstate(actor, 2);
          actor->group = 1;
          SetActorGroupSubstate(actor, 2);
          actor->group = 0xFF;
          actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                       SPY.posY - actor->posY, 0);
          st->camTimer = 0;
          if (g_anLevelRunnerArrived[st->mode] != 0) {
            CopyVector(&actor->posX,
                          (int *)(st->path + (st->path[0] * 16 - 8)));
            ENTER_POSE(actor, 3);
          }
          st->path = st->home;
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
      }
      if (st->mode == 1 && st->pathInit == 0) {
        st->pathInit = 1;
        if (IsPointInRotatedBox(&spy->posX, st->volB) != 0) {
          actor->group = 2;
          SetActorGroupSubstate(actor, 2);
          actor->group = 0xFF;
          actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                       SPY.posY - actor->posY, 0);
          if (g_anLevelRunnerArrived[st->mode] != 0) {
            CopyVector(&actor->posX, PATH_PT(st->home, 0));
            SnapActorToGround(actor);
            ENTER_POSE(actor, 3);
          }
          st->path = st->home;
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
        actor->mode = 0x14;
        continue;
      }

      {
        int r = 1;

        if (st->camTimer != 0) {
          if (g_nSpyroAirborneFrames == 0) {
            r = TickCountdownByFrameDelta(&st->camTimer, 4);
            st->camDone = 1;
          }
          if (st->camDone == 0) {
            continue;
          }
        }
        if (r == 0) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          g_nGameplayBlocked = 1;
        } else if (r == 2) {
          g_nGameplayBlocked = 0;
        }
      }

      if (actor->nextAnim == 4 && actor->cullRadius != 0) {
        if (actor->nextFrame == 1 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 10 && !(GetRandomU32() & 3) &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case 0: {
        int spd;
        int t;
        int r;
        int floor;
        int z;

        t = RUN_FLAGS(st->path) & 0xFFC;
        spd = 0xB4;
        if (t != 0) {
          spd = t;
        }
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1400 && spd < 0xAA) {
          spd = 0xAA;
        }
        r = PivotActorAlongPath(actor, st->path, 0x17C, spd, 0, 8, 0x28, 0xFF, 0);
        floor = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->lift -= 0x1E;
        if (st->lift < -0x190) {
          st->lift = -0x190;
        }
        z = actor->posZ + st->lift;
        if (z < floor) {
          actor->posZ = floor;
        } else {
          actor->posZ = z;
        }
        if (st->mode == 0 && r == 0x11C &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x1C00 &&
            ABS2(SPY_DZ(actor)) < 0x898) {
          g_anLevelRunnerArrived[st->mode] = 1;
        }
        if (st->mode == 1 && r == 0x10A &&
            ApproxDist2D(&actor->posX, &spy->posX) < 0x1D4C &&
            ABS2(SPY_DZ(actor)) < 0xBB8) {
          g_anLevelRunnerArrived[st->mode] = 1;
        }
        if (r == 0x100) {
          if (st->mode == 1) {
            if (g_anLevelRunnerArrived[1] == 0) {
              st->path = st->home;
              st->path[1] = 0;
              actor->mode = 4;
              continue;
            }
            actor->mode = 10;
            continue;
          } else if (st->mode == 0) {
            if (g_anLevelRunnerArrived[0] != 0) {
              actor->mode = 3;
              continue;
            }
            st->path = st->home;
            st->path[1] = 0;
            actor->mode = 4;
            continue;
          }
        } else if (r & 0x100) {
          switch (RUN_FLAGS(st->path) & 3) {
          case 1:
            st->lift = 0xDC;
            break;
          case 2:
            st->lift = 0x140;
            break;
          }
        }
        if (st->target != -1 && actor->posZ == floor &&
            ApproxDist2D(&actor->posX, &g_pActorListBase[st->target].posX) <
                0x9C4) {
          actor->mode = 1;
          continue;
        }
        continue;
      }

      case 1:
        ANIM_GO(actor, 3);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                    g_pActorListBase[st->target].posY - actor->posY,
                                    0),
                      6, 0, 0);
        if (g_nActorAnimDone != 0) {
          g_pActorListBase[st->target].subMode = 1;
          st->target = *(int *)g_pActorListBase[st->target].state;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 2:
        DriveActorTrackedMove(actor, &st->knock, st->knockAng, &st->lift, 0xC, 0xC);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 3:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < st->range &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - SPY.posY, 0)) < 0x20) {
          st->path = st->home;
          st->path[1] = 0;
          st->lift = 0;
          actor->mode = 4;
          continue;
        }
        continue;

      case 4: {
        int spd;
        int t;
        int r;
        int floor;
        int z;
        int last;

        ANIM_GO(actor, 0);
        t = RUN_FLAGS(st->path) & 0xFFC;
        spd = 0xF0;
        if (t != 0) {
          spd = t;
        }
        r = PivotActorAlongPath(actor, st->path, 0x17C, spd, 0, 10, 0x28, 0xFF, 0);
        floor = ProbeActorGroundHeightRaised(actor, 0xBB8);
        st->lift -= 10;
        if (st->lift < -0x190) {
          st->lift = -0x190;
        }
        z = actor->posZ + st->lift;
        if (z < floor) {
          actor->posZ = floor;
        } else {
          actor->posZ = z;
        }
        if (spd > 0x118) {
          int k = 0;

          do {
            int a = (actor->rotZ + RandomIntInRange(-7, 7)) & 0xFF;
            CopyVector(runnerSpark, &actor->posX);
            k++;
            runnerSpark[2] += RandomIntInRange(0, 0x14);
            runnerSpark[3] = (g_anCosineLut[a] * 15) >> 10;
            runnerSpark[4] = (g_anSineLut[a] * 15) >> 10;
            runnerSpark[5] = 10;
            CopyVector(svH, &runnerSpark[3]);
            svH[2] = 2;
            LShiftVector3(svH, 3);
            AddVector(runnerSpark, runnerSpark, svH);
            runnerSpark[6] = spd;
            g_pfnLevelEmitParticles(1, EMIT_HEAT_SPARK, runnerSpark, 0);
          } while (k < 3);
        }
        last = st->mode < 2 ? st->path[0] - 1 : 0;
        if (r == 0x103 && st->mode == 0 && g_anLevelRunnerArrived[0] == 0) {
          st->path[1] = st->path[0] - 1;
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 0x18)));
          SnapActorToGround(actor);
          actor->mode = 5;
          continue;
        }
        if (r == last + 0x100) {
          if (st->mode < 2) {
            st->path[1] = st->path[0] - 1;
            actor->mode = 5;
            continue;
          }
          st->range = 0;
          actor->mode = 3;
          continue;
        }
        if (r & 0x100) {
          switch (RUN_FLAGS(st->path) & 3) {
          case 1:
            st->lift = 0x17C;
            break;
          case 2:
            if (st->mode == 0) {
              st->lift = 0x104;
            } else if (st->mode == 1) {
              st->lift = 0xD2;
            }
            break;
          case 3:
            st->lift = 0xC8;
            break;
          }
        }
        continue;
      }

      case 5: {
        int r;

        ANIM_GO(actor, 0);
        r = SteerActorAroundPoint(
            actor, PATH_PT(st->path, st->path[1]), st->speed,
            (ArcTan2(spy->posX - PATH_X(st->path, st->path[1]),
                           spy->posY - PATH_Y(st->path, st->path[1]), 0) +
             0x80) &
                0xFF,
            0x1E, 0x64, 0xF, 0x20, 0xFF, 0xFF, 0, 0, 0);
        SnapActorToGround(actor);
        if (r < 0x23 && spy->state != SPYRO_ST_CHARGE && spy->state != SPYRO_ST_CHARGE_AIR) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            actor->mode = 6;
            continue;
          }
          continue;
        }
        st->timer = 0x78;
        continue;
      }

      case 6: {
        int r;

        ANIM_GO(actor, 0);
        r = SteerActorAroundPoint(
            actor, PATH_PT(st->path, st->path[1]), st->speed,
            ArcTan2(spy->posX - PATH_X(st->path, st->path[1]),
                          spy->posY - PATH_Y(st->path, st->path[1]), 0),
            5, 0xC8, 0xF, 0x20, 0xFF, 0xFF, 0, 0, 0);
        SnapActorToGround(actor);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0xFA0) {
          actor->mode = 5;
          continue;
        }
        if (g_nSpyroState == SPYRO_ST_CHARGE || g_nSpyroState == SPYRO_ST_CHARGE_AIR) {
          actor->mode = 5;
          continue;
        }
        if (r < 0x46) {
          actor->mode = 7;
          continue;
        }
        continue;
      }

      case 7:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        ANIM_GO(actor, 4);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x12C0) {
          actor->mode = 5;
          continue;
        }
        if (g_nSpyroState == SPYRO_ST_CHARGE || g_nSpyroState == SPYRO_ST_CHARGE_AIR) {
          actor->mode = 5;
          continue;
        }
        continue;

      case 10:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (SPY_DZ(actor) < 0 &&
            ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1964) {
          actor->mode = 11;
          continue;
        }
        continue;

      case 11:
        ANIM_GO(actor, 0);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(PATH_X(st->home, 0) - actor->posX,
                                        PATH_Y(st->home, 0) - actor->posY, 0),
                          8, 0x14, 1) != 0) {
          if (ApproxDist2D(&actor->posX, PATH_PT(st->home, 0)) < 0x12C) {
            actor->mode = 3;
            continue;
          }
          MoveActorAlongHeading(actor, 0x118, 0, 0, 5);
        }
        continue;

      case 20:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->volA) != 0) {
          ENTER_POSE(actor, 0);
        }
        continue;
      }
      continue;
    }

    /* Whirlwind (WhirlwindState): lifts Spyro to its top (SPYRO_EV_LIFT). */
    case ACTOR_TYPE_WHIRLWIND: {
      WhirlwindState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (WhirlwindState *)actor->state;
      if (st->flags & 8) {
        if (g_pActorListBase[st->idx].type == ACTOR_TYPE_KEY) {
          if (g_pActorListBase[st->idx].subMode == 0)
            break;
          if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1C01)
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
      if (flags & 2) {
        if (st->timer != 0) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0)
            st->flags = (st->flags & ~2) | 4;
          break;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, SFX_MODE_3D, &actor->voiceRow);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_PLUME, (int *)actor, 0);
      SubtractVector(triggerToSpyro, &spy->posX, &actor->posX);
      distance = VectorLength(triggerToSpyro, VEC_LEN_XY);
      if (distance < st->radius && triggerToSpyro[2] >= -0x3FF && triggerToSpyro[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(spy->firstContactPoint, &actor->posX);
        if (st->flags & 1) {
          spyro->requestMask |= SPYRO_RQ_TARGET_CAMERA;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamMode2Front;
          SPY.pCamAnchor = &spy->posX;
          SPY.pCamTarget = &g_anCameraActiveTargetParams;
          spyro->requestMask |= SPYRO_RQ_CAMERA;
          g_anCameraActiveTargetParams.w[0] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
        }
        st->timer = 0x78;
        break;
      }
      if ((st->flags & ~1) == 4 && st->timer != 0 &&
          TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~4) | 2;
      }
      break;
    }

    /* Sparkling shard: 1 puff per frame. */
    case ACTOR_TYPE_SPARKLE_SHARD: {
      ShardState *st = actor->state;

      if (st->life != 0 && actor->drawn != 0) {
        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];

        svH[0] = (GetRandomU32() & 0xFE) - 0x7F;
        svH[1] = (GetRandomU32() & 0xFE) - 0x7F;
        svH[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(svH, svH, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, svH, (int *)1);
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
      ShardState *st = actor->state;
      register int i asm("$16");

      if (st->life > 0) {
        if (actor->drawn == 0) {
          DespawnActorRecord(actor);
          continue;
        }

        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
          int *nrm = &g_anCollisionProbeVec;
          int along;

          SetVector3Magnitude(nrm, 0x1000);
          along = (st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                  (st->vel[2] * g_nCollisionProbeVecZ);
          i = along >> 11;

          if (i < 0) {
            ScaleVector3ByRatio(nrm, 0x1000, (along >> 13) - i);
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
          }
        }

        actor->posX += st->vel[0];
        actor->posY += st->vel[1];
        st->vel[2] -= 6;
        if (st->vel[2] < -0x80) {
          st->vel[2] = -0x80;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin[0];
        i = 0;
        {
          actor->rotY += st->spin[1];
          actor->rotZ += st->spin[2];

          do {
            i += 1;
            shardSparkPos[0] = (GetRandomU32() & 0xFE) - 0x7F;
            shardSparkPos[1] = (GetRandomU32() & 0xFE) - 0x7F;
            shardSparkPos[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(shardSparkPos, shardSparkPos, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, shardSparkPos, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (int *)0x10);
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Respawner (RespawnState): puts its type's dormant actors back once Spyro is far away. */
    case ACTOR_TYPE_RESPAWNER: {
      RespawnState *st = actor->state;

      CopyVector(&actor->posX, &SPY.posX);

      if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        Actor *scan;

        st->timer = st->period;

        for (scan = g_pActorListBase; scan < g_pActorPoolDynBase; scan++) {
          if (scan->type == st->type && scan->mode >= ACTOR_MODE_DORMANT &&
              ApproxDist2D(&SPY.posX, scan->state) >= 0x6001) {
            CopyVector(&scan->posX, scan->state);
            InsertActorIntoSpatialGrid(scan);
            scan->flags = 0;
            scan->mode = 0;
            scan->killGroup &= ACTOR_KILL_GROUP_MASK;
            ANIM_RESET(scan, 0);
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

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case SEEDPOD_CLOSED:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
          actor->subMode = 1;
          actor->updateDist = 0x10;
        }
        if (actor->flags & ACTOR_HITS_SPYRO) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = 0x40;
          st->prize = SpawnActorDeathDrop(actor, DROP_IN_PLACE, 0, 0);
          st->prize->subMode = COLLECT_INERT;
          st->prize->cullRadius = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, EMIT_GEM_COLOR_BURST, &actor->posX, (void *)(st->prize->type - 0x53));
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = SEEDPOD_OPEN;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          SpawnActorRotBillboard(actor, g_anRotBillboardOffsetsRand[GetRandomU32() & 3]);
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
        break;

      case SEEDPOD_OPEN: {
        int i;

        st->prize->cullRadius = 0x18;
        CopyVector(&st->prize->posX, &actor->posX);
        st->prize->posZ += LookupSine((st->timer << 11) / 90) / 3 + 0x1A4;
        st->prize->rotZ += g_nFrameStep * 2;
        if (st->timer < 0x40) {
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
        if (st->timer < 0x20 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          ZeroVector(svI);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, svI);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == SPYRO_ST_SUPERCHARGE || g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
            (g_nSpyroState == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & ACTOR_HIT_SUPER) {
          st->prize->subMode = COLLECT_BOUNCE;
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SEED_POD_SHARD + 2, actor);
          }
          g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        st->timer += g_nFrameStep;
        if (st->timer >= 0x56) {
          int j;

          for (j = 0; j < 6; j++) {
            CopyVector(svJ, &st->prize->posX);
            svJ[0] += g_anCosineLut[j * 43] >> 6;
            svJ[1] += g_anSineLut[j * 43] >> 6;
            svJ[2] += 0x28;
            podSparkVel[0] = g_anCosineLut[j * 43] >> 7;
            podSparkVel[1] = g_anSineLut[j * 43] >> 7;
            podSparkVel[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, svJ, podSparkVel);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= ACTOR_KILL_GROUP_MASK;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, 1, 0);
          actor->mode = SEEDPOD_CLOSED;
          ANIM_RESET(actor, 0);
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
    case ACTOR_TYPE_CHECKPOINT:      switch (actor->mode) {
      case 0:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1: {
        int height;
        int drop;

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x380) {
          break;
        }

        height = actor->posZ - actor->floorOffset;
        drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = 2;
          actor->anim = 1;
          actor->subMode = 0;
          continue;
        }
        break;
      }

      case 2: {
        int d = g_nFrameStep;
        int tick = actor->subMode + d;

        actor->subMode = tick;
        if ((unsigned char)tick < 0x30) {
          break;
        }
        actor->mode = 0;
        actor->anim = 0;
        continue;
      }
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case ACTOR_TYPE_SPIN_CHEST: {
      SpinChestState *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
      int cur = g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0];
      int i;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, SFX_MODE_3D,
                      &actor->voiceRow);
        if (actor->voiceRow != VOICE_ROW_NONE) {
          g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
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
          st->timer = 0x10;
          st->speed += 0x64;
        }
      }
      st->handle = SelectActorLodBand(actor, st->handle);
      st->speed -= 1;
      if (pitch < cur && actor->voiceRow != VOICE_ROW_NONE) {
        g_anSpuVoiceRowPitchStep[actor->voiceRow & 0x7F][0] = -4;
      }
      if (st->speed < 0x18) {
        st->speed = 0x18;
        actor->updateDist = 0x10;
      } else {
        actor->updateDist = 0x40;
      }
      st->angle += (st->speed * g_nFrameStep) >> 1;
      st->lid->rotZ = st->angle >> 4;
      if (st->speed > 0xE0 || (actor->flags & ACTOR_HIT_SUPER)) {
        st->lid->subMode = LID_LAUNCH;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
        }
        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
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
        int bestAng = 0x180;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= ACTOR_MODE_DORMANT || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(svI, &a->posX, &actor->posX);
          d = ABS2(svI[0]) + ABS2(svI[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(svI, VEC_LEN_XY) - 0x1000) <= 0x2000 &&
              (unsigned int)(svI[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(svI[0], svI[1], 1));
            if (d < bestAng) {
              bestAng = d;
              best = a;
            }
          }
        }
        if (best != 0) {
          SubtractVector(st->vel, &best->posX, &actor->posX);
          st->vel[2] += 0x200;
          RShiftVector3(st->vel, 7);
        } else {
          SubtractVector(st->vel, &actor->posX, &SPY.posX);
          st->vel[2] = 0;
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, VEC_LEN_XY), 0x50);
        }
        st->vel[2] += 0xA0;
        st->timer = 0;
        actor->subMode = LID_FLYING;
        actor->cullRadius = 0x20;
        actor->updateDist = 0xFF;
        actor->rotX = 0xA;
        actor->rotY = 0;
        continue;
      }
      if (actor->subMode != LID_FLYING) {
        continue;
      }
      st->timer += g_nFrameStep;
      actor->rotZ += g_nFrameStep * 8;
      if (st->vel[2] >= -0x9F) {
        st->vel[2] -= (g_nFrameStep * 5) >> 1;
      }
      for (i = 0; i < g_nFrameStep; i++) {
        AddVector(&actor->posX, &actor->posX, st->vel);
        if (actor->posZ < 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += 0xB4;
        if (st->timer >= 5 &&
            (CollideSphereWithWorldAndActors(&actor->posX, 0xB4, 0xB4) != 0 ||
             DispatchActorContactAtSphere(&actor->posX, 0x100, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, 0) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, 0x400, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_CHARGE), actor, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
            g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          }
          g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x18);
          PlayActorMeshVoice(actor, 0, 0);
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0xB4;
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
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 4;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD, actor);
          i++;
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_SHARD + 1, actor);
        }
        for (i = 0; i < 0xA;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPARKLE_SHARD, actor);
          i++;
        }
        g_pfnLevelEmitParticles(0x20, EMIT_STREAK_BURST, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);
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

    /* Life chest (LiftState). */
    case ACTOR_TYPE_LIFE_CHEST: {
      LiftState *st = actor->state;
      Actor *rider;
      int i;
      int a;
      int b;

      if (st->rideMode == 1) {
        SnapActorToGround(actor);
        EncodeCachedVecToActorDirCode(actor);
      }

      if (actor->mode == LIFECHEST_CLOSED) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x1E1) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->mode = LIFECHEST_OPEN;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = 1;

          st->passenger = g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_RIDER, actor);
          st->passenger->rotX = actor->rotX;
          st->passenger->rotY = actor->rotY;
          st->passenger->rotZ = actor->rotZ;
          st->passenger->frameRate = 0x10;
          st->passenger->updateDist = 0;
          st->passenger->cullRadius = 0;
          st->passenger->drawn = 0;
          st->passenger->otBias = 5;

          PlayActorMeshVoice(actor, 0, 0);
        } else if (st->phase >= 0x1A0) {
          actor->rotX = g_aShakeRotOffsets[LIFT_STEP(st)].a + st->savedRotX;
          actor->rotY = g_aShakeRotOffsets[LIFT_STEP(st)].b + st->savedRotY;
          a = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].a;
          b = (signed char)g_aShakeRotOffsets[LIFT_STEP(st)].b;
          if (a < 0) {
            a = -a;
          }
          if (b < 0) {
            b = -b;
          }
          actor->posZ = st->savedPosZ + (a + b) * 6;

          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, 1, 0);
          }
        } else if (st->phase >= 0x19C) {
          /* Just before the shake: remember the rest pose. */
          st->savedRotX = actor->rotX;
          st->savedRotY = actor->rotY;
          st->savedPosZ = actor->posZ;
          if (st->rideMode == 1) {
            st->phase = 0;
          }
        }
      } else {
        if (actor->anim == 1) {
          st->phase += g_nFrameStep;
          st->passenger->cullRadius = 0x10;
          st->passenger->updateDist = 0xFF;
        }

        if (st->phase >= 0xE6) {
          DespawnActorRecord(st->passenger);
          st->phase = GetRandomU32() & 0xFF;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->mode = LIFECHEST_CLOSED;
          actor->nextAnim = 0;
          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, 3, 0);
          }
        }
      }

      if ((actor->flags & ACTOR_HITS_SPYRO) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, DROP_IN_PLACE, 0, 0);
      if (st->rideMode == 1) {
        ((CollectibleState *)rider->state)->mode = st->rideMode;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 4, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD, actor);
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD + 1, actor);
      }

      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(ACTOR_TYPE_LIFE_CHEST_SHARD + 2, actor);
      }

      for (i = 0; i < 8; i++) {
        svJ[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        svJ[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        svJ[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, svJ);
      }

      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x18);

      if (actor->mode == LIFECHEST_OPEN) {
        DespawnActorRecord(st->passenger);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[2], actor, SFX_MODE_3D,
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

      if (st->parent->mode == 0 || st->parent->subMode != actor->mode) {
        DespawnActorRecord(actor);
        continue;
      }

      if (st->parent->type == ACTOR_TYPE_LEVEL_EXIT) {
        if (ApproxDist2D(&st->parent->posX, &spy->posX) < 0x400) {
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
          bearing = ArcTan2(spy->posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, 0);
          svJ[0] = (g_anCosineLut[bearing] * 3) >> 4;
          svJ[1] = (g_anSineLut[bearing] * 3) >> 4;
          svJ[2] = 0;
          orbitDir[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          orbitDir[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          orbitDir[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(orbitSwing, orbitDir, st->radius - 1);
          LShiftVector3(orbitDir, 1);
          arm = st->radius - 1;
          orbitSwing[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          orbitSway[0] = svJ[0] * g_anCosineLut[arm * 2];
          orbitSway[1] = svJ[1] * g_anCosineLut[arm * 2];
          orbitSway[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(orbitDir, orbitDir, st->phase);
          SubtractVector(orbitSwing, orbitSwing, orbitDir);
          swing = &g_anCosineLut[bearing];
          actor->posX = svJ[0] * *swing;
          actor->posY = svJ[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, orbitSway);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, svJ);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, orbitSwing);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* Type 453: roaming ram. Wanders its waypoint ring away from Spyro,
     * charges down its path once he is close, and is knocked back by hits
     * or by its partner. */
    case ACTOR_TYPE_ROAMING_RAM_QUIET: {
      RoamingRamState *st = actor->state;

      if ((actor->flags & ACTOR_HIT_ANY) && actor->mode != 2) {
        if (actor->flags & ACTOR_HIT_BLAST) {
          st->knock = ArcTan2(actor->posX - g_pActorListBase[st->target].posX,
                                    actor->posY - g_pActorListBase[st->target].posY,
                                    0);
          st->speed = 0x8C;
          st->lift = 0x23;
        } else {
          st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    spy->bodyRotZ, 0x20, 0x40);
          st->speed = 0xC8;
          st->lift = 0x32;
          if (actor->flags & ACTOR_HIT_CHARGE) {
            st->speed += 0x46;
            st->lift += 0x32;
          }
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorCommittedKill(actor);
        }
        if (actor->nextAnim != 2) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 2;
          actor->nextFrame = 0;
          OnActorAnimQueued(actor);
        }
        actor->mode = 2;
        continue;
      }
      actor->flags = 0;
      switch (actor->mode) {
      case 0:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == 1 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000) {
          actor->mode = 10;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1388) {
          int ang = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, 0);
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
                                            0));
            if (best < d) {
              best = d;
              sel = k;
            }
          }
          if (best > 0x40) {
            st->path->cur = sel;
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1: {
        int ang =
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, 0);

        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (TurnActorYawTowardAngle(actor, ang, 6, 0x14, 1) != 0) {
          MoveActorAlongAngle(actor, ang, 0x50, 0, 0x12C, 5);
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < 0x100) {
            ENTER_POSE(actor, 0);
          }
        }
        break;
      }

      case 2:
        DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 10:
        if (SPY.airborneFrames != 0) {
          break;
        }
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x28, 0xFF, 5) ==
            0x100) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->mode = 11;
          continue;
        }
        break;

      case 11:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x3F) + 0x78;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        ANIM_GO(actor, 0);
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }
}
  }
}
