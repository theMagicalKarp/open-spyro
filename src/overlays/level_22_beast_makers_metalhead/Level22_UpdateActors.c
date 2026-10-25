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
  StubActorHook(a);

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
    StubActorHook(a);                                                          \
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
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  base = (char *)&g_nSpyroWorldPosZ;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Exit drop (DropState). */
    case 9: {
      DropState *st = actor->state;
      int dist = ApproxDist2D(&actor->posX, &CAM.posX);
      int i;

      if (actor->mode != 0 ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == 0) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = 1;
      } else {
        actor->mode = 0;
      }

      dist = ApproxDist2D(&actor->posX, &SPY.posX);

      for (i = 0; i < g_nFrameStep; i++) {
        int tick = ++st->tick;

        if (dist < 0x4000) {
          if (!(tick & 3)) {
            g_pfnLevelEmitParticles(1, 0x15, &actor->posX, (void *)tick);
          }
        } else if ((tick & 0xF) < 8 && !(tick & 3)) {
          g_pfnLevelEmitParticles(1, 0x4C, &actor->posX, (void *)tick);
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
                g_abLevelGemPercent[g_nLevelIntroIndex] = 1;
                g_nCdStreamState = 0;
                g_nEnteredPortalIdx = -1;
                g_nScriptedRespawnFlag = 1;
                g_nLevelTransitionTallyActive = 1;
                g_nLevelTransitionPhase = 0;
                g_nGamestate = GS_LEVEL_LOAD;
                g_nGamePaused = 1;
                g_nReturnPortalLevelId = g_nActiveLevelId;
                g_nCurrentLevelId = g_nActiveLevelId / 10 * 10;
                g_nCameraNextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
                g_dwSpyroRequestMask = 0;
                ResetSpyroEntity(0);
                g_nSpyroFallTrackingFlag = 0;
                g_nSpyroFlameBreathTimer = 0;
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
    case 13: {
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
            if (parent != actor && parent->mode < ACTOR_MODE_DORMANT && parent->type == 13) {
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
              SpawnActorDeathDrop(actor, 4, 0, 0);
            } else if (FindGroundHeightBelow(st->pos, 0x1400) != 0) {
              st->pos[2] -= 0x400;
              SpawnActorDeathDrop(actor, 2, 0, st->pos);
            } else {
              SpawnActorDeathDrop(actor, 1, 0, 0);
            }
            MarkActorCommittedKill(actor);
            *(volatile int *)&st->count <<= 7;
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= ACTOR_MODE_DORMANT) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < ACTOR_MODE_DORMANT && parent->type == 13 &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[0x37]);
            PlaySoundEffect(g_pLevelSampleBankHeader[0x37], actor, 8, &actor->sfxSlot);
          }
        }
      }

      break;
    }

    /* Butterfly (ChaseState). */
    case ACTOR_TYPE_BUTTERFLY: {
      ChaseState *st = (ChaseState *)actor->state;

      if (SPY.sparxHealth < 3 && ApproxDist2D(&actor->posX, &SPY.posX) < 2400) {
        int actorZ = actor->posZ - actor->floorOffset;

        if (ABS2(actorZ - SPY.posZ) < 1800) {
          if (g_pSparxActor == 0) {
            if (SPY.sparxHealth >= 0) {
              g_pSparxActor = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              PlayActorMeshVoice(g_pSparxActor, 0, 0);

              SPY.sparxHealth = 1;

              DespawnActorRecord(actor);
              break;
            }
          } else if (g_pSparxActor->subMode != 99) {
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

              g_pSparxActor->subMode = 0;
              cs->owner = actor;
              cs->timer = RandomIntInRange(150, 210);

              actor->mode = 1;
            }
          }
        }
      }

      if (actor->cullRadius & 0x80) {
        break;
      }

      switch (actor->mode) {
      case 0: {
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
      case 1: {
        SteerActorAroundPoint(actor, &spy->posX, 0x320,
                      WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    st->dir * 8),
                      8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

        if (g_pSparxActor == 0 || g_pSparxActor->subMode == 99) {
          actor->mode = 0;
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
          actor->mode = 5;
        }
      }

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->centre, &actor->posX);
        st->angle =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->radius = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode += 1;
        break;

      case 2: {
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
          actor->mode = 5;
        }
        break;
      }

      case 3: {
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
          actor->mode = 5;
        }
        break;
      }

      case 4: {
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
          actor->mode = 5;
        }
        break;
      }

      case 5: {
        SubtractVector(g120, &actor->posX, &CAM.posX);
        WorldToCameraRotate(g120, g120);
        g_pfnLevelEmitParticles(0x10, 0x4D, g120, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->idx);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Type 48: Metalhead. Stomps, sprays shots or lobs a bolt depending on
     * how far away Spyro is; each time his health drops a phase he runs
     * the path to the next spot. */
    case 48: {
      MetalheadState *st = actor->state;
      int dist;

      SubtractVector(g104, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
      if (dist < 0x32C8) {
        dist = VectorLength(g104, 0);
      }
      if (st->init == 0) {
        st->init = 1;
        ModifyWorldChunkAnimFlags(3, 0xFC, 0);
        if (IsActorInRunningKillSet(actor) != 0) {
          ModifyWorldChunkAnimFlags(4, 0xFC, 0);
          ModifyWorldChunkAnimFlags(5, 0xFC, 0);
          OrPickupGroupActorFlags(0, 0x40000);
          OrPickupGroupActorFlags(1, 0x40000);
          DespawnPickupGroupActors(2);
          DespawnPickupGroupActors(3);
          CopyVector(&actor->posX,
                        (int *)(st->path + (st->path[0] * 16 - 8)));
          if (actor->dropKind != 0xFF) {
            SpawnActorDeathDrop(actor, 1, 0, 0);
            MarkActorCommittedKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      st->lodHandle = SelectActorLodBand(actor, st->lodHandle);
      actor->flags = 0;
      if (actor->mode < 10) {
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
        ModifyWorldChunkAnimFlags(st->phase + 4, 0xFC, 0);
        actor->mode = 10;
        continue;
      }
      if (st->seen == 0 && dist < 0x6800) {
        st->seen = 1;
        ModifyWorldChunkAnimFlags(3, 0xFC, 0);
      }

      switch (actor->mode) {
      case 0: {
        int sel;
        int d;

        d = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
        sel = 1;
        if (!(PickupGroupHasLiveActor(st->phase) != 0 && d >= 0x2000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x385
                                 : -SPY_DZ(actor) < 0x385))) {
          sel = 2;
          if (st->choice == 1) {
            st->choice = 0;
          }
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (st->choice == 0) {
          int r = RandomIntInRange(0, 100);

          if (sel >= 2) {
            r = RandomIntInRange(0x3C, 100);
          }
          if (r < 0x3C) {
            st->choice = 1;
          } else if (r < 0x55) {
            st->choice = 2;
          } else {
            st->choice = 3;
          }
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          continue;
        }
        switch (st->choice) {
        case 1:
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
            ENTER_POSE(actor, 1);
          }
          continue;

        case 2:
          if (d > 0x800 && d < 0x7000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E8
                                 : -SPY_DZ(actor) < 0x3E8)) {
            st->shots = 0;
            actor->mode = 2;
            continue;
          }
          st->choice = 0;
          continue;

        case 3:
          if (d > 0x2800 && d < 0x7000 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x3E8
                                 : -SPY_DZ(actor) < 0x3E8)) {
            st->count = 0;
            CopyVector(st->aim, &g_anSpyroWorldPos);
            actor->mode = 3;
            continue;
          }
          st->choice = 0;
          continue;
        }
        continue;
      }

      case 1:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = 0;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 2:
        ANIM_GO(actor, 0xC);
        if ((unsigned)(actor->nextFrame - 5) < 0x10) {
          Actor *o = g_pfnLevelSpawnActor(0x52, actor);
          PodShotState *sh = o->state;
          int d;

          RefreshActorDerivedState(actor, 4);
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
          o->rotZ = ArcTan2(g056[0] - o->posX, g056[1] - o->posY, 0);
          o->rotY = ArcTan2(ApproxDist2D(&o->posX, g056),
                                   g056[2] - o->posZ, 0);
          sh->timer = 0xB4;
          sh->heading = st->phase + 2;
          sh->mode = 0;
          st->shots += g_nFrameStep;
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = 0;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 3:
        ANIM_GO(actor, 0xD);
        if ((unsigned)(actor->nextFrame - 4) < 5 && st->count >= 7) {
          Actor *o = g_pfnLevelSpawnActor(0x46, actor);
          LobbedBoltState *b;

          CopyVector(&o->posX, st->hand);
          b = o->state;
          b->timer = 0xF0;
          o->rotZ = ArcTan2(st->aim[0] - o->posX, st->aim[1] - o->posY,
                                   0);
          o->cullRadius = 0x28;
          st->count = 0;
          b->heading = st->phase + 2;
          b->dvz = -7;
          b->speed = (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) >> 8) + 0x190;
          b->vz = ComputeLobVelocityZ(&o->posX, st->aim, b->speed, b->dvz, &b->flightTime);
        }
        if (actor->nextFrame < 3) {
          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 5, st->hand);
        }
        st->count += g_nFrameStep;
        if (g_nActorAnimDone != 0) {
          st->timer = 0x50;
          st->choice = 0;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 10: {
        int h = ArcTan2(PATH_X(st->path, st->path[1]) - actor->posX,
                              PATH_Y(st->path, st->path[1]) - actor->posY, 0);

        OrPickupGroupActorFlags(st->phase, 0x40000);
        if (st->phase == 1) {
          SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          actor->mode = 0x14;
          continue;
        }
        ANIM_GO(actor, 4);
        if (TurnActorYawTowardAngle(actor, h, 6, 8, 1) != 0) {
          actor->subMode = 0;
          actor->mode++;
          continue;
        }
        continue;
      }

      case 11:
        ANIM_GO(actor, 8);
        if (g_nActorAnimDone != 0) {
          ANIM_THEN(actor, 9);
          st->timer2 = 0x46;
          actor->mode++;
          continue;
        }
        continue;

      case 12:
        PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0, 3, 5, 0xFF, 5);
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          actor->mode++;
          continue;
        }
        continue;

      case 13:
        ANIM_GO(actor, 0xA);
        if (g_nActorAnimDone != 0) {
          ANIM_THEN(actor, 5);
          actor->mode++;
          continue;
        }
        continue;

      case 14:
        if (PivotActorAlongPath(actor, st->path, 0x15E, 0x118, 0, 8, 0x80, 0xFF, 5) ==
            st->pathEnd[st->phase] + 0x100) {
          actor->mode++;
          continue;
        }
        continue;

      case 15:
        if (TrackEntityYawTowardSpyro(actor, 6, 4, 1) != 0) {
          actor->group = ++st->phase;
          SetActorGroupSubstate(actor, 1);
          actor->group = 0xFF;
          actor->subMode = 0;
          ENTER_POSE(actor, 0);
        }
        continue;

      case 20:
        ANIM_GO(actor, 0xB);
        if (g_nActorAnimDone != 0) {
          ModifyWorldChunkAnimFlags(3, 0xFC, 0);
          RefreshActorDerivedState(actor, 4);
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
    case 57: {
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
            RaiseSpyroTriggerInRadius(&actor->posX, 0xDC, 0x86) == 0 &&
            CastRayWorldAndActors(g136, &actor->posX) == 0) {
          continue;
        }
        for (n = 0; n < 8; n++) {
          Actor *o;
          BoulderState *q;

          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          o = g_pfnLevelSpawnActor(0x39, actor);
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
    case 70: {
      LobbedBoltState *st = actor->state;
      int i;

      switch (actor->mode) {
      case 0:
        actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
        actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
        if (TickCountdownByFrameDelta(&st->timer, 2) != 0 ||
            RaiseSpyroTriggerInRadius(&actor->posX, 0xC8, 0x26) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        OrActorGroupFlagsInCone(actor, st->heading, 0xC0000, 0x258, 0x4B0, 0x4B0, 0x28);
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x168, 0x168) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ += st->vz;
        st->vz += st->dvz;
        actor->rotY = ArcTan2(st->speed, st->vz, 0);
        break;

      case 1:
        i = 0;
        do {
          i++;
          g168[0] = (GetRandomU32() & 0x3E) - 0x1F;
          g168[1] = (GetRandomU32() & 0x3E) - 0x1F;
          g168[2] = GetRandomU32() & 0xF;
          CopyVector(g184, g168);
          LShiftVector3(g184, 2);
          AddVector(g184, g184, &actor->posX);
          g_pfnLevelEmitParticles(1, 0xD, g184, (void *)g168);
        } while (i < 8);
        DespawnActorRecord(actor);
        continue;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 82: a thrown shot. Flies until it hits the floor, a wall or
     * times out. */
    case 82: {
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
      if (RaiseSpyroTriggerInRadius(&actor->posX, 0x64, 0x26) != 0) {
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
      OrActorGroupFlagsInCone(actor, st->heading, 0xC0000, 0x12C, 0x4B0, 0x258, 0x32);
      continue;
    }

    /* Type 100 (and 102): boulder thrower. Patrols its path, picks up a
     * boulder (57) and hurls it, rides its partner, and charges Spyro. */
    case 102: {
      BoulderThrowerState *st = actor->state;
      int f;

      if (actor->type == 0x66) {
        TickActorFlameGlow(actor);
      }
      f = actor->flags;
      if ((f & 0xF0000) && actor->mode != 3 &&
          (actor->type != 0x66 || f != 0x10000)) {
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
        SpawnActorDeathDrop(actor, 3, 0, 0);
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
          ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
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
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        TrackEntityYawTowardSpyro(actor, 8, 0, 0);
        if ((unsigned)(actor->nextFrame - 3) < 5 && st->rock == 0) {
          st->rock = g_pfnLevelSpawnActor(0x39, actor);
          st->rock->subMode = 0;
        }
        if (st->rock != 0 && st->rock->subMode == 0) {
          RefreshActorDerivedState(actor, 4);
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
          if (actor->type == 0x66) {
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

        CopyVector(g200, &actor->posX);
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
          SpawnActorDeathDrop(actor, 4, 0, 0);
          ENTER_POSE(actor, 3);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apSaveMenuAnimActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == 0x41 &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x384 &&
                AbsAngleDelta8(st->heading,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= 0xC0000;
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
        RefreshActorDerivedState(p, 4);
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

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        RefreshActorDerivedState(p, 4);
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
                                  spy->posY - actor->posY, 0);
          st->speed = 0x1C2;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 101: gatekeeper. Guards a run of keys (g_pActorListBase actors) and
     * releases them as Spyro approaches; rides its partner and charges. */
    case 170: {
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
        SpawnActorDeathDrop(actor, 1, 0, 0);
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
        if (actor->type == 0xAA) {
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
          ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
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
          RefreshActorDerivedState(actor, 4);
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

        CopyVector(g200, &actor->posX);
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
          SpawnActorDeathDrop(actor, 4, 0, 0);
          ENTER_POSE(actor, 5);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 || r == 2 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) > 0x9470) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x34);
          SpawnActorDeathDrop(actor, 4, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          continue;
        } else {
          Actor **lp = g_apSaveMenuAnimActorList;
          Actor *o;

          while ((o = *lp++) != 0) {
            if (o->type == 0x41 &&
                (actor->posX - o->posX > 0 ? actor->posX - o->posX < 0xBB8
                                           : o->posX - actor->posX < 0xBB8) &&
                (actor->posY - o->posY > 0 ? actor->posY - o->posY < 0xBB8
                                           : o->posY - actor->posY < 0xBB8) &&
                ApproxDist2D(&actor->posX, &o->posX) < 0x4B0 &&
                AbsAngleDelta8(st->heading,
                              ArcTan2(o->posX - actor->posX,
                                            o->posY - actor->posY, 0)) < 0x28) {
              o->flags |= 0xC0000;
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
        RefreshActorDerivedState(p, 4);
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

      case 54: {
        Actor *p = &g_pActorListBase[st->rider];

        TurnActorYawTowardAngle(actor, st->heading, 6, 0, 0);
        RefreshActorDerivedState(p, 4);
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
                                  spy->posY - actor->posY, 0);
          st->speed = 0x1F4;
          st->timer = 0xB4;
          st->zSpeed = 0;
          actor->mode = 0x2B;
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 65: a pod. Raises a ring of four bobbing platforms (210) and
     * lobs shots (82) at the spot its partner watches; a charge or a
     * flame-and-charge pops it. */
    case 65: {
      PlatformPodState *st = actor->state;
      int n = 4;
      int i;

      TickActorFlameGlow(actor);
      if ((actor->flags & 0x20000) && actor->mode < 2) {
        PlayActorMeshVoice(actor, 0, 0);
        actor->posZ -= 0x190;
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x5D, actor);
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
      if ((actor->flags & 0xC0000) == 0xC0000) {
        PlayActorMeshVoice(actor, 0, 0);
        actor->posZ -= 0x190;
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x5D, actor);
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
      case 0:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x46;
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->timer, 4) == 0 || g_nActorAnimDone == 0) {
          break;
        }
        st->timer = st->period;
        {
          for (i = 0; i < n; i++) {
            BobbingPlatformState *b;

            st->kids[i] = g_pfnLevelSpawnActor(0xD2, actor);
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
        actor->mode = 2;
        ANIM_RESET(actor, 2);
        continue;

      case 2:
        if (*(int *)g_pActorListBase[st->partner].state == actor->group - 2 &&
            TickCountdownByFrameDelta(&st->shotTimer, 4) != 0) {
          Actor *o = g_pfnLevelSpawnActor(0x52, actor);
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
          o->rotZ = ArcTan2(g200[0] - o->posX, g200[1] - o->posY, 0);
          o->rotY = ArcTan2(ApproxDist2D(&o->posX, g200),
                                   g200[2] - o->posZ, 0);
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
        StopSoundVoicesByOwner(actor, 1);
        PlayActorMeshVoice(actor, 1, 0);
        RESET_IDLE(actor);
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 93:
    case 255:
    case 256:
    case 423:
    case 424: {
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
        g_pfnLevelEmitParticles(1, 1, &actor->posX, g200);
      }

      st->life -= 1;
      break;
      }
      g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Falling chest fragments (TumbleState). */
    case 69:
    case 151:
    case 257:
    case 425: {
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
          RefreshActorDerivedState(actor, 4);

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

      if (!actor->drawn && dist > 0x2000 && actor->subMode == 1 && st->sub == 0) {
        break;
      }

      switch (actor->subMode) {
      case 0: {
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

        actor->subMode = 1;
        break;
      }
      case 1: {
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

            UnpackWorldCollisionTri(st->groundTri, g376[0]);

            AddVector(g376[0], g376[0], g376[1]);
            AddVector(g376[0], g376[0], g376[2]);

            actor->posX = g376[0][0] / 3;
            actor->posY = g376[0][1] / 3;
            actor->posZ = g376[0][2] / 3;

            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);

          } else if (st->sub == 3) {
            int colIndex;

            CopyVector(g344, &actor->posX);
            g344[2] += 0x400;

            RefreshActorDerivedState(actor, 2);

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

                    g_pSparxActor->subMode = 4;
                    cs->owner = actor;
                  }
                }
              }
            }
          }
        }

        break;
      }
      case 2: {
        int speed;

        ScaleVector3Sat(g232, st->vel, g_nFrameStep);
        RShiftVector3(g232, 1);

        speed = VectorLength(g232, 1);

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
                  (signed char)ArcTan2_8bit(g416[2], VectorLength(g416, 0));

              if ((g232[2] - 400) < groundHeight && groundAngle < 24) {
                actor->subMode = 1;
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

              PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

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

                  g_pSparxActor->subMode = 4;
                  cs->owner = actor;
                }
              }
            }
          }
        }

        break;
      }
      case 3: {
        if (st->timer >= 32) {
          CopyVector(&actor->posX, &SPY.posX);
        } else {

          SubtractVector(g432, &SPY.posX, st->vel);
          RShiftVector3(g432, 5);

          if (VectorLength(g432, 1) > 480) {
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

      case 4: {
        continue;
      }
      }
      if (actor->subMode < 3) {
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
          (actor->subMode == 3 && st->timer > 64)) {

        if (g_pSparxActor != 0) {
          ChaserState *cs = (ChaserState *)g_pSparxActor->state;

          if (cs->owner == actor) {
            cs->owner = 0;
          }
        }

        if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
          RefreshActorDerivedState(actor, 4);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)8);
          g_pfnLevelEmitParticles(16, 70, &actor->posX, (void *)0x10);

          if (g_nExtraLives < 99) {
            g_nExtraLives++;
          }

          CommitActorKillGems(0, actor);
          DespawnActorRecord(actor);
          continue;
        } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
          PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 16, 0);
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

    /* Dragon-pad fairy (PoleState). */
    case 110: {
      PoleState *st = (PoleState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->idxA];
      Actor *tgtB = &g_pActorListBase[st->idxB];

      switch (actor->mode) {
      case 0: {
        actor->collision = 0;
        if (ApproxDist2D(&tgtB->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;
      }

      case 1: {
        if (tgtA->mode >= 0x80u) {
          actor->mode = 2;
          st->timer = 16;
          st->baseZ = actor->posZ;
        }
        break;
      }

      case 2: {
        if (st->timer != 0) {
          st->timer--;
        } else {
          actor->collision = 0;

          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            if (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 2048) {
              if (st->flag24 == 0 || tgtB->mode == 1) {
                st->flag24 = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->cullRadius = 0x20;
                actor->mode = 3;
                actor->scale = 0x60;

                g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

                RefreshActorDerivedState(actor, 1);
                FindGroundHeightBelow(&actor->posX, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= 0x80000000;
              }
            }
          }
        }
        break;
      }

      case 3: {
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->t20 = 30;
        }
        break;
      }

      case 4: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 1024 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = 6;
            st->flag24 = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > 0xC00)) {
          actor->mode = 6;
        } else {
          st->t20--;
          if (st->t20 == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + 0x140;
            char finalAngle = (ArcTan2(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, 0) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->tgt, &tgtB->posX);

            actor->mode = 5;

            st->tgt[0] -= (LookupCosine(finalAngle * 0x10) * baseDistance) >> 12;
            st->tgt[1] -= (LookupSine(finalAngle * 0x10) * baseDistance) >> 12;

            st->angle = ArcTan2(SPY.posX - st->tgt[0],
                                      SPY.posY - st->tgt[1], 0);
            st->t20 = 0;
          }
        }
        break;
      }
      case 5: {
        st->timer++;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((tgtB->mode == 0 || (tgtB->mode == 2 && tgtB->subMode >= 0x10)) &&
            ApproxDist2D(&tgtB->posX, &SPY.posX) < 0x400 &&
            ABS2(tgtB->posZ - tgtB->floorOffset - SPY.posZ) < 512) {

          if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_TEETER) && SPY.stateTimer > 0) {
            actor->mode = 6;
            st->flag24 = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2800 ||
            ABS2(actor->posZ - actor->floorOffset - SPY.posZ) >= 0xC01) {
          actor->mode = 6;
        } else {
          int angleDiff;
          int cosWeight1;
          int cosDelta;
          int totalWeight;

          st->t20++;

          SubtractVector(g296, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (g296[0] * cosDelta) / totalWeight;
          actor->posY += ((g296[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->t20 == 0x10) {
            actor->mode = 4;
            st->t20 = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }

        break;
      }

      case 6: {
        if (actor->scale < 96) {
          actor->scale += 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 16;

          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (void *)0xC);

          actor->groundCode &= 0x7FFFFFFF;
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

      if (g_anSparxHealthBlock[0] >= 2) {
        g296[0] = 0;
        g296[1] = 0x64;
        g296[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g296, g296);
        AddVector(g296, g296, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g296, 0);
        g296[0] = 0;
        g296[1] = -0x64;
        g296[2] = 0;
        ApplyActiveGteRotation(g296, g296);
        AddVector(g296, g296, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g296, 0);
      }

      if (g_anSparxHealthBlock[0] >= 3) {
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

      if (g_anSparxHealthBlock[0] <= 0) {
        if (st->target != 0 && st->target->type == ACTOR_TYPE_BUTTERFLY) {
          st->target->mode = 0;
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

      if (actor->subMode != 0x63) {
        if (st->target != 0) {
          switch (actor->subMode) {
          case 0: {
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
            dist = VectorLength(g344, 1);

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
            TurnActorYawTowardAngle(actor, ArcTan2(g344[0], g344[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(g344, 0), g344[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_anSparxHealthBlock[0] += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            actor->subMode += 1;
            DespawnActorRecord(st->target);
            break;

          case 2:
            if (g_nActorAnimDone != 0) {
              if (actor->nextAnim != actor->nextAnim - 1) {
                g_nActorAnimDone = 0;
                ANIM_ADVANCE(actor, actor->nextAnim - 1);
              }
              st->target = 0;
              actor->subMode = 0;
            }
            break;

          case 4: {
            /* Fetching a pickup: once close it homes in on Spyro. */
            CollectibleState *pk = st->target->state;
            int dist;

            SubtractVector(g448, &st->target->posX, &actor->posX);
            dist = VectorLength(g448, 1);

            if (dist < 0x200) {
              pk->timer = 0;
              pk->lean = GetRandomU32() & 0xE;
              pk->roll = GetRandomU32() & 0xE;
              pk->spin = GetRandomU32() & 0xE;
              CopyVector((int *)pk, &st->target->posX);
              st->target->subMode = 3;

              if (st->target->type == ACTOR_TYPE_EXTRA_LIFE) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType14);
              } else if (st->target->type == ACTOR_TYPE_SPARX_FOOD) {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_dwBurstColorType15);
              } else {
                g_pfnLevelEmitParticles(1, 0xC, actor, g_adwActorBurstColors[st->target->type]);
              }
              st->target = 0;
              EBB_BREAK();

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[0x2F]) == 0) {
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(g448, dist, lim);
              } else {
                ScaleVector3ByRatio(g448, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g448);
              TurnActorYawTowardAngle(actor, ArcTan2(g448[0], g448[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(g448, 0), g448[2], 0);
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
            TurnActorYawTowardAngle(actor, g_bSpyroPersistentEulerZ, 4, 0, 0);
          }

          switch (SPY.sparxHealth) {
          case 1:
            if (actor->nextAnim != 0 && actor->anim != 0) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 0);
            }
            break;

          case 2:
            if (actor->nextAnim != 2 && actor->anim != 2) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 2);
            }
            break;

          case 3:
            if (actor->nextAnim != 4 && actor->anim != 4) {
              g_nActorAnimDone = 0;
              ANIM_RESET(actor, 4);
            }
            break;
          }
        }

        RefreshActorDerivedState(actor, 4);
      }

      break;
    }

    /* Types 152/153: sparking debris. Falls under gravity, spinning, and
     * pops when it runs out of life or reaches the floor. */
    case 152:
    case 153: {
      DebrisFallState *st = actor->state;

      if (actor->drawn == 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (--actor->subMode == 0 || actor->posZ < st->floor) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (void *)0x10);
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
        g_pfnLevelEmitParticles(1, 0x11, g464, g488);
      }
      break;
    }

    /* Ambient flyer (FlyState): circles its anchor and pops near Spyro. */
    case 173: {
      FlyState *st = actor->state;

      if (actor->subMode == 0) {
        actor->rotX = (unsigned short)g_anCosineLut[st->angle] >> 7;
        actor->rotY = (unsigned short)g_anSineLut[st->angle] >> 7;
        st->angle += g_nFrameStep * 2;

        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x200) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) {
            int i;

            PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
            RefreshActorDerivedState(actor, 4);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, 0xC, actor, (int *)(0x8080 + (i << 24)));
            }

            g_nLevelEggCount = 1;
            actor->scale = 0x40;
            actor->cullRadius = 0;
            actor->updateDist = 0;
            actor->drawn = 0;
            actor->groundCode = 0;
            actor->visZone = 0xFF;
            actor->subMode = 2;
          }
        }

        if (st->attach != 0xFF) {
          RefreshActorDerivedState(actor, 4);
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

    /* Type 174: the cage that carries Spyro (CageState). Shakes when hit;
     * once its flyer is freed, Spyro steps in and it lifts him up and away,
     * bursting when the ride ends. */
    case 174: {
      CageState *st = actor->state;
      Actor *fly = &g_pActorListBase[st->idx];

      if (actor->flags & 0x80000) {
        actor->subMode = 2;
        st->shake = 0xD2;
      }

      if (actor->subMode < 2) {
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
        } else if (actor->flags & 0xB0000) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      switch (actor->subMode) {
      case 0:
        if (g_nLevelEggCount == 0 || g_nLevelEggCount == 3) {
          g_nLevelEggCount = 0;
          actor->updateDist = sixteen;
          fly->updateDist = 0x40;
          fly->cullRadius = 0x18;
        } else {
          g_nLevelEggCount = 1;
          actor->subMode = 1;
          fly->scale = 0x40;
          fly->groundCode = 0;
          fly->visZone = 0xFF;
          fly->subMode = 2;
        }
        break;

      case 1: {
        int height;
        int drop;

        if (g_nLevelEggCount == 1 &&
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
            actor->subMode = 2;
            if (st->shake > 0) {
              st->shake = 0;
              actor->rotX = st->rotX;
              actor->rotY = st->rotY;
              actor->posZ = st->restZ;
            }
            fly->rotX = 0;
            fly->rotY = 0;
            fly->rotZ = spy->bodyRotZ + 0x80;
          }
        }
        break;
      }

      case 2:
        if (g_dwPadPressed & PAD_CROSS) {
          st->shake = 0xD2;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        fly->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nGameplayBlocked = 1;

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
          CopyVector(&fly->posX, g504);
          g_nCameraScriptStickRequestY[0] = g_anSineLut[(st->shake << 7) / 192] >> 3;

          if (st->shake < 0x40) {
            int rot;

            fly->rotY = -st->shake;
            rot = g_bSpyroPersistentEulerZ + 0x80;
            fly->rotZ = rot + st->shake * 4;
          } else if (st->shake < 0x80) {
            int rot;

            fly->rotY = 0xC0;
            rot = g_bSpyroPersistentEulerZ + 0x80;

            fly->rotZ = rot + (st->shake - 0x40) * 3;
          } else {
            int turn = AbsAngleDelta8(actor->rotZ,
                                     (unsigned char)(g_bSpyroPersistentEulerZ + 0x40)) *
                       (st->shake - 0x80);
            int rot = g_bSpyroPersistentEulerZ + 0x40;

            fly->rotZ = rot + turn / 64;
          }
        } else if (st->shake < 0xD0) {
          fly->rotZ = actor->rotZ;
          g520[0] = 0x200 - (st->shake - 0xC0) * 8;
          g520[1] = 0;
          g520[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g520, g520);
          AddVector(&fly->posX, g520, &actor->posX);
        }

        if (st->shake >= 0xD0 ||
            !(SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
              SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_STOP)) {
          int i;

          fly->cullRadius = 0;
          fly->updateDist = 0;
          fly->drawn = 0;
          g_nGameplayBlocked = 0;
          actor->soundRange = 0x20;
          PlayActorMeshVoice(actor, 0, 0);

          for (i = 0; i < 4; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }

          for (i = 0; i < 10; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }

          g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, (void *)0x18);
          g_pThiefEggDrop = SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          g_nLevelEggCount = 2;
          g_nCameraScriptStickRequestX = 0;
          g_nCameraScriptStickRequestY[0] = 0;
          g_nSpyroSwingEulerPitchTarget = 0;
        }

        RefreshActorDerivedState(fly, 4);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, 0xC, (int *)fly, (void *)0x5008080);
        }
        break;
      }

      st->handle = SelectActorLodBand(actor, st->handle);
      actor->flags = 0;
      continue;
    }

    /* Wooden chest (BreakState). */
    case 194: {
      BreakState *st;
      int i;

      st = (BreakState *)actor->state;
      if (st->snapToGround != 0) {
        SnapActorToGround(actor);
      }
      if ((actor->flags & 0xB0000) == 0) {
        continue;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 0, 0);

      /* Two of each of the first pair, then six of the third. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0xFF, actor);
        g_pfnLevelSpawnActor(0x100, actor);
      }
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x101, actor);
      }

      g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
      SpawnActorDeathDrop(actor, 3, 0, 0);
      MarkActorCommittedKill(actor);
      DespawnActorRecord(actor);
      continue;
    }

    /* Wobbling chest (WobbleState): plays a recoil pose, then breaks. */
    case 195: {
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

      if (actor->flags & 0xA0000) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }

        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);

        if ((actor->flags & 0x10000) && st->phase == 0) {
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
    case 210: {
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

      if (actor->mode == 0) {
        if (st->partner != -1) {
          g_pActorListBase[st->partner].mode = 3;
        }
        actor->mode = 1;
        st->rotX = actor->rotX;
        st->rotY = actor->rotY;
        st->restZ = actor->posZ;
        break;
      }

      if (actor->mode == 1) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x101) {
          st->phase = 0;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          actor->flags = 0;
          StopSoundVoicesByOwner(actor, 1);
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
          StopSoundVoicesByOwner(actor, 1);
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

          if (VectorLength(g536, 1) < 0x440) {
            int facing;

            facing =
                st->partner != -1 ? g_pActorListBase[st->partner].rotZ : st->facing;
            CommitActorKillGems(0, actor);
            SetSpawnAnchorFromActor(actor, facing);

            if (st->reward != -1) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;
              StubRewardHook(st->reward, 0);

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            if (st->cutsceneId == st->reward) {
              g_anLevelDragonsRescued[g_nLevelIntroIndex] += 1;
              g_nWorldDragonsRescued += 1;

              if (st->partner != -1) {
                g_pActorListBase[st->partner].mode = 1;
              }
              DespawnActorRecord(actor);
              continue;
            }

            actor->mode = 2;
            actor->rotX = st->rotX;
            actor->rotY = st->rotY;
            actor->posZ = st->restZ;
            BeginGemPickupOverlay(actor);
            ZeroVector(&SPY.headLook);
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z | SPYRO_RQ_RAW_VELOCITY | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT);

            if (g_nSpyroAirborneFrames != 0) {
              g_nSpyroRequestState = 6;
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

              g_nSpyroRequestState = 3;
              CopyVector(look, &SPY.motionX);
              RShiftVector3(look, 6);
              g_nSpyroRequestVelocityZ = 0;

              if (SPY.requestVelX != 0 || g_nSpyroRequestVelocityY != 0) {
                ScaleVector3ByRatio(look, VectorLength(look, 0), 0x60);
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
    case ACTOR_TYPE_STATUE_SHARD:      TickGemPickupBurstEntity(actor);
      break;

    /* Floating digits (DebrisState). */
    case 260:
    case 261:
    case 262:
    case 263:
    case 264:
    case 265:
    case 266:
    case 267:
    case 268:
    case 269: {
      DebrisState *st = actor->state;

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
          g_pfnLevelEmitParticles(10, 0x47, &actor->posX, 0);
          DespawnActorRecord(actor);
        }
      }
      break;
    }

    /* Camera trigger volume (TriggerState). */
    case 300: {
      TriggerState *st;
      SpyroObj *spyro;
      int distance;
      int flags;
      st = (TriggerState *)actor->state;
      if (st->flags & 8) {
        if (g_pActorListBase[st->idx].type == 0xAD) {
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
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[54], actor, 8, &actor->sfxSlot);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, 6, (int *)actor, 0);
      SubtractVector(g536, &SPY.posX, &actor->posX);
      distance = VectorLength(g536, 0);
      if (distance < st->radius && g536[2] >= -0x3FF && g536[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(SPY.firstContactPoint, &actor->posX);
        if (st->flags & 1) {
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
      if ((st->flags & ~1) == 4 && st->timer != 0 &&
          TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        st->flags = (st->flags & ~4) | 2;
      }
      break;
    }

    /* Sparkling shard: 1 puff per frame. */
    case 309: {
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
        g_pfnLevelEmitParticles(1, 0x42, g552, (int *)1);
        st->life -= 1;
      } else {
        DespawnActorRecord(actor);
        continue;
      }

      break;
    }

    /* Wood shard: bounces and throws three sparks a frame. */
    case 310:
    case 311: {
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
            g_pfnLevelEmitParticles(1, 0x42, g568, (int *)1);
          } while (i < 3);
        }

        st->life -= 1;
        break;
      }

      if (actor->drawn != 0) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
      }
      DespawnActorRecord(actor);
      continue;
    }

    /* Type 312: cracking boulder. Shakes and smokes when hit by a charge or
     * flame, then bursts into three rings of debris and knocks the camera
     * back. */
    case 312: {
      L16Boulder *st = actor->state;

      switch (actor->subMode) {
      case 0:
        *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
        actor->subMode = 1;
        break;

      case 1:
        if (actor->flags & 0x40000) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & 0x10000) {
          actor->subMode = 2;
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
        if (actor->flags & 0xA0000) {
          actor->subMode = 3;
          actor->cullRadius = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;

      case 2: {
        int t = st->timer + g_nFrameStep;
        int i;
        int n;

        st->timer = t;
        if (t < 0x78) {
        if ((actor->flags & 0x40000) && t < 0x5A) {
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
          g_pfnLevelEmitParticles(1, 0x4A, g584, g584 + 3);
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
          g_pfnLevelEmitParticles(1, 0x10, g584, g608);
          if (actor->sfxSlot == 0x7F) {
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
          actor->subMode = 3;
          actor->cullRadius = 0;
          actor->drawn = 0;
          st->timer = 0;
        }
        break;
      }

      case 3:
        if (st->timer == 0) {
          int i;
          Actor *c;
          DebrisFallState *cs;

          PlayActorMeshVoice(actor, 1, 0);
          SpawnActorDeathDrop(actor, 6, 0, 0);
          for (i = 0; i < 8; i++) {
            short *p;

            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            p = g_pfnLevelSpawnActor(0x97, actor)->state;
            p[0] >>= 1;
            p[1] >>= 1;
          }
          for (i = 0; i < 16; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            c = g_pfnLevelSpawnActor(0x99, actor);
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
            c = g_pfnLevelSpawnActor(0x98, actor);
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
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, 0x86) != 0) {
          st->pushed = 1;
          g_anSpyroRequestVelocity.x = SPY.posX - actor->posX;
          g_nSpyroRequestVelocityZ = 0;
          g_nSpyroRequestVelocityY = SPY.posY - actor->posY;
          if (g_anSpyroRequestVelocity.x != 0 || g_nSpyroRequestVelocityY != 0) {
            int len = VectorLength(&g_anSpyroRequestVelocity.x, 1);

            ScaleVector3ByRatio(&g_anSpyroRequestVelocity.x, len,
                          (0x1000 - ApproxDist2D(&actor->posX, &SPY.posX)) /
                              48);
          }
          g_nSpyroRequestVelocityZ = 0x46;
        }
        DispatchActorContactAtSphere(&actor->posX, (st->timer + 1) << 8, 0, 0x50000, actor, 0);
        if (++st->timer >= 8) {
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
        }
        break;
      }
      actor->flags = 0;
      break;
    }

    /* Respawn warden (WardenState): puts its group back once Spyro has left. */
    case 323: {
      WardenState *st = actor->state;

      CopyVector(&actor->posX, &SPY.posX);

      if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
        Actor *scan;

        st->timer = st->period;

        for (scan = g_pActorListBase; scan < g_pActorPoolDynBase; scan++) {
          if (scan->type == st->group && scan->mode >= ACTOR_MODE_DORMANT &&
              ApproxDist2D(&SPY.posX, scan->state) >= 0x6001) {
            CopyVector(&scan->posX, scan->state);
            InsertActorIntoSpatialGrid(scan);
            scan->flags = 0;
            scan->mode = 0;
            scan->killGroup &= 0x7F;
            ANIM_RESET(scan, 0);
          }
        }
      }

      break;
    }

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case 329: {
      L16Pod *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case 0:
        if (actor->subMode == 0) {
          *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
          actor->subMode = 1;
          actor->updateDist = 0x10;
        }
        if (actor->flags & 0xB0000) {
          st->timer = 0;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
          actor->updateDist = 0x40;
          st->prize = SpawnActorDeathDrop(actor, 5, 0, 0);
          st->prize->subMode = 4;
          st->prize->cullRadius = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, 0x40, &actor->posX, (void *)(st->prize->type - 0x53));
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->timer >= 0xF8) {
          SpawnActorRotBillboard(actor, g_anRotBillboardOffsetsRand[GetRandomU32() & 3]);
          st->timer = (GetRandomU32() & 0x3F) + 0x18;
        }
        st->timer += g_nFrameStep;
        break;

      case 1: {
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
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, g608);
        }
        if ((ApproxDist2D(&st->prize->posX, &spy->posX) < 0x1E0 &&
             (unsigned int)(SPY.posZ - st->prize->posZ + 0xDF) < 0x2BF) ||
            g_nSpyroState == SPYRO_ST_SUPERCHARGE || g_nSpyroState == SPYRO_ST_SUPERCHARGE_AIR ||
            (g_nSpyroState == SPYRO_ST_CHARGE_AIR && (g_nSpyroStateFlags & 0x40))) {
          CollectPickupActor(st->prize);
          DespawnActorRecord(st->prize);
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
          DespawnActorRecord(actor);
          continue;
        }
        if (actor->flags & 0x80000) {
          st->prize->subMode = 2;
          PlayActorMeshVoice(actor, 2, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x43, actor);
            g_pfnLevelSpawnActor(0x44, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x45, actor);
          }
          g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x20);
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
            g_pfnLevelEmitParticles(1, 0, g624, g640);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= 0x7F;
          actor->updateDist = 0x10;
          actor->rotX = st->rotX;
          actor->rotY = st->rotY;
          actor->posZ = st->restZ;
          st->timer = 0;
          PlayActorMeshVoice(actor, 1, 0);
          actor->mode = 0;
          ANIM_RESET(actor, 0);
          continue;
        }
        break;
      }
      }
      actor->flags = 0;
      break;
    }

    /* Whirlwind pad: a short spin-up before the ride, then a cooldown. */
    case 331:      switch (actor->mode) {
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

    /* Fade-out props: removed as soon as their animation finishes. */
    case 507:
    case 508:      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Life chest (LiftState). */
    case 421: {
      LiftState *st = actor->state;
      Actor *rider;
      int i;
      int a;
      int b;

      if (st->rideMode == 1) {
        SnapActorToGround(actor);
        EncodeCachedVecToActorDirCode(actor);
      }

      if (actor->mode == 0) {
        st->phase += g_nFrameStep;

        if (st->phase >= 0x1E1) {
          st->phase = 0;
          actor->rotX = st->savedRotX;
          actor->rotY = st->savedRotY;
          actor->posZ = st->savedPosZ;
          actor->mode = 1;
          actor->frameProgress = 8;
          actor->frameRate = 8;
          actor->nextAnim = 1;

          st->passenger = g_pfnLevelSpawnActor(0x1A6, actor);
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

          if (actor->sfxSlot == 0x7F) {
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
          actor->mode = 0;
          actor->nextAnim = 0;
          if (actor->sfxSlot == 0x7F) {
            PlayActorMeshVoice(actor, 3, 0);
          }
        }
      }

      if ((actor->flags & 0xB0000) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, 5, 0, 0);
      if (st->rideMode == 1) {
        ((unsigned char *)rider->state)[0xE] = st->rideMode;
      }

      actor->soundRange = 0x20;
      PlayActorMeshVoice(actor, 4, 0);

      /* Fragment bursts; each stops early once the dynamic pool is full. */
      for (i = 0; i < 2; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A7, actor);
        g_pfnLevelSpawnActor(0x1A8, actor);
      }

      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1A9, actor);
      }

      for (i = 0; i < 8; i++) {
        g608[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g608[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g608[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, g608);
      }

      g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (void *)0x18);

      if (actor->mode == 1) {
        DespawnActorRecord(st->passenger);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[2], actor, 8,
                    (void *)(long)actor->sfxSlot);
      DespawnActorRecord(actor);
      continue;
    }

    /* Orbiting satellite (SatState): rides a spinning parent at a fixed
     * radius, or just spins in place behind a non-spinner. */
    case 76:
    case 426:
    case 427:
    case 428:
    case 429:
    case 430:
    case 431:
    case 432:
    case 433:
    case 434:
    case 435:
    case 436:
    case 437:
    case 438:
    case 439:
    case 440:
    case 441:
    case 442:
    case 443:
    case 444:
    case 445:
    case 446:
    case 447:
    case 448:
    case 449:
    case 450:
    case 451: {
      SatState *st = actor->state;

      if (st->parent->mode == 0 || st->parent->subMode != actor->mode) {
        DespawnActorRecord(actor);
        continue;
      }

      if (st->parent->type == 9) {
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
                                  SPY.posY - st->parent->posY, 0);
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
    case 466: {
      RoamingRamState *st = actor->state;

      if ((actor->flags & 0xF0000) && actor->mode != 3) {
        EBB_BREAK();
        if (actor->flags & 0x40000) {
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
          if (actor->flags & 0x20000) {
            st->speed += 0x46;
            st->lift += 0x32;
          }
          SpawnActorDeathDrop(actor, 3, 0, 0);
          MarkActorCommittedKill(actor);
        }
        if (actor->nextAnim != 3) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 3;
          actor->nextFrame = 0;
          StubActorHook(actor);
        }
        actor->mode = 3;
        continue;
      }
      actor->flags = 0;
      if (st->chan != -1 && g_anLevelTriggerStateSlots[st->chan] != 0 &&
          (unsigned int)(actor->mode - 3) >= 2 &&
          IsPointInRotatedBox(&actor->posX, &st->p2C) != 0) {
        ENTER_POSE(actor, 4);
      }

      switch (actor->mode) {
      case 0:
        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (st->mode == 1 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800) {
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

      case 3:
        DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        if (g_anLevelTriggerStateSlots[st->chan] == 0) {
          st->speed = 0;
          st->lift = 0;
          SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, 3);
        }
        if (actor->nextFrame >= 0xF) {
          actor->frame = 0;
          actor->nextFrame = 1;
          actor->frameProgress = 0;
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
      RefreshActorDerivedState(actor, 1);
      break;
    }
}
  }
}
