/* Level22_UpdateActors -- per-frame actor update for level 22
 * (Metalhead). 0x8007B770, 48,524 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 4/9/16/20/21/25/26/31/33; the level-22
 * classes are Metalhead himself (48), his bolt (70) and shots (82), the pod
 * that raises a ring of bobbing platforms (65) and the platform (210).
 * The boulder (57), thrower (102) and gatekeeper (170) are level 21's
 * 57/100/101, and the ram (466) is level 20's.
 *
 * Load-bearing source forms:
 *   - No spy carrier: `spy` is `&SPY` and the constant 2 is what loop.c
 *     hoists into fp. The scratch arrays are declared in the original's slot
 *     order and named by sp offset (g056..g672); the first one (g056) is
 *     addressed sp-relative, every later one through a cse'd base.
 *   - Case 9 is level 16's (its spawn camera reads through a block-local
 *     `base`, held in s3); case 110 is level 25's spelling so its
 *     `< 0xA01` exit cross-jumps into case 331 (A289).
 *   - Case 48: the delta vector reads Spyro as `&spy->posX` and the distance
 *     as `&g_anSpyroWorldPos` (two symbols, so neither address is held), and each
 *     state's distance is its own block-local.
 *   - Case 65: the pod count `n = 4` is a variable (the original keeps the
 *     loop-entry tests), and the platform heading is the giv `i * 0x3C`.
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


/* ==== Level 22 classes ==== */


/* ==== The function ==== */
#define spy (&SPY)
#define one 1
void Level22_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  char *base;
  int dt;

  /* Scratch vectors, in the original's slot order (named by sp offset). */
  int g056[3];
  int g072[3];
  int g088[3];
  int g104[3];
  int g120[3];
  int g136[3];
  int g152[3];
  int g168[3];
  int g184[3];
  int g200[3];
  int g216[3];
  int g232[3];
  int g248[3];
  int g264[3];
  int g280[3];
  int g296[3];
  int g312[3];
  int g328[3];
  int g344[3];
  int g360[3];
  int g376[3][3];
  int g416[3];
  int g432[3];
  int g448[3];
  int g464[6];
  int g488[3];
  int g504[3];
  int g520[3];
  int g536[3];
  int g552[3];
  int g568[3];
  int g584[6];
  int g608[3];
  int g624[3];
  int g640[3];
  int g656[3];
  int g672[3];

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
          SpawnNameLetterActors(actor, NAME_LETTERS_RETURN_HOME);
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
        char *base = (char *)&g_nSpyroWorldPosZ;
        int top = *(int *)base;
        int z = actor->posZ;
        int drop = top - z;

        if ((unsigned int)(drop - 0x201) < 0x3DFF) {
          int depth = 0x2000;

          if (st->depth != 0) {
            depth = st->depth;
          }

          if (g_nSpyroState == SPYRO_ST_LIFT || top + 0xC00 < z + depth) {
            if (g_abHudCounterDisplayState == HUD_CTR_SHOWN) {
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
                g_nCdStreamState = LSTREAM_PRESERVE;
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
                                              actor->posY - spy->posY, ATAN_ANGLE8)) <
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

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, YAW_STEP_IN_BAND);
          moved = MoveActorAlongHeading(actor, 30, 0, 50, MOVE_LIFT);

          if (rotated && moved) {
            st->heading = WrapAddAngle8(st->heading, RandomIntInRange(98, 158));
            st->headingTimer = RandomIntInRange(30, 80);
          }

          if (ApproxDist2D(&actor->posX, &st->tgtX) > 2000) {
            int angle = ArcTan2(st->tgtX - actor->posX,
                                      st->tgtY - actor->posY, ATAN_ANGLE8);

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
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
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

          CopyVector(g056, &SPY.posX);
          cb = LookupCosine(g_bSpyroPersistentEulerZ * 0x10);
          cs = LookupSine(g_bSpyroPersistentEulerZ * 0x10);
          x = cb * 0xB5;
          y = cs * 0xB5;
          g056[0] += (y + x) * 4 >> 12;
          g056[1] += (int)((LookupSine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5) -
                          (LookupCosine(g_bSpyroPersistentEulerZ * 0x10) * 0xB5U)) *
                        4 >>
                    12;
          g056[2] += 0x300;
          VectorToSVectorShr2(st->apex, g056);
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
          SVectorToVectorShl2(g072, st->centre);
          SVectorToVectorShl2(g088, st->apex);
          SubtractVector(g088, g088, g072);
          ScaleVector3Sat(g088, g088, st->phase);
          RShiftVector3(g088, 6);
          AddVector(&actor->posX, g072, g088);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(g104, &SPY.posX);
          g104[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          g104[2] += 0x300;
          VectorToSVectorShr2(st->centre, g104);
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
        SubtractVector(g120, &actor->posX, &CAM.posX);
        WorldToCameraRotate(g120, g120);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, g120, 0);
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

    /* Type 48: Metalhead. Stomps, sprays shots or lobs a bolt depending on
     * how far away Spyro is; each time his health drops a phase he runs
     * the path to the next spot. */
    case ACTOR_TYPE_METALHEAD: {
      MetalheadState *st = actor->state;
      int dist;

      SubtractVector(g104, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
      if (dist < 0x32C8) {
        dist = VectorLength(g104, VEC_LEN_XY);
      }
      if (st->init == 0) {
        st->init = 1;
        ModifyWorldChunkAnimFlags(3, CHUNK_ANIM_RESUME, 0);
        if (IsActorInRunningKillSet(actor) != 0) {
          ModifyWorldChunkAnimFlags(4, CHUNK_ANIM_RESUME, 0);
          ModifyWorldChunkAnimFlags(5, CHUNK_ANIM_RESUME, 0);
          OrPickupGroupActorFlags(0, ACTOR_HIT_BLAST);
          OrPickupGroupActorFlags(1, ACTOR_HIT_BLAST);
          DespawnPickupGroupActors(2);
          DespawnPickupGroupActors(3);
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 8)));
          if (actor->dropKind != 0xFF) {
            SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
            MarkActorCommittedKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      st->lodHandle = SelectActorLodBand(actor, st->lodHandle);
      actor->flags = 0;
      if (actor->mode < METALHEAD_PHASE_TURN) {
        if (st->phase == 0) {
          if (TickCountdownByFrameDelta(&st->camTimer, 4) != 0 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                                 : -SPY_DZ(actor) < 0x578) &&
              dist < 0x6400 && g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FOLLOW_CAMERA);
            g_pScriptedCameraActor = actor;
          }
        } else {
          if (TickCountdownByFrameDelta(&st->camTimer, 4) != 0 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                                 : -SPY_DZ(actor) < 0x578) &&
              dist < 0x6000 && g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FOLLOW_CAMERA);
            g_pScriptedCameraActor = actor;
          }
        }
      }
      if (g_nSpyroState == SPYRO_ST_CHARGE || g_nSpyroState == SPYRO_ST_CHARGE_AIR) {
        st->camTimer = 0x3C;
      }
      if (PickupGroupHasLiveActor(st->phase + 2) == 0 && actor->mode < 4) {
        ModifyWorldChunkAnimFlags(st->phase + 4, CHUNK_ANIM_RESUME, 0);
        actor->mode = METALHEAD_PHASE_TURN;
        continue;
      }
      if (st->seen == 0 && dist < 0x6800) {
        st->seen = 1;
        ModifyWorldChunkAnimFlags(3, CHUNK_ANIM_RESUME, 0);
      }

      switch (actor->mode) {
      case METALHEAD_IDLE: {
        int sel;
        int d;

        d = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
        sel = 1;
        if (!(PickupGroupHasLiveActor(st->phase) != 0 && d >= 0x2000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x385
                                 : -SPY_DZ(actor) < 0x385))) {
          sel = 2;
          if (st->choice == METALHEAD_ATTACK_STOMP) {
            st->choice = METALHEAD_ATTACK_NONE;
          }
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (st->choice == METALHEAD_ATTACK_NONE) {
          int r = RandomIntInRange(0, 100);

          if (sel >= 2) {
            r = RandomIntInRange(0x3C, 100);
          }
          if (r < 0x3C) {
            st->choice = METALHEAD_ATTACK_STOMP;
          } else if (r < 0x55) {
            st->choice = METALHEAD_ATTACK_SPRAY;
          } else {
            st->choice = METALHEAD_ATTACK_LOB;
          }
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          continue;
        }
        switch (st->choice) {
        case METALHEAD_ATTACK_STOMP:
          if (d < st->range[st->phase] << 10 && d > 0x2000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x320
                                 : -SPY_DZ(actor) < 0x320)) {
            if (actor->subMode == 0) {
              actor->subMode = 1;
            }
          } else if (actor->subMode == 1) {
            actor->subMode = 0;
          }
          if (actor->subMode == 2) {
            actor->subMode = 0;
            ENTER_POSE(actor, METALHEAD_STOMP);
          }
          continue;

        case METALHEAD_ATTACK_SPRAY:
          if (d > 0x800 && d < 0x7000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E8
                                 : -SPY_DZ(actor) < 0x3E8)) {
            st->shots = 0;
            actor->mode = METALHEAD_SPRAY;
            continue;
          }
          st->choice = METALHEAD_ATTACK_NONE;
          continue;

        case METALHEAD_ATTACK_LOB:
          if (d > 0x2800 && d < 0x7000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E8
                                 : -SPY_DZ(actor) < 0x3E8)) {
            st->count = 0;
            CopyVector(st->aim, &g_anSpyroWorldPos);
            actor->mode = METALHEAD_LOB;
            continue;
          }
          st->choice = METALHEAD_ATTACK_NONE;
          continue;
        }
        continue;
      }

      case METALHEAD_STOMP:
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = METALHEAD_ATTACK_NONE;
          ENTER_POSE(actor, METALHEAD_IDLE);
        }
        continue;

      case METALHEAD_SPRAY:
        ANIM_GO(actor, 0xC);
        if ((unsigned)(actor->nextFrame - 5) < 0x10) {
          Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_POD_SHOT, actor);
          PodShotState *sh = o->state;
          int d;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 3, &o->posX);
          CopyVector(g056, &actor->posX);
          g056[0] += ((st->shots * 150 + 3000) * g_anCosineLut[actor->rotZ]) >> 12;
          g056[1] += ((st->shots * 150 + 3000) * g_anSineLut[actor->rotZ]) >> 12;
          g056[0] += RandomIntInRange(-0x320, 0x320);
          g056[1] += RandomIntInRange(-0x320, 0x320);
          g056[2] += 0x12C;
          SubtractVector(sh->vel, g056, &o->posX);
          d = ApproxDist2D(&actor->posX, g056) / 500;
          sh->vel[0] /= d;
          sh->vel[1] /= d;
          sh->vel[2] /= d;
          o->cullRadius = 0x28;
          o->rotZ = ArcTan2(g056[0] - o->posX, g056[1] - o->posY, ATAN_ANGLE8);
          o->rotY = ArcTan2(ApproxDist2D(&o->posX, g056),
                                   g056[2] - o->posZ, ATAN_ANGLE8);
          sh->timer = 0xB4;
          sh->heading = st->phase + 2;
          sh->mode = 0;
          st->shots += g_nFrameStep;
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = METALHEAD_ATTACK_NONE;
          ENTER_POSE(actor, METALHEAD_IDLE);
        }
        continue;

      case METALHEAD_LOB:
        ANIM_GO(actor, 0xD);
        if ((unsigned)(actor->nextFrame - 4) < 5 && st->count >= 7) {
          Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_LOBBED_BOLT, actor);
          LobbedBoltState *b;

          CopyVector(&o->posX, st->hand);
          b = o->state;
          b->timer = 0xF0;
          o->rotZ = ArcTan2(st->aim[0] - o->posX, st->aim[1] - o->posY,
                                   ATAN_ANGLE8);
          o->cullRadius = 0x28;
          st->count = 0;
          b->heading = st->phase + 2;
          b->dvz = -7;
          b->speed = (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) >> 8) + 0x190;
          b->vz = ComputeLobVelocityZ(&o->posX, st->aim, b->speed, b->dvz, &b->flightTime);
        }
        if (actor->nextFrame < 3) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 5, st->hand);
        }
        st->count += g_nFrameStep;
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = METALHEAD_ATTACK_NONE;
          ENTER_POSE(actor, METALHEAD_IDLE);
        }
        continue;

      case METALHEAD_PHASE_TURN: {
        int h = ArcTan2(PATH_X(st->path, st->path[1]) - actor->posX,
                              PATH_Y(st->path, st->path[1]) - actor->posY, ATAN_ANGLE8);

        OrPickupGroupActorFlags(st->phase, ACTOR_HIT_BLAST);
        if (st->phase == 1) {
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorCommittedKill(actor);
          actor->mode = METALHEAD_DYING;
          continue;
        }
        ANIM_GO(actor, 4);
        if (TurnActorYawTowardAngle(actor, h, 6, 8, YAW_STEP_IN_BAND) != 0) {
          actor->subMode = 0;
          actor->mode++;
          continue;
        }
        continue;
      }

      case METALHEAD_PHASE_ROAR:
        ANIM_GO(actor, 8);
        if (g_nActorAnimDone != 0) {
          ANIM_THEN(actor, 9);
          st->timer2 = 0x46;
          actor->mode++;
          continue;
        }
        continue;

      case METALHEAD_PHASE_STAGGER:
        PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 3, 5, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          actor->mode++;
          continue;
        }
        continue;

      case METALHEAD_PHASE_RECOVER:
        ANIM_GO(actor, 0xA);
        if (g_nActorAnimDone != 0) {
          ANIM_THEN(actor, 5);
          actor->mode++;
          continue;
        }
        continue;

      case METALHEAD_PHASE_RUN:
        if (PivotActorAlongPath(actor, st->path, 0x15E, 0x118, 0, 8, 0x80, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            st->pathEnd[st->phase] + 0x100) {
          actor->mode++;
          continue;
        }
        continue;

      case METALHEAD_PHASE_ARRIVE:
        if (TrackEntityYawTowardSpyro(actor, 6, 4, YAW_STEP_IN_BAND) != 0) {
          actor->group = ++st->phase;
          SetActorGroupSubstate(actor, 1);
          actor->group = 0xFF;
          actor->subMode = 0;
          ENTER_POSE(actor, METALHEAD_IDLE);
        }
        continue;

      case METALHEAD_DYING:
        ANIM_GO(actor, 0xB);
        if (g_nActorAnimDone != 0) {
          ModifyWorldChunkAnimFlags(3, CHUNK_ANIM_RESUME, 0);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x40);
          g_bLevelBossDefeated = 1;
          DespawnActorRecord(actor);
          continue;
        }
        continue;
      }
      continue;
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
        CopyVector(g136, &actor->posX);
        actor->posX += st->vx;
        actor->posY += st->vy;
        actor->posZ += st->vz;
        actor->rotX += st->spin[0];
        actor->rotY += st->spin[1];
        actor->rotZ += st->spin[2];
        st->life += g_nFrameStep;
        if (actor->subMode != 2 && st->life < 0x79 &&
            RaiseSpyroTriggerInRadius(&actor->posX, 0xDC, SPYRO_EV_KNOCKBACK_2 | SPYRO_EV_CHARGE_HIT) == 0 &&
            CastRayWorldAndActors(g136, &actor->posX) == 0) {
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
        g152[0] = st->vx;
        g152[1] = st->vy;
        g152[2] = st->vz;
        CopyVector(g136, &actor->posX);
        AddVector(&actor->posX, &actor->posX, g152);
        if (TickCountdownByFrameDelta(&st->life, 2) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) == 0) {
          continue;
        }
        CopyVector(g168, &g_anCollisionProbeVec);
        if (ReflectVectorOffNormal(g152, g168, g152) == 0) {
          continue;
        }
        st->vx = (g152[0] * 7) >> 3;
        st->vy = (g152[1] * 7) >> 3;
        st->vz = (g152[2] * 7) >> 3;
        continue;
      }
      continue;
    }

    /* Type 70: a lobbed bolt. Arcs along its heading and bursts into
     * sparks when it lands. */
    case ACTOR_TYPE_LOBBED_BOLT: {
      LobbedBoltState *st = actor->state;
      int i;

      switch (actor->mode) {
      case SHOT_FLY:
        actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
        actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
        if (TickCountdownByFrameDelta(&st->timer, 2) != 0 ||
            RaiseSpyroTriggerInRadius(&actor->posX, 0xC8, SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        OrActorGroupFlagsInCone(actor, st->heading, (ACTOR_HIT_BLAST | ACTOR_HIT_SUPER), 0x258, 0x4B0, 0x4B0, 0x28);
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x168, 0x168) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += st->vz;
        st->vz += st->dvz;
        actor->rotY = ArcTan2(st->speed, st->vz, ATAN_ANGLE8);
        break;

      case SHOT_BURST:
        i = 0;
        do {
          i++;
          g168[0] = (GetRandomU32() & 0x3E) - 0x1F;
          g168[1] = (GetRandomU32() & 0x3E) - 0x1F;
          g168[2] = GetRandomU32() & 0xF;
          CopyVector(g184, g168);
          LShiftVector3(g184, 2);
          AddVector(g184, g184, &actor->posX);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_YELLOW, g184, (void *)g168);
        } while (i < 8);
        DespawnActorRecord(actor);
        continue;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 82: a thrown shot. Flies until it hits the floor, a wall or
     * times out. */
    case ACTOR_TYPE_POD_SHOT: {
      PodShotState *st = actor->state;

      CopyVector(g168, &actor->posX);
      AddVector(&actor->posX, &actor->posX, st->vel);
      if (st->mode == 1) {
        if (TickCountdownByFrameDelta(&st->timer, 2) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        continue;
      }
      if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT) != 0) {
        g_nSpyroRequestVelocityZ = 0;
        DespawnActorRecord(actor);
        continue;
      }
      if (TickCountdownByFrameDelta(&st->timer, 2) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (CastRayWorldAndActors(g168, &actor->posX) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      OrActorGroupFlagsInCone(actor, st->heading, (ACTOR_HIT_BLAST | ACTOR_HIT_SUPER), 0x12C, 0x4B0, 0x258, 0x32);
      continue;
    }

    /* Type 100 (and 102): boulder thrower. Patrols its path, picks up a
     * boulder (57) and hurls it, rides its partner, and charges Spyro. */
    case ACTOR_TYPE_BOULDER_THROWER_B: {
      BoulderThrowerState *st = actor->state;
      int f;

      if (actor->type == ACTOR_TYPE_BOULDER_THROWER_B) {
        TickActorFlameGlow(actor);
      }
      f = actor->flags;
      if ((f & ACTOR_HIT_ANY) && actor->mode != BTHROW_KNOCKED &&
          (actor->type != ACTOR_TYPE_BOULDER_THROWER_B || f != ACTOR_HIT_FLAME)) {
        Actor *c;

        if (f & ACTOR_HIT_BLAST) {
          st->speed = 0;
          st->zSpeed = 0x28;
        } else {
          if (f == ACTOR_HIT_CHARGE) {
            st->speed = 0x15E;
          } else {
            st->speed = 0x118;
          }
          st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, 0x20, 0x40);
          if (actor->mode != BTHROW_HURTLE) {
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
        ENTER_POSE(actor, BTHROW_KNOCKED);
      }
      actor->flags = 0;

      if (st->command != 0) {
        if (st->command == 1) {
          st->command = 0;
          actor->mode = BTHROW_PAIR_WAIT;
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
                                 ATAN_ANGLE8)) >= 0x40) {
          st->path->cur++;
        }
        st->command = 0;
        actor->mode = BTHROW_RIDE_WAIT;
        continue;
      }

      if (st->flags == BTHROW_F_HOP) {
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
      case BTHROW_IDLE:
        if (actor->subMode != 0) {
          ENTER_POSE(actor, actor->subMode);
        }
        if (st->idlePose2 != 0) {
          ENTER_POSE(actor, BTHROW_WATCH);
        }
        break;

      case BTHROW_ROUSE:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->groupAlert != 0) {
          SetActorGroupSubstate(actor, BTHROW_ROUSE);
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        if (st->flags & THROWER_F_ADVANCE) {
          ENTER_POSE(actor, BTHROW_PATROL);
        }
        actor->mode = BTHROW_WATCH;
        ANIM_RESET(actor, 2);
        continue;

      case BTHROW_WATCH:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if ((st->flags & THROWER_F_ADVANCE) &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < st->throwRange) {
          goto pose5;
        }
        if (st->box0HalfWidth != 0 &&
            IsPointInRotatedBox((int *)&g_anSpyroWorldPos, st->box0) != 0) {
        pose5:
          ENTER_POSE(actor, BTHROW_PATROL);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 &&
            ApproxDist2D(&actor->posX, &spy->posX) < st->sightRange &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                               : -SPY_DZ(actor) < 0x1F4) &&
            CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          ENTER_POSE(actor, BTHROW_THROW);
        }
        break;

      case BTHROW_KNOCKED:
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

      case BTHROW_THROW:
        TrackEntityYawTowardSpyro(actor, 8, 0, YAW_HOLD_IN_BAND);
        if ((unsigned)(actor->nextFrame - 3) < 5 && st->rock == 0) {
          st->rock = g_pfnLevelSpawnActor(ACTOR_TYPE_ROLLING_BOULDER, actor);
          st->rock->subMode = 0;
        }
        if (st->rock != 0 && st->rock->subMode == 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, &st->rock->posX);
          st->rock->rotZ = ArcTan2(spy->posX - st->rock->posX,
                                          spy->posY - st->rock->posY, ATAN_ANGLE8);
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
          ENTER_POSE(actor, BTHROW_WATCH);
        }
        break;

      case BTHROW_PATROL:
        st->flags &= ~THROWER_F_ADVANCE;
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1400) {
          ENTER_POSE(actor, BTHROW_THROW);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            0x100) {
          ENTER_POSE(actor, BTHROW_WATCH);
        }
        break;

      case BTHROW_PAIR_WAIT:
        ANIM_GO(actor, 2);
        if (actor->subMode == 0) {
          break;
        }
        st->orbitRadius = ApproxDist2D(&actor->posX, &g_pActorListBase[st->partner].posX);
        actor->mode = BTHROW_PAIR_CIRCLE;
        continue;

      case BTHROW_PAIR_CIRCLE: {
        int h = g_pActorListBase[st->partner].rotZ;

        ANIM_GO(actor, 5);
        if (SteerActorAroundPoint(actor, &g_pActorListBase[st->partner].posX, st->orbitRadius, h, 4,
                          0x64, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0) < 6) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->subMode = 0;
          actor->mode = BTHROW_PAIR_READY;
          continue;
        }
        break;
      }

      case BTHROW_PAIR_READY:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
        if (actor->subMode != 0) {
          int d = ApproxDist2D(&actor->posX, &spy->posX);

          ANIM_GO(actor, 2);
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  SPY.posY - actor->posY, ATAN_ANGLE8);
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
          actor->mode = BTHROW_HURTLE;
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
                                   ATAN_ANGLE8)) < 0xD) {
            break;
          }
          g_pActorListBase[st->partner].subMode = 0;
          actor->mode = BTHROW_PAIR_CIRCLE;
          continue;
        }

      case BTHROW_HURTLE: {
        int r;

        CopyVector(g200, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, 0x12);
        if (r == 3) {
          st->zSpeed = 0x50;
        }
        actor->updateDist = 0xFF;
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        ANIM_GO(actor, 7);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2BC &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x2BC
                               : -SPY_DZ(actor) < 0x2BC) &&
            g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
          int a = StepAngle8Toward(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                              g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8),
                                st->heading, 0x20)
                  << 4;

          g_dwSpyroTriggerEventFlags |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
          g_anSpyroRequestVelocity.x = (LookupCosine(a) * 0x6E) >> 12;
          g_nSpyroRequestVelocityY = (LookupSine(a) * 0x6E) >> 12;
          CAM_LOOK.z = 0x28;
          st->heading = ArcTan2(actor->posX - g_anSpyroWorldPos,
                                  actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
          st->speed = 0xDC;
          st->zSpeed = 0x3C;
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          ENTER_POSE(actor, BTHROW_KNOCKED);
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
                                            o->posY - actor->posY, ATAN_ANGLE8)) < 0x28) {
              o->flags |= ACTOR_HIT_BLAST | ACTOR_HIT_SUPER;
            }
          }
        }
        break;
      }

      case BTHROW_RIDE_WAIT:
        if (st->partnerSignal != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case BTHROW_RIDE_WALK: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR);

        ANIM_GO(actor, 5);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case BTHROW_RIDE_FACE: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->heading = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  ATAN_ANGLE8);
          st->speed = 0x5A;
          st->zSpeed = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        break;
      }

      case BTHROW_RIDE_LEAP: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        ANIM_GO(actor, 6);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, g232);
        GetActorAttachPoint(p, 1, g248);
        AddVector(g216, g232, g248);
        RShiftVector3(g216, 1);
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, sixteen);
        if (actor->posZ < g216[2]) {
          CopyVector(&actor->posX, g216);
          actor->mode++;
        }
        break;
      }

      case BTHROW_RIDE_HELD: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, g280);
        GetActorAttachPoint(p, 1, g296);
        AddVector(g264, g280, g296);
        RShiftVector3(g264, 1);
        CopyVector(&actor->posX, g264);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->releaseFlag = 0;
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, ATAN_ANGLE8);
          st->speed = 0x1C2;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = BTHROW_HURTLE;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 101: gatekeeper. Guards a run of keys (g_pActorListBase actors) and
     * releases them as Spyro approaches; rides its partner and charges. */
    case ACTOR_TYPE_GATEKEEPER_B: {
      GatekeeperState *st = actor->state;
      int f = actor->flags;

      if ((f & (ACTOR_HIT_FLAME | ACTOR_HIT_BLAST | ACTOR_HIT_SUPER)) && actor->mode != GATEKEEPER_KNOCKED) {
        if (f & ACTOR_HIT_BLAST) {
          st->speed = 0;
          st->zSpeed = 0x28;
        } else {
          st->speed = 0xF0;
          st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, ATAN_ANGLE8),
                                  spy->bodyRotZ, 0x20, 0x40);
        }
        if (actor->mode != GATEKEEPER_HURTLE) {
          st->zSpeed = 0x1E;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, GATEKEEPER_KNOCKED);
      }

      if (st->command != 0) {
        if (st->command == 1) {
          st->command = 0;
          actor->mode = GATEKEEPER_KEYS_ARM;
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
                                 ATAN_ANGLE8)) >= 0x40) {
          st->path->cur++;
        }
        st->command = 0;
        actor->mode = GATEKEEPER_RIDE_WAIT;
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
      case GATEKEEPER_IDLE:
        if (actor->subMode == 0 && st->idlePose2 == 0) {
          break;
        }
        if (actor->type == ACTOR_TYPE_GATEKEEPER_B) {
          actor->mode = GATEKEEPER_WATCH;
          ANIM_RESET(actor, 2);
          continue;
        }
        ENTER_POSE(actor, GATEKEEPER_ROUSE);

      case GATEKEEPER_ROUSE:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->groupAlert != 0) {
          SetActorGroupSubstate(actor, BTHROW_WATCH);
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
        }
        actor->mode = GATEKEEPER_WATCH;
        ANIM_RESET(actor, 2);
        continue;

      case GATEKEEPER_WATCH:
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x1C00) {
          ENTER_POSE(actor, GATEKEEPER_STALK);
        }
        break;

      case GATEKEEPER_STALK: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int dz = SPY_DZ(actor);

        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (d > 0x2400) {
          ENTER_POSE(actor, GATEKEEPER_WATCH);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0xD48 &&
            (unsigned)(dz + 0x577) < 0x4AF) {
          ENTER_POSE(actor, GATEKEEPER_SWIPE);
        }
        break;
      }

      case GATEKEEPER_SWIPE: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
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
          ENTER_POSE(actor, GATEKEEPER_STALK);
        }
        break;
      }

      case GATEKEEPER_KNOCKED:
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0xE, 0xA);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x34);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case GATEKEEPER_KEYS_ARM:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (st->idx < st->count &&
            ApproxDist2D(&actor->posX, &spy->posX) <
                (st->range[0] + 6) << 10 &&
            actor->drawn != 0 &&
            (st->testFacing == -1 ||
             AbsAngleDelta8(st->testFacing,
                           ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, ATAN_ANGLE8)) < 0x1C)) {
          g_pActorListBase[st->keys[st->idx]].subMode = 1;
          actor->mode++;
          break;
        }
        if (st->idx < st->count) {
          break;
        }
        ENTER_POSE(actor, GATEKEEPER_WATCH);

      case GATEKEEPER_KEYS_WAIT:
        ANIM_GO(actor, 2);
        TrackEntityYawTowardSpyro(actor, 4, 0, YAW_HOLD_IN_BAND);
        if (actor->subMode == 0) {
          break;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) >=
            st->range[st->idx] << 10) {
          break;
        }
        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - SPY.posY, ATAN_ANGLE8)) >= 0x30) {
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

      case GATEKEEPER_KEYS_RELEASE: {
        int k;

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, YAW_HOLD_IN_BAND);
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
        actor->mode = GATEKEEPER_KEYS_RECOVER;
        continue;
      }

      case GATEKEEPER_HURTLE: {
        int r;

        CopyVector(g200, &actor->posX);
        r = DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, 0x12);
        actor->updateDist = 0xFF;
        if (r == 3) {
          st->zSpeed = 0x5A;
        }
        ANIM_GO(actor, 8);
        actor->probeDist = 0xFF;
        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x3E8 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x320
                               : -SPY_DZ(actor) < 0x320)) {
          int a = StepAngle8Toward(ArcTan2(spy->posX - actor->posX,
                                              spy->posY - actor->posY, ATAN_ANGLE8),
                                st->heading, 0x20)
                  << 4;

          spy->triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
          spy->requestVelX = (LookupCosine(a) * 0x6E) >> 12;
          spy->requestVelY = (LookupSine(a) * 0x6E) >> 12;
          spy->requestVelZ = 0x28;
          st->heading = ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, ATAN_ANGLE8);
          st->speed = 0xF0;
          st->zSpeed = 0x3C;
          SpawnActorDeathDrop(actor, DROP_TUMBLE, 0, 0);
          ENTER_POSE(actor, GATEKEEPER_KNOCKED);
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
                                            o->posY - actor->posY, ATAN_ANGLE8)) < 0x28) {
              o->flags |= ACTOR_HIT_BLAST | ACTOR_HIT_SUPER;
            }
          }
        }
        break;
      }

      case GATEKEEPER_KEYS_RECOVER:
        if (g_nActorAnimDone == 0) {
          break;
        }
        if (st->idx < st->count) {
          actor->mode = GATEKEEPER_KEYS_WAIT;
          continue;
        }
        ENTER_POSE(actor, GATEKEEPER_WATCH);

      case GATEKEEPER_RIDE_WAIT:
        if (st->partnerSignal != 0) {
          actor->cullRadius = 0x28;
          actor->mode++;
        }
        break;

      case GATEKEEPER_RIDE_WALK: {
        int r = PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x80, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR);

        ANIM_GO(actor, 6);
        if (r == 0x100) {
          actor->mode++;
        }
        break;
      }

      case GATEKEEPER_RIDE_FACE: {
        Actor *p = &g_pActorListBase[st->rider];

        if (p->subMode != 0) {
          st->heading = ArcTan2(p->posX - actor->posX, p->posY - actor->posY,
                                  ATAN_ANGLE8);
          st->speed = 0x5A;
          st->zSpeed = 0x78;
          p->subMode = 2;
          actor->mode++;
          break;
        }
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, ATAN_ANGLE8),
                      6, 0, YAW_HOLD_IN_BAND);
        break;
      }

      case GATEKEEPER_RIDE_LEAP: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        ANIM_GO(actor, 7);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, g232);
        GetActorAttachPoint(p, 1, g312);
        AddVector(g280, g232, g312);
        RShiftVector3(g280, 1);
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0, sixteen);
        if (actor->posZ < g280[2]) {
          CopyVector(&actor->posX, g280);
          actor->mode++;
        }
        break;
      }

      case GATEKEEPER_RIDE_HELD: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, YAW_HOLD_IN_BAND);
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, g344);
        GetActorAttachPoint(p, 1, g360);
        AddVector(g328, g344, g360);
        RShiftVector3(g328, 1);
        CopyVector(&actor->posX, g328);
        if (p->nextFrame >= 0xE) {
          if (st->partner != -1) {
            ((int *)g_pActorListBase[st->partner].state)[5] = 1;
          }
          *st->releaseFlag = 0;
          st->heading = ArcTan2(spy->posX - actor->posX,
                                  spy->posY - actor->posY, ATAN_ANGLE8);
          st->speed = 0x1F4;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = GATEKEEPER_HURTLE;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 65: a pod. Raises a ring of four bobbing platforms (210) and
     * lobs shots (82) at the spot its partner watches; a charge or a
     * flame-and-charge pops it. */
    case ACTOR_TYPE_PLATFORM_POD: {
      PlatformPodState *st = actor->state;
      int n = 4;
      int i;

      TickActorFlameGlow(actor);
      if ((actor->flags & ACTOR_HIT_CHARGE) && actor->mode < PPOD_UP) {
        PlayActorMeshVoice(actor, 0, 0);
        actor->posZ -= 0x190;
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_GROUND_BOUNCE_SHARD, actor);
        }
        (*st->counter)--;
        for (i = 0; i < n; i++) {
          if (st->kids[i] != 0) {
            DespawnActorRecord(st->kids[i]);
          }
        }
        DespawnActorRecord(actor);
        continue;
      }
      if ((actor->flags & (ACTOR_HIT_BLAST | ACTOR_HIT_SUPER)) == (ACTOR_HIT_BLAST | ACTOR_HIT_SUPER)) {
        PlayActorMeshVoice(actor, 0, 0);
        actor->posZ -= 0x190;
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_GROUND_BOUNCE_SHARD, actor);
        }
        (*st->counter)--;
        for (i = 0; i < n; i++) {
          if (st->kids[i] != 0) {
            DespawnActorRecord(st->kids[i]);
          }
        }
        DespawnActorRecord(actor);
        continue;
      }
      actor->flags = 0;
      if (st->init == 0) {
        st->init = 1;
        *st->counter = st->initCount;
        st->timer *= 60;
      }

      switch (actor->mode) {
      case PPOD_WAIT:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x46;
          actor->mode = PPOD_OPEN;
          ANIM_RESET(actor, 1);
          continue;
        }
        break;

      case PPOD_OPEN:
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0 || g_nActorAnimDone == 0) {
          break;
        }
        st->timer = st->period;
        {
          for (i = 0; i < n; i++) {
            BobbingPlatformState *b;

            st->kids[i] = g_pfnLevelSpawnActor(ACTOR_TYPE_BOBBING_PLATFORM, actor);
            st->kids[i]->cullRadius = 0;
            b = st->kids[i]->state;
            CopyVector(&st->kids[i]->posX, &actor->posX);
            b->period = 0x3B6;
            b->spawned = 1;
            b->z = i * (0x3B6 / n);
            b->baseZ = st->kids[i]->posZ - 0x2EE;
            st->kids[i]->posZ += b->z;
            st->kids[i]->rotZ = i * 0x3C;
            st->kids[i]->otBias = 6;
            st->kids[i]->subMode = RandomSignedInRange(10, 0x12);
          }
        }
        actor->mode = PPOD_UP;
        ANIM_RESET(actor, 2);
        continue;

      case PPOD_UP:
        if (*(int *)g_pActorListBase[st->partner].state == actor->group - 2 &&
            TickCountdownByFrameDelta(&st->shotTimer, 4) != 0) {
          Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_POD_SHOT, actor);
          PodShotState *sh = o->state;
          int d;

          st->shotTimer = RandomIntInRange(0xC, 0x28);
          if (o->anim != 1) {
            ANIM_RESET(o, 1);
          }
          o->scale = 0x36;
          o->posZ += 0x1F4;
          CopyVector(g200, &g_pActorListBase[st->partner].posX);
          g200[2] += 0x898;
          SubtractVector(sh->vel, g200, &o->posX);
          d = ApproxDist2D(&actor->posX, g200) / 300;
          sh->vel[0] /= d;
          sh->vel[1] /= d;
          sh->vel[2] /= d;
          o->cullRadius = 0x28;
          o->rotZ = ArcTan2(g200[0] - o->posX, g200[1] - o->posY, ATAN_ANGLE8);
          o->rotY = ArcTan2(ApproxDist2D(&o->posX, g200),
                                   g200[2] - o->posZ, ATAN_ANGLE8);
          sh->timer = d * 2;
          sh->heading = 0xFF;
          sh->mode = 1;
          AddVector(&o->posX, &o->posX, sh->vel);
          AddVector(&o->posX, &o->posX, sh->vel);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        st->timer = *st->counter * 60;
        for (i = 0; i < n; i++) {
          DespawnActorRecord(st->kids[i]);
          st->kids[i] = 0;
        }
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        PlayActorMeshVoice(actor, 1, 0);
        RESET_IDLE(actor);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_GROUND_BOUNCE_SHARD:
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
        g200[0] = GetRandomU32() & 3;
        g200[1] = GetRandomU32() & 3;
        g200[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, g200);
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

        CopyVector(g296, &actor->posX);
        g296[2] += 0x400;
        FindGroundHeightBelow(g296, 0x10000);
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
            if (actor->type >= ACTOR_TYPE_GEM_1) {
              int *nrm = &g_anCollisionProbeVec;

              st->pitch = -ArcTan2_8bit(
                  IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                  g_nCollisionProbeVecY);
              st->yaw = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
            }

          } else if (st->sub == 2) {

            UnpackWorldCollisionTri(st->groundTri, g376[0]);

            AddVector(g376[0], g376[0], g376[1]);
            AddVector(g376[0], g376[0], g376[2]);

            actor->posX = g376[0][0] / 3;
            actor->posY = g376[0][1] / 3;
            actor->posZ = g376[0][2] / 3;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(g344, &actor->posX);
            g344[2] += 0x400;

            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);

            if (FindGroundHeightBelow(g344, 0x1000) > 0) {
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

        ScaleVector3Sat(g232, st->vel, g_nFrameStep);
        RShiftVector3(g232, 1);

        speed = VectorLength(g232, VEC_LEN_XYZ);

        if (220 < speed) {
          ScaleVector3ByRatio(g232, speed, 220);
        }

        AddVector(g232, &actor->posX, g232);

        if (-220 < st->vel[2]) {
          st->vel[2] -= g_nFrameStep * 5;
        }

        if (g232[2] < 0) {
          if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
            CollectPickupActor(actor);
          }
          DespawnActorRecord(actor);
          continue;
        } else {
          g232[2] += 240;

          if (CollideSphereWithWorldAndActors(g232, 240, 240)) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(g416, &g_anCollisionProbeVec);
            CopyVector(&actor->posX, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->count == 0) {
              int groundHeight = FindGroundHeightBelow(g232, 0x400);
              int groundAngle =
                  (signed char)ArcTan2_8bit(g416[2], VectorLength(g416, VEC_LEN_XY));

              if ((g232[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = COLLECT_REST;
                CopyVector(&actor->posX, g232);
                actor->posZ = groundHeight;

                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {

                  st->pitch = -ArcTan2_8bit(
                      IntegerSqrt((g416[0] * g416[0]) + (g416[2] * g416[2])),
                      g416[1]);
                  st->yaw = -ArcTan2_8bit(g416[2], g416[0]);

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

              if (ReflectVectorOffNormal(st->vel, g416, st->vel)) {
                st->count--;

                st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 32;
                st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
              }
            }

          } else {
            g232[2] -= 240;
            CopyVector(&actor->posX, g232);
            FindGroundHeightBelow(g232, 0x10000);
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

          SubtractVector(g432, &SPY.posX, st->vel);
          RShiftVector3(g432, 5);

          if (VectorLength(g432, VEC_LEN_XYZ) > 480) {
            CopyVector(&actor->posX, &SPY.posX);
            st->timer = 32;
          } else {
            ScaleVector3Sat(g432, g432, st->timer);
            AddVector(&actor->posX, st->vel, g432);
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
      SaveHostState *st = (SaveHostState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->armer];
      Actor *tgtB = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case SAVEHOST_WAIT_AWAY: {
        actor->collision = 0;
        if (ApproxDist2D(&tgtB->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = SAVEHOST_WAIT_ARMED;
        continue;
      }

      case SAVEHOST_WAIT_ARMED: {
        if (tgtA->mode >= 0x80u) {
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 16;
          st->baseZ = actor->posZ;
        }
        break;
      }

      case SAVEHOST_HIDDEN: {
        if (st->timer != 0) {
          st->timer--;
        } else {
          actor->collision = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 2048) {
              if (st->prompted == 0 || tgtB->mode == 1) {
                st->prompted = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, ATAN_ANGLE8) -
                               128;
                actor->cullRadius = 0x20;
                actor->mode = SAVEHOST_POP_IN;
                actor->scale = 0x60;

                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (void *)0xC);

                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(&actor->posX, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= ACTOR_GROUND_VALID;
              }
            }
          }
        }
        break;
      }

      case SAVEHOST_POP_IN: {
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->mode = SAVEHOST_HOVER;
          st->timer = 0;
          st->wanderTimer = 30;
        }
        break;
      }

      case SAVEHOST_HOVER: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 1024 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > 0xC00)) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          st->wanderTimer--;
          if (st->wanderTimer == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + 0x140;
            char finalAngle = (ArcTan2(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, ATAN_ANGLE8) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->wanderTo, &tgtB->posX);

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

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 0x400 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = SAVEHOST_POP_OUT;
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2800 ||
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) >= 0xC01) {
          actor->mode = SAVEHOST_POP_OUT;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->wanderTimer++;

          SubtractVector(g296, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (g296[0] * cosDelta) / totalWeight;
          actor->posY += ((g296[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->wanderTimer == 0x10) {
            actor->mode = SAVEHOST_HOVER;
            st->wanderTimer = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }

        break;
      }

      case SAVEHOST_POP_OUT: {
        if (actor->scale < 96) {
          actor->scale += 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = SAVEHOST_HIDDEN;
          st->timer = 16;

          g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (void *)0xC);

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
        g296[0] = 0;
        g296[1] = 0x64;
        g296[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g296, g296);
        AddVector(g296, g296, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g296, 0);
        g296[0] = 0;
        g296[1] = -0x64;
        g296[2] = 0;
        ApplyActiveGteRotation(g296, g296);
        AddVector(g296, g296, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g296, 0);
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

            CopyVector(g344, &st->target->posX);

            if (high == 0) {
              g344[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              g344[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              g344[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              g344[1] -=
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

            SubtractVector(g344, g344, &actor->posX);
            dist = VectorLength(g344, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(g344, dist, limit);
            } else {
              ScaleVector3ByRatio(g344, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, g344);
            TurnActorYawTowardAngle(actor, ArcTan2(g344[0], g344[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
            actor->rotY = ArcTan2(VectorLength(g344, VEC_LEN_XY), g344[2], ATAN_ANGLE8);
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

            SubtractVector(g448, &st->target->posX, &actor->posX);
            dist = VectorLength(g448, VEC_LEN_XYZ);

            if (dist < 0x200) {
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
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(g448, dist, lim);
              } else {
                ScaleVector3ByRatio(g448, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g448);
              TurnActorYawTowardAngle(actor, ArcTan2(g448[0], g448[1], ATAN_ANGLE8), 0xA, 0, YAW_HOLD_IN_BAND);
              actor->rotY = ArcTan2(VectorLength(g448, VEC_LEN_XY), g448[2], ATAN_ANGLE8);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* No target: hover at a jittered offset beside Spyro. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            g296[0] = -0x258;
            g296[1] = (GetRandomU32() & 0x310) - 0x188;
            g296[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = g296[0];
            st->drift[1] = g296[1];
            st->drift[2] = g296[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              g296[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              g296[1] = st->drift[1];
              g296[2] = st->drift[2];
              if (g296[2] < 0x64) {
                g296[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                g296[0] = st->drift[0] - 0x200;
              } else {
                g296[0] = st->drift[0];
              }
              g296[1] = st->drift[1];
              g296[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, g296, g296);
            AddVector(g296, g296, &SPY.posX);
            at = &actor->posX;
            SubtractVector(g296, g296, at);
            RShiftVector3(g296, 2);
            AddVector(at, at, g296);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (g296[2] >= 0x21) {
              g296[2] = 0x20;
            }
            if (g296[2] < -0x20) {
              g296[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = g296[2];
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, YAW_HOLD_IN_BAND);
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
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);
        DespawnActorRecord(actor);
        continue;
      }
      AddVector(&actor->posX, &actor->posX, st->vel);
      st->vel[2] -= 0xC;
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];
      if (!(actor->subMode & 1)) {
        CopyVector(g464, &actor->posX);
        g464[3] = GetRandomU32() & 3;
        g464[4] = GetRandomU32() & 3;
        g464[5] = 0x14;
        g488[0] = 0x80;
        g488[1] = 0x70;
        g488[2] = 0x40;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_RGB, g464, g488);
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
                                          actor->posY - spy->posY, ATAN_ANGLE8)) < 0x30 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, ATAN_ANGLE8)) < 0x30) {
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
        g_nCinematicActive = 1;

        if (st->shake < 0xC0) {
          g504[0] = 0x200;
          g504[1] = 0;
          g504[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g504, g504);
          AddVector(g504, g504, &actor->posX);
          SubtractVector(g504, g504, &spy->posX);
          g504[2] -= 0x100;
          RShiftVector3(g504, 5);
          ScaleVector3Sat(g504, g504, st->shake);
          DivideVector3(g504, g504, 6);
          AddVector(g504, g504, &spy->posX);
          g504[2] += 0x100;
          g504[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, g504);
          g_nSpyroSwingEulerTargetY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

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
          g520[0] = 0x200 - (st->shake - 0xC0) * 8;
          g520[1] = 0;
          g520[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g520, g520);
          AddVector(&key->posX, g520, &actor->posX);
        }

        if (st->shake >= 0xD0 ||
            !(SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
              SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          int i;

          key->cullRadius = 0;
          key->updateDist = 0;
          key->drawn = 0;
          g_nCinematicActive = 0;
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
          g_nSpyroSwingEulerTargetZ = 0;
          g_nSpyroSwingEulerTargetY[0] = 0;
          g_nSpyroSwingEulerTargetX = 0;
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

    /* Type 210: a spinning platform that bobs on a fixed period. */
    case ACTOR_TYPE_BOBBING_PLATFORM: {
      BobbingPlatformState *st = actor->state;

      actor->cullRadius = 0x20;
      actor->rotZ += actor->subMode;
      if (st->z > st->period) {
        st->z -= st->period;
      }
      actor->scale = 0x3A - ((st->period - st->z) * 20) / st->period;
      actor->posZ = st->baseZ + st->z;
      continue;
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
        if (ApproxDist2D(at, &SPY.posX) < 0x800) {
          SubtractVector(g536, at, &SPY.posX);
          g536[2] = (g536[2] * 3) >> 2;

          if (VectorLength(g536, VEC_LEN_XYZ) < 0x440) {
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
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->cutsceneId == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIndex] += 1;
              g_nTotalDragonsRescued += 1;

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

    /* Whirlwind (WhirlwindState): lifts Spyro to its top (SPYRO_EV_LIFT). */
    case ACTOR_TYPE_WHIRLWIND: {
      WhirlwindState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (WhirlwindState *)actor->state;
      if (st->flags & WHIRL_F_GATED) {
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
      if (flags & WHIRL_F_IDLE) {
        if (st->timer != 0) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0)
            st->flags = (st->flags & ~WHIRL_F_IDLE) | WHIRL_F_WOKEN;
          break;
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & WHIRL_F_MARK)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, SFX_MODE_3D, &actor->voiceRow);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_PLUME, (int *)actor, 0);
      SubtractVector(g536, &SPY.posX, &actor->posX);
      distance = VectorLength(g536, VEC_LEN_XY);
      if (distance < st->radius && g536[2] >= -0x3FF && g536[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(SPY.firstContactPoint, &actor->posX);
        if (st->flags & WHIRL_F_TARGET_CAM) {
          spyro->requestMask |= SPYRO_RQ_TARGET_CAMERA;
        } else {
          g_anCameraActiveTargetParams = g_anCameraParamMode2Front;
          SPY.pCamAnchor = &SPY.posX;
          SPY.pCamTarget = &g_anCameraActiveTargetParams;
          spyro->requestMask |= SPYRO_RQ_CAMERA;
          g_anCameraActiveTargetParams.w[0] = ((0x80 - actor->rotZ) << 4) & 0xFFF;
        }
        st->timer = 0x78;
        break;
      }
      if ((st->flags & ~WHIRL_F_TARGET_CAM) == WHIRL_F_WOKEN && st->timer != 0 &&
          TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~WHIRL_F_WOKEN) | WHIRL_F_IDLE;
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

        g552[0] = (GetRandomU32() & 0xFE) - 0x7F;
        g552[1] = (GetRandomU32() & 0xFE) - 0x7F;
        g552[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(g552, g552, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, g552, (int *)1);
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
            g568[0] = (GetRandomU32() & 0xFE) - 0x7F;
            g568[1] = (GetRandomU32() & 0xFE) - 0x7F;
            g568[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(g568, g568, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, g568, (int *)1);
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
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & ACTOR_HIT_FLAME) {
          actor->subMode = BOULDER_CRACKING;
          actor->updateDist = 0xFF;
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
          actor->cullRadius = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;

      case BOULDER_CRACKING: {
        int t = st->timer + g_nFrameStep;
        int i;
        int n;

        st->timer = t;
        if (t < 0x78) {
        if ((actor->flags & ACTOR_HIT_BLAST) && t < 0x5A) {
          st->timer = 0x5A;
        }
        for (i = 0; i < 6; i++) {
          g584[0] = g_aSmokeJetEmitters[i >> 1].pos[0];
          g584[1] = g_aSmokeJetEmitters[i >> 1].pos[1];
          g584[2] = g_aSmokeJetEmitters[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, g584, g584);
          AddVector(g584, g584, &actor->posX);
          g584[3] = g_aSmokeJetEmitters[i >> 1].vel[0];
          g584[4] = g_aSmokeJetEmitters[i >> 1].vel[1];
          g584[5] = g_aSmokeJetEmitters[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, g584 + 3, g584 + 3);
          g584[3] += (GetRandomU32() & 0xF) - 8;
          g584[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, g584, g584 + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          g584[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          g584[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          g584[2] = (GetRandomU32() & 0x7F) + 0xC0;
          g584[3] = g584[0] >> 4;
          g584[4] = g584[4] >> 4;
          g584[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, g584, g584);
          AddVector(g584, g584, &actor->posX);
          RotateVectorByMatrix(actor->mtx, g584 + 3, g584 + 3);
          g608[0] = (GetRandomU32() & 7) + 0x10;
          g608[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, g584, g608);
          if (actor->voiceRow == VOICE_ROW_NONE) {
            PlayActorMeshVoice(actor, 0, 0);
          }
        }
        if (st->timer >= 0x39) {
          actor->rotX = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a + st->rotX;
          actor->rotY = g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b + st->rotY;
          actor->posZ =
              st->restZ +
              (ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].a) +
               ABS2((signed char)g_aShakeRotOffsets[(0x78 - st->timer) >> 1].b)) *
                  6;
        }
        } else {
          actor->subMode = BOULDER_BURST;
          actor->cullRadius = 0;
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

          PlayActorMeshVoice(actor, 1, 0);
          SpawnActorDeathDrop(actor, DROP_HIGH_HOP, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            p = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 2, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 1) + 0x12;
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
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(ACTOR_TYPE_BOULDER_DEBRIS + 1, actor);
            cs = c->state;
            c->subMode = (GetRandomU32() & 3) + 0x19;
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
                          (0x1000 - ApproxDist2D(&actor->posX, &SPY.posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = 0x46;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, (ACTOR_HIT_FLAME | ACTOR_HIT_BLAST), actor, 0);
        if (++st->timer >= 8) {
          MarkActorCommittedKill(actor);
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
          ZeroVector(g608);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, g608);
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
            CopyVector(g624, &st->prize->posX);
            g624[0] += g_anCosineLut[j * 43] >> 6;
            g624[1] += g_anSineLut[j * 43] >> 6;
            g624[2] += 0x28;
            g640[0] = g_anCosineLut[j * 43] >> 7;
            g640[1] = g_anSineLut[j * 43] >> 7;
            g640[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, g624, g640);
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
      case CHECKPT_REARM:
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = CHECKPT_ARMED;
        continue;

      case CHECKPT_ARMED: {
        int height;
        int drop;

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x380) {
          break;
        }

        height = actor->posZ - actor->floorOffset;
        drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
          SetSpawnAnchorFromActor(actor, actor->rotZ);
          actor->mode = CHECKPT_FLASH;
          actor->anim = 1;
          actor->subMode = 0;
          continue;
        }
        break;
      }

      case CHECKPT_FLASH: {
        int d = g_nFrameStep;
        int tick = actor->subMode + d;

        actor->subMode = tick;
        if ((unsigned char)tick < 0x30) {
          break;
        }
        actor->mode = CHECKPT_REARM;
        actor->anim = 0;
        continue;
      }
      }
      break;

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD2_ENTER_FX:
    case ACTOR_TYPE_HAZARD2_DEATH_FX:      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

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
        g608[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g608[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g608[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, g608);
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
        if (ApproxDist2D(&st->parent->posX, &SPY.posX) < 0x400) {
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
          bearing = ArcTan2(SPY.posX - st->parent->posX,
                                  SPY.posY - st->parent->posY, ATAN_ANGLE8);
          g608[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g608[1] = (g_anSineLut[bearing] * 3) >> 4;
          g608[2] = 0;
          g624[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g624[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g624[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(g656, g624, st->radius - 1);
          LShiftVector3(g624, 1);
          arm = st->radius - 1;
          g656[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          g672[0] = g608[0] * g_anCosineLut[arm * 2];
          g672[1] = g608[1] * g_anCosineLut[arm * 2];
          g672[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(g624, g624, st->phase);
          SubtractVector(g656, g656, g624);
          swing = &g_anCosineLut[bearing];
          actor->posX = g608[0] * *swing;
          actor->posY = g608[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, g672);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g608);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, g656);
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
          st->knock = ArcTan2(actor->posX - g_pActorListBase[st->target].posX,
                                    actor->posY - g_pActorListBase[st->target].posY,
                                    ATAN_ANGLE8);
          st->speed = 0x8C;
          st->lift = 0x23;
        } else {
          st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
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
        if (actor->nextAnim != 3) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 3;
          actor->nextFrame = 0;
          OnActorAnimQueued(actor);
        }
        actor->mode = RAM_KNOCKED;
        continue;
      }
      actor->flags = 0;
      if (st->triggerSlot != -1 && g_anLevelTriggerStateSlots[st->triggerSlot] != 0 &&
          (unsigned int)(actor->mode - 3) >= 2 &&
          IsPointInRotatedBox(&actor->posX, &st->box.x) != 0) {
        ENTER_POSE(actor, RAM_TRAPPED);
      }

      switch (actor->mode) {
      case RAM_GRAZE:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == 1 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800) {
          actor->mode = RAM_RUN_PATH;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1388) {
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
          if (best > 0x40) {
            st->path->cur = sel;
            ENTER_POSE(actor, RAM_WANDER);
          }
        }
        break;

      case RAM_WANDER: {
        int ang =
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8);

        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (TurnActorYawTowardAngle(actor, ang, 6, 0x14, YAW_STEP_IN_BAND) != 0) {
          MoveActorAlongAngle(actor, ang, 0x50, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < 0x100) {
            ENTER_POSE(actor, RAM_GRAZE);
          }
        }
        break;
      }

      case RAM_KNOCKED:
        DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case RAM_TRAPPED:
        if (g_anLevelTriggerStateSlots[st->triggerSlot] == 0) {
          st->speed = 0;
          st->lift = 0;
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, RAM_KNOCKED);
        }
        if (actor->nextFrame >= 0xF) {
          actor->frame = 0;
          actor->nextFrame = 1;
          actor->frameProgress = 0;
        }
        break;

      case RAM_RUN_PATH:
        if (SPY.airborneFrames != 0) {
          break;
        }
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 8, 0x28, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            0x100) {
          g_pActorListBase[st->partner].subMode = 1;
          actor->mode = RAM_PATH_END;
          continue;
        }
        break;

      case RAM_PATH_END:
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
