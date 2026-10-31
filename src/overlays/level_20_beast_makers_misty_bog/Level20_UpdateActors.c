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


/* Type 286: ambient sound source; mode 2 also coasts along a path. */


/* Type 250: crystal dragon. Shakes and sparkles; when Spyro walks into it,
 * saves a checkpoint and starts the rescue. */

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


/* ==== Level 16 classes ==== */

/* Case 328 -- floor-snapped marker. */

/* ENTER_POSE with the already-there test first. */
/* ==== Level 2 classes ==== */


/* ==== Level 9 classes ==== */

/* ==== level_10 per-class state blocks ==== */


/* ==== Level 20 classes ==== */


/* ==== The function ==== */
void Level20_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  char *base;
  int sixteen;
  int dt;

  /* Scratch vectors, in first-use order. */
  int svA[3];
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int svF[3];
  int acc[3][3];
  int svG[3];
  int svH[3];
  int svI[3];
  int svJ[3];
  int A256[3];
  int A272[3];
  int A288[3];
  int A304[3];
  int A320[3];
  int A336[3];
  int A352[3];
  int A368[2];
  int A376[3];
  int A392[3];
  int A408[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickActorAnims(g_apActiveActorList, 0);

  if (g_nFrameStep >= 3) {
    TickActorAnims(g_apActiveActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apActiveActorList;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    sixteen = 0x10;
    base = (char *)g_anSparxHealthBlock;
    spy = (SpyroObj *)(base - 0x164);

    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Level-exit whirlwind (ExitWhirlState): shows its name as the camera nears,
     * lifts Spyro, and once he is high above it starts the return to the hub. */
    case ACTOR_TYPE_LEVEL_EXIT: {
      ExitWhirlState *st = actor->state;
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

    /* Butterfly (WanderState): flutters about its spawn point. Near a hurt
     * Spyro it becomes Sparx when Sparx is gone, else Sparx comes to eat it. */
    case ACTOR_TYPE_BUTTERFLY: {
      WanderState *st;

      EBB_BREAK();
      st = actor->state;
      if (SPY.sparxHealth < 3 && ApproxDist2D(&actor->posX, (int *)(base - 0x164)) < 0x960) {
        int height = actor->posZ - actor->floorOffset;
        int drop = height - SPY.posZ;

        if (drop > 0 ? drop < 0x708 : (SPY.posZ - height) < 0x708) {
          if (g_pSparxActor == 0) {
            if (g_nSparxHealth >= 0) {
              Actor *sparx = g_pfnLevelSpawnActor(ACTOR_TYPE_SPARX, actor);

              g_pSparxActor = sparx;
              PlayActorMeshVoice(sparx, 0, 0);
              g_nSparxHealth = 1;
              DespawnActorRecord(actor);
              continue;
            }
          } else if (g_pSparxActor->subMode != 0x63) {
            SparxState *sparx = g_pSparxActor->state;

            if (sparx->target == 0) {
              if (WrapAngleDelta8(spy->bodyRotZ,
                                ArcTan2(actor->posX - spy->posX,
                                              actor->posY - spy->posY, 0)) <
                  0) {
                st->turn = 1;
              } else {
                st->turn = -1;
              }
              g_pSparxActor->subMode = 0;
              sparx->target = actor;
              sparx->timer = RandomIntInRange(0x96, 0xD2);
              actor->mode = 1;
            }
          }
        }
      }

      if (!(actor->cullRadius & 0x80)) {
        switch (actor->mode) {
        case 0:
          if (TickCountdownByFrameDelta(&st->wanderTimer, 2) == 0 || actor->drawn != 0) {
            int moved;
            int blocked;
            int step;
            int z;
            unsigned int spd;
            int *at;

            if (TickCountdownByFrameDelta(&st->reTurn, 1) != 0) {
              int turn = RandomSignedInRange(0x1E, 0x5A);

              if (GetRandomU32() & 1) {
                turn = -turn;
              }
              st->heading = WrapAddAngle8(st->heading, turn);
              st->reTurn = RandomIntInRange(0x3C, 0x8C);
            }

            if (TickCountdownByFrameDelta(&st->reStep, 1) != 0) {
              st->speed = RandomSignedInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            spd = st->speed;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            EBB_BREAK();
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < 0x12C) {
              st->speed = RandomIntInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            if ((unsigned int)(step - 0x401) < 0x3FF) {
              st->speed = -RandomIntInRange(0xA, 0x19);
              st->reStep = RandomIntInRange(0x50, 0x8C);
            }

            moved = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
            blocked = MoveActorAlongHeading(actor, 0x1E, 0, 0x32, 1);

            if (moved != 0) {
              if (blocked != 0) {
                st->heading =
                    WrapAddAngle8(st->heading, RandomIntInRange(0x62, 0x9E));
                st->reTurn = RandomIntInRange(0x1E, 0x50);
              }
            }

            if (ApproxDist2D(&actor->posX, st->home) >= 0x7D1) {
              st->heading = ArcTan2(st->home[0] - actor->posX,
                                          st->home[1] - actor->posY, 0);
              st->reTurn = 0x28;
            }
          } else {
            DespawnActorRecord(actor);
            continue;
          }
          break;

        case 1:
          SteerActorAroundPoint(actor, &spy->posX, 0x320,
                        WrapAddAngle8(ArcTan2(actor->posX - spy->posX,
                                                    actor->posY - spy->posY, 0),
                                      st->turn * 8),
                        8, 0x82, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0);

          if (g_pSparxActor == 0 || g_pSparxActor->subMode == 0x63) {
            actor->mode = 0;
            break;
          }

          if (TickCountdownByFrameDelta(&st->reStep, 1) != 0) {
            st->speed = RandomSignedInRange(5, 0x14);
            st->reStep = RandomIntInRange(0x28, 0x50);
          }

          {
            int step;
            int z;
            unsigned int spd;

            spd = st->speed;
            if (spd >= 0x80) {
              z = actor->posZ - 0x100;
            } else {
              z = actor->posZ;
            }
            actor->posZ = z + spd;
            EBB_BREAK();
            step = actor->posZ - ProbeActorGroundHeight(actor);

            if (step < 0xC8) {
              st->speed = RandomIntInRange(5, 0x14);
              st->reStep = RandomIntInRange(0x28, 0x50);
            }

            if ((unsigned int)(step - 0x3E9) < 0x417) {
              st->speed = -RandomIntInRange(5, 0x14);
              st->reStep = RandomIntInRange(0x28, 0x50);
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
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode != 5) {
        if (actor->flags & ACTOR_HIT_SUPER) {
          st->knockSpeed = 0;
          st->knockZSpeed = 0x104;
        } else {
          st->knockSpeed = 0x190;
          st->knockZSpeed = 0xAA;
          st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE_B(actor, 5);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        if (st->mode == 0) {
          ENTER_POSE(actor, 4);
        } else if (st->mode == 1) {
          Actor *p = &g_pActorListBase[st->partner];

          if (p->mode != 0) {
            st->mode = 0;
            ENTER_POSE(actor, 4);
          }
          ANIM_GO(actor, 7);
          PivotActorAlongPath(actor, st->path, 0x100, 0xA0, 0, 8, 0x28, 0xFF, 5);
        } else if (st->mode == 2) {
          ANIM_GO(actor, 0);
          if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x640 &&
              g_pActorListBase[st->partner].mode == 0) {
            ENTER_POSE(actor, 9);
          }
          if (actor->subMode != 0) {
            ENTER_POSE(actor, 1);
          }
        }
        break;

      case 1:
        if (actor->nextFrame >= 4 && g_pActorListBase[st->partner].mode == 0) {
          g_pActorListBase[st->partner].mode = 1;
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 2);
        }
        break;

      case 2:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, 0),
            4, 0, 0);
        if (g_nActorAnimDone != 0) {
          st->timer = 0x78;
          ENTER_POSE(actor, 3);
        }
        break;

      case 3:
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                          g_pActorListBase[st->partner].posY - actor->posY, 0),
            6, 0, 0);
        if ((TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
             ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x514) &&
            g_nActorAnimDone != 0) {
          if (actor->anim != 4) {
            actor->frameProgress = 8;
            actor->frameRate = 8;
            actor->anim = actor->nextAnim;
            actor->nextAnim = 4;
            actor->frame = actor->nextFrame;
            actor->nextFrame = 0;
            OnActorAnimQueued(actor);
          }
          actor->mode = 4;
          continue;
        }
        break;

      case 4: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);
        int r = st->range;

        if (r < 0x20) {
          r <<= 10;
        }
        if (d < r && g_nSpyroState != SPYRO_ST_CHARGE && g_nSpyroState != SPYRO_ST_CHARGE_AIR) {
          st->timer = 0x78;
          ENTER_POSE(actor, 7);
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0x514) {
          st->timer = 0x78;
          ENTER_POSE_B(actor, 8);
        }
        break;
      }

      case 5: {
        int r = DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, 0xC, sixteen);

        if ((g_nGroundSurfaceType & 0x3F) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < 0x64 : h - z < 0x64) {
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->sfxSlot);
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);
            st->timer = 0xA;
            actor->mode = 0xA;
            continue;
          }
        }
        if (r == 3 && st->hit == 0) {
          st->hit = 1;
          PlayActorMeshVoice(actor, 0, 0);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      case 7:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 ||
            ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0x3E8) {
          st->range = 0;
          st->timer = 0;
          ENTER_POSE_B(actor, 8);
        }
        if (TrackEntityYawTowardSpyro(actor, 6, 0x14, 1) != 0 &&
            MoveActorAlongHeading(actor, 0xA0, 0x12C, 0x12C, 0x15) != 0) {
          st->timer = 0;
          ENTER_POSE(actor, 4);
        }
        break;

      case 8:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 9:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 10:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0x64;
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
          actor->mode = 5;
        }
      }

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)0x602080);
      }

      switch (actor->mode) {
      case 1:
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
          actor->mode = 5;
        }
        break;
      }

      case 3: {
        st->phase += g_nFrameStep;

        if (st->phase < 0x40) {
          SVectorToVectorShl2(svB, st->centre);
          SVectorToVectorShl2(svC, st->apex);
          SubtractVector(svC, svC, svB);
          ScaleVector3Sat(svC, svC, st->phase);
          RShiftVector3(svC, 6);
          AddVector(&actor->posX, svB, svC);
          actor->rotZ += g_nFrameStep * 4;
        } else {
          CopyVector(svD, &spy->posX);
          svD[0] += LookupCosine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[1] += LookupSine(g_bSpyroPersistentEulerZ * 0x10) >> 3;
          svD[2] += 0x300;
          VectorToSVectorShr2(st->centre, svD);
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
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, svE, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
        CommitActorKillGems(0, st->idx);
        g_nWorldEggsRecovered += 1;
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Bouncing chest fragments (SparkState). */
    case ACTOR_TYPE_SEED_POD_SHARD:
    case ACTOR_TYPE_SEED_POD_SHARD + 1:
    case ACTOR_TYPE_WOOD_CHEST_SHARD:
    case ACTOR_TYPE_WOOD_CHEST_SHARD + 1:
    case ACTOR_TYPE_LIFE_CHEST_SHARD:
    case ACTOR_TYPE_LIFE_CHEST_SHARD + 1: {
      SparkState *st = actor->state;

      if (st->life > 0 && actor->drawn) {
        if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100)) {
          int *nrm = &g_anCollisionProbeVec;
          int dot;

          SetVector3Magnitude(nrm, 0x1000);
          dot = st->vel[0] * nrm[0] + st->vel[1] * g_nCollisionProbeVecY +
                    st->vel[2] * g_nCollisionProbeVecZ >>
                11;
          if (dot < 0) {
            ScaleVector3ByRatio(nrm, 0x1000, (dot >> 2) - (dot));
            st->vel[0] += nrm[0];
            st->vel[1] += AS_I32(g_nCollisionProbeVecY);
            st->vel[2] += AS_I32(g_nCollisionProbeVecZ);
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
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svD);
        }
        st->life--;
      } else {
        g_pfnLevelEmitParticles(8, EMIT_STREAK_BURST, &actor->posX, (void *)0x10);
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
        if (st->attach != 0xFF) {
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
            st->attach = 0xFF;
          }
        } else if (st->timer >= 0xF8) {
          int handle = SpawnActorRotBillboard(actor, &g_anRotBillboardOffset3);

          if (handle >= 0 && toSpyro < 0x4000) {
            st->attach = handle;
          }
          st->timer = (GetRandomU32() & 0x1F) + 0x21;
        }
      }

      if (actor->drawn != 0 || toSpyro <= 0x2000 || actor->subMode != 1 ||
          st->mode != 0) {
        switch (actor->subMode) {
        case 0:
          /* Idle: sample the floor normal and face along it. */
          CopyVector(svA, &actor->posX);
          svA[2] += 0x400;
          FindGroundHeightBelow(svA, 0x10000);
          {
            int *nrm = &g_anCollisionProbeVec;

            st->lean = -ArcTan2_8bit(
                IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                g_nCollisionProbeVecY);
            st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
          }

          if (st->lean != 0 || st->roll != 0) {
            actor->rotZ = 0;
          }
          actor->subMode = 1;
          goto enemy_pose;

        case 1: {

          if (st->mode == 0) {
          } else if (st->mode == 1) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= 83) {
              {
                int *nrm = &g_anCollisionProbeVec;

                st->lean =
                    -ArcTan2_8bit(IntegerSqrt((nrm[0] * nrm[0]) +
                                                 (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                                   g_nCollisionProbeVecY);
                st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
              }
            }
          } else if (st->mode == 2) {
            UnpackWorldCollisionTri(st->groundTri, acc[0]);
            AddVector(acc[0], acc[0], acc[1]);
            AddVector(acc[0], acc[0], acc[2]);
            actor->posX = acc[0][0] / 3;
            actor->posY = acc[0][1] / 3;
            actor->posZ = acc[0][2] / 3;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
              st->groundTri = g_nLastCollisionHitChunkId;
              st->mode = 2;
            }
          }

          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pSparxActor != 0 && g_nSpyroAirborneFrames == 0 &&
                CheckLineOfSightSegment(&g_pSparxActor->posX, &actor->posX) != 0) {
              {
                SparxState *sparx = g_pSparxActor->state;

                if (sparx->target == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pSparxActor,
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pSparxActor, 0, 0);
                  }
                  g_pSparxActor->subMode = 4;
                  sparx->target = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

        case 2: {
          /* Knocked back: arc away, bounce, then settle. */
          int *at;
          int dist;

          ScaleVector3Sat(svG, (int *)st, g_nFrameStep);
          RShiftVector3(svG, 1);
          dist = VectorLength(svG, VEC_LEN_XYZ);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(svG, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(svG, at, svG);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (svG[2] < 0) {
            if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          svG[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(svG, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(svH, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(svG, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, VEC_LEN_XY));

              if ((svG[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, svG);
                actor->posZ = floor;
                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((svH[0] * svH[0]) + (svH[2] * svH[2])),
                      svH[1]);
                  st->roll = -ArcTan2_8bit(svH[2], svH[0]);
                  if (st->lean != 0 || st->roll != 0) {
                    actor->rotZ = 0;
                  }
                }
                EncodeCachedVecToActorDirCode(actor);
              } else {
                st->bounce += 1;
              }

              if (st->bounce == 0) {
                goto enemy_settle;
              }
            }

            g_nSfxOverrideFlags = 1;
            g_nSfxOverrideVolL = 0x3CCC >> (3 - st->bounce);
            g_nSfxOverrideVolR = 0x3CCC >> (3 - st->bounce);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->sfxSlot);

            if (ReflectVectorOffNormal((int *)st, svH, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            svG[2] -= 0xF0;
            CopyVector(at, svG);
            FindGroundHeightBelow(svG, 0x10000);
            EncodeCachedVecToActorDirCode(actor);
          }

        enemy_settle:
          if (toSpyro < 0x59A) {
            int top = actor->posZ + 0x164;
            register int py asm("$2") = SPY.posZ;
            int dz = py - top;

            if (dz < 0) {
              dz = -dz;
            }

            if (dz < 0x200 && g_pSparxActor != 0 && g_nSpyroAirborneFrames == 0 &&
                st->bounce < 3) {
              {
                SparxState *sparx = g_pSparxActor->state;

                if (sparx->target == 0) {
                  if (GetSoundVoiceStatusByOwner(
                          g_pSparxActor,
                          g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0]) == 0) {
                    g_nSfxOverrideFlags = 1;
                    g_nSfxOverrideVolR = 0x2000;
                    g_nSfxOverrideVolL = 0x2000;
                    PlayActorMeshVoice(g_pSparxActor, 0, 0);
                  }
                  g_pSparxActor->subMode = 4;
                  sparx->target = actor;
                }
              }
              goto enemy_pose;
            }
          }
          goto enemy_pose;
        }

          goto enemy_pose;

        case 3:
          /* Fetched by Sparx: pulled toward Spyro, then collected. */
          if (st->timer >= 0x20) {
            CopyVector(&actor->posX, &SPY.posX);
          } else {
            SubtractVector(svI, &spy->posX, (int *)st);
            RShiftVector3(svI, 5);

            if (VectorLength(svI, VEC_LEN_XYZ) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(svI, svI, st->timer);
              AddVector(&actor->posX, (int *)st, svI);
              actor->posZ += (short)(g_anSineLut[st->timer * 4] / 12);
            }
          }

          actor->rotX += (unsigned char)(st->lean - 7);
          EBB_BREAK();
          actor->rotY += (unsigned char)(st->roll - 7);
          EBB_BREAK();
          actor->rotZ += (unsigned char)(st->spin - 7);
          goto enemy_pose;

        case 4:
          continue;

        default:
        enemy_pose:
          if (actor->subMode < 3) {
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

          if ((st->timer >= 0x21 && toSpyro < 0x200 &&
               (unsigned int)((SPY.posZ - actor->posZ) + 0x17F) < 0x37F) ||
              (actor->subMode == 3 && st->timer >= 0x41)) {
            if (g_pSparxActor != 0) {
              SparxState *sparx = g_pSparxActor->state;

              if (sparx->target == actor) {
                sparx->target = 0;
              }
            }

            if (actor->type == ACTOR_TYPE_EXTRA_LIFE) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
              RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
              g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)8);
              g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x10);
              if (g_nExtraLives < 0x63) {
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
      Actor *trigger = &g_pActorListBase[st->armer];
      Actor *rider = &g_pActorListBase[st->anchor];

      switch (actor->mode) {
      case 0:
        actor->collision = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) < 0xA01) {
          break;
        }
        actor->mode = 1;
        continue;

      case 1:
        if (trigger->mode >= ACTOR_MODE_DORMANT) {
          actor->mode = 2;
          st->timer = 0x10;
          st->baseZ = actor->posZ;
        }
        break;

      case 2:
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
                actor->mode = 3;
                actor->scale = 0x60;
                g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, at, (int *)0xC);
                RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
                FindGroundHeightBelow(at, 0x1000);
                EncodeCachedVecToActorDirCode(actor);
                actor->groundCode |= 0x80000000;
              }
            }
          }
        }
        break;

      case 3:
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ += 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->wanderTimer = 0x1E;
        }
        break;

      case 4: {
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
          actor->mode = 6;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2401 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             close = drop > 0 ? drop < 0xC01 : (SPY.posZ - height) < 0xC01,
             close == 0)) {
          actor->mode = 6;
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
          actor->mode = 5;
          st->wanderTo[0] -= (LookupCosine(aim) * reach) >> 12;
          st->wanderTo[1] -= (LookupSine(aim) * reach) >> 12;
          st->faceYaw = ArcTan2(spy->posX - st->wanderTo[0],
                                  spy->posY - st->wanderTo[1], 0);
          st->wanderTimer = 0;
        }
        break;
      }

      case 5: {
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
          actor->mode = 6;
          st->prompted = 1;
          BeginSaveMenuFromWorld(actor);
          break;
        }

        if (ApproxDist2D(&actor->posX, &spy->posX) >= 0x2801 ||
            (height = actor->posZ - actor->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop >= 0xC01 : (SPY.posZ - height) >= 0xC01)) {
          actor->mode = 6;
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
            actor->mode = 4;
            st->wanderTimer = (GetRandomU32() & 0x1F) + 0x1E;
          }
        }
        break;
      }

      case 6:
        if (actor->scale < 0x60) {
          actor->scale += 4;
          actor->rotZ += 8;
        } else {
          actor->cullRadius = 0;
          actor->drawn = 0;
          actor->mode = 2;
          st->timer = 0x10;
          g_pfnLevelEmitParticles(0x18, EMIT_FALLING_STREAK_BURST, &actor->posX, (int *)0xC);
          actor->groundCode &= 0x7FFFFFFF;
        }
        break;
      }

      break;
    }

    /* Sparx (SparxState): hovers beside Spyro, glows by health, catches
     * butterflies (health + 1) and fetches nearby pickups. Despawns at 0. */
    case ACTOR_TYPE_SPARX: {
      SparxState *st = actor->state;

      if (g_anSparxHealthBlock[0] >= 2) {
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
              ScaleVector3ByRatio(svA, dist, limit);
            } else {
              ScaleVector3ByRatio(svA, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, svA);
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(svA, VEC_LEN_XY), svA[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_anSparxHealthBlock[0] += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, SFX_MODE_PANNED, 0);
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

            SubtractVector(svJ, &st->target->posX, &actor->posX);
            dist = VectorLength(svJ, VEC_LEN_XYZ);

            if (dist < 0x200) {
              pk->timer = 0;
              pk->lean = GetRandomU32() & 0xE;
              pk->roll = GetRandomU32() & 0xE;
              pk->spin = GetRandomU32() & 0xE;
              CopyVector((int *)pk, &st->target->posX);
              st->target->subMode = 3;

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
                PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->sfxSlot);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(svJ, dist, lim);
              } else {
                ScaleVector3ByRatio(svJ, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, svJ);
              TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(svJ, VEC_LEN_XY), svJ[2], 0);
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

        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      }

      break;
    }

    /* Type 137: guard. Patrols, waits or lurks until Spyro comes within
     * range, then turns on him and charges; after a charge the camera is
     * pulled in on it, and a hit knocks it back. */
    case ACTOR_TYPE_CHARGING_GUARD: {
      ChargingGuardState *st = actor->state;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != 2) {
        EBB_BREAK();
        st->knock = StepAngle8Toward(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20);
        st->speed = 0xFA;
        st->turn = 0;
        if (actor->mode != 0) {
          *st->flag = 0;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }
      if (st->init == 0) {
        st->init = 1;
        if (st->lurk != 0 && ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
          if (actor->dropKind != 0xFF) {
            SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
            MarkActorCommittedKill(actor);
          }
          DespawnActorRecord(actor);
          continue;
        }
      }
      if (TickCountdownByFrameDelta(&st->timer3, 4) == 0) {
        int d = ApproxDist2D(&actor->posX, &spy->posX) + 0x1000;

        if (st->camDist < d) {
          st->camDist = d;
        }
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        g_anCameraActiveTargetParams.w[0] = -ArcTan2(spy->posX - actor->posX,
                                         SPY.posY - actor->posY, 0) *
                          16;
        g_anCameraActiveTargetParams.w[2] = st->camDist;
      }

      switch (actor->mode) {
      case 0: {
        int *p;

        TickCountdownByFrameDelta(&st->timer2, 4);
        switch (st->mode) {
        case 0:
          TrackEntityYawTowardSpyro(actor, 0xA, 0, 0);
          break;

        case 1:
          ANIM_GO(actor, 1);
          if (st->speed < 0x96) {
            st->speed = 0x96;
          }
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) < 0x100) {
            int r;
            PathData *pp;

            do {
              pp = st->path;
              r = RandomIntInRange(0, pp->count - 1);
              pp = st->path;
            } while (r == pp->cur);
            pp->cur = r;
            st->speed = RandomIntInRange(0xB4, 0xE6);
          }
          if (TurnActorYawTowardAngle(
                  actor,
                  ArcTan2(
                      st->path->nodes[st->path->cur].x - actor->posX,
                      st->path->nodes[st->path->cur].y - actor->posY, 0),
                  6, 0x1E, 1) != 0) {
            MoveActorAlongHeading(actor, st->speed, 0, 0, 5);
          }
          break;

        case 2:
          if (st->dist2 == 0) {
            st->dist2 = ApproxDist2D(&actor->posX, &st->path->nodes[0].x);
          }
          break;

        case 3: {
          int spd = 0xA0;
          int d = ApproxDist2D(&actor->posX, &g_pActorListBase[st->target].posX);

          ANIM_GO(actor, 1);
          if (d < 0xFA0) {
            spd = 0x8C;
          } else if (d > 0x1770) {
            spd = 0xB4;
          }
          TurnActorYawTowardAngle(actor,
                        ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                      g_pActorListBase[st->target].posY - actor->posY,
                                      0),
                        4, 0, 0);
          MoveActorAlongHeading(actor, spd, 0, 0, 5);
          break;
        }
        }
        if (st->mode == 1 || st->mode == 3) {
          p = &st->path->nodes[0].x;
        } else {
          p = &actor->posX;
        }
        if (st->mode == 2) {
          if (ApproxDist2D(&spy->posX, &st->path->nodes[1].x) < 0x5DC ||
              ApproxDist2D(&spy->posX, &st->path->nodes[2].x) < 0x5DC) {
            st->charging = 0;
            actor->mode = 1;
            continue;
          }
          break;
        }
        if (st->timer2 == 0 && *st->flag == 0 && g_nSpyroAirborneFrames == 0) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if ((dz > 0 ? dz < 0x320
                      : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x320) &&
              ApproxDist2D(p, &SPY.posX) < st->range << 10) {
            *st->flag = 1;
            st->timer = 0;
            st->charging = 0;
            if (st->mode == 3) {
              st->mode = 1;
              CopyVector(st->vec, p);
            }
            actor->mode = 1;
            continue;
          }
        }
        break;
      }

      case 1: {
        int s137;
        int dist = ApproxDist2D(&actor->posX, &spy->posX);

        if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
          break;
        }
        ANIM_GO(actor, 1);
        if (st->mode == 2) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist2,
                        ArcTan2(spy->posX - st->path->nodes[0].x,
                                      SPY.posY - st->path->nodes[0].y, 0),
                        8, 0x12C, 0xE, 8, 0xFF, 0xFF, 0, 0, 0);
          if (dist < 0xED8) {
            st->speed = 0xE6;
            st->cam = 0;
            ENTER_POSE(actor, 3);
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
              if (d > 0 ? d < 0xBB8 : a->posX - actor->posX < 0xBB8) {
                d = actor->posY - a->posY;
                if ((d > 0 ? d < 0xBB8 : a->posY - actor->posY < 0xBB8) &&
                    ApproxDist2D(&actor->posX, &a->posX) < 0x7D0 &&
                    AbsAngleDelta8(actor->rotZ,
                                  ArcTan2(a->posX - actor->posX,
                                                a->posY - actor->posY, 0)) <
                        0x19) {
                  a->flags |= 0x80000;
                }
              }
            }
          }
        }
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->knock = SPY.bodyRotZ;
          st->timer2 = 0x1E;
        }
        st->turn = (ArcTan2(actor->posX - spy->posX,
                                  actor->posY - spy->posY, 0) -
                    st->knock + 0x100) %
                   0x100;
        if (TurnActorYawTowardAngle(actor,
                          WrapAddAngle8(ArcTan2(spy->posX - actor->posX,
                                                      spy->posY - actor->posY,
                                                      0),
                                        st->turn),
                          5, 0x1E, 1) == 0) {
          if (st->charging == 0) {
            break;
          }
        } else {
          st->charging = 1;
        }
        s137 = 0x15E;
        if (st->speedOverride != 0) {
          s137 = st->speedOverride;
        }
        if (MoveActorAlongHeading(actor, s137, 0, 0x12C, 0x15) != 0) {
        reset137:
          *st->flag = 0;
          st->timer2 = 0x78;
          actor->mode = 0x64;
          continue;
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, 0)) >= 0x3D ||
            ApproxDist2D(&actor->posX, st->vec) >= 0x4001) {
          goto reset137;
        }
        if (dist < 0xED8 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x14) {
          st->speed = 0xFA;
          st->cam = 0;
          ENTER_POSE(actor, 3);
        }
        break;
      }

      case 2:
        if (actor->frame < 5) {
          actor->rotZ += st->turn;
        }
        if (st->speed > 0xF) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x12C, 5);
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x28);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 3:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (MoveActorAlongHeading(actor, st->speed, 0, 0x12C, 0x15) == 2) {
          *st->flag = 0;
          st->timer2 = 0x78;
          actor->mode = 0x64;
          continue;
        }
        if (actor->frame >= 4 && st->speed > 0x28) {
          st->speed -= 0x1E;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x5DC) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0x384
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x384) {
            if (st->cam == 0) {
              st->cam = 1;
              st->timer2 = 0xDC;
              SPY.triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
              g_anSpyroRequestVelocity.x = LookupCosine(actor->rotZ * 16) * 0x87 >> 12;
              g_nSpyroRequestVelocityY = LookupSine(actor->rotZ * 16) * 0x87 >> 12;
              g_nSpyroRequestVelocityZ = 0x32;
            } else if (st->cam == 1) {
              st->cam = 2;
              st->timer3 = 0x1E;
              SPY.requestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
              SPY.pCamAnchor = &actor->posX;
              SPY.pCamTarget = &g_anCameraActiveTargetParams;
              g_anCameraActiveTargetParams.w[0] = -ArcTan2(SPY.posX - actor->posX,
                                               SPY.posY - actor->posY, 0) *
                                16;
              g_anCameraActiveTargetParams.w[2] =
                  ApproxDist2D(&actor->posX, &SPY.posX) + 0xC00;
              g_anCameraActiveTargetParams.w[1] = 0x64;
              g_anCameraActiveTargetParams.w[3] = 0;
              g_anCameraActiveTargetParams.w[4] = -0xC8;
              g_anCameraActiveTargetParams.w[5] = 0;
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 4);
        }
        break;

      case 4:
        if (g_nActorAnimDone != 0) {
          actor->rotZ = WrapAddAngle8(actor->rotZ, 0x80);
          if (st->cam == 0) {
            ENTER_POSE(actor, 1);
          }
          *st->flag = 0;
          ANIM_GO(actor, 1);
          actor->mode = 0x64;
          continue;
        }
        break;

      case 0x64:
        ANIM_GO(actor, 1);
        if (st->mode == 2) {
          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist2,
                        ArcTan2(st->vec[0] - st->path->nodes[0].x,
                                      st->vec[1] - st->path->nodes[0].y, 0),
                        1, 0xC8, 0xE, 8, 0xFF, 0xFF, 0, 0, 0);
          if (ApproxDist2D(&actor->posX, st->vec) < 0x200) {
            ENTER_POSE(actor, 0);
          }
          break;
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2_8bit(st->vec[0] - actor->posX,
                                        st->vec[1] - actor->posY),
                          4, 0x14, 1) != 0) {
          MoveActorAlongHeading(actor, 0xA0, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, st->vec) < 0xB4) {
          ENTER_POSE(actor, 0);
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Key (KeyState): rocks in place and sparkles; touching it picks it up. */
    case ACTOR_TYPE_KEY: {
      KeyState *st = actor->state;

      if (actor->subMode == 0) {
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

            g_nLevelKeyState = 1;
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

    /* Breakable chest (ChestState): one hit showers gems. */
    case ACTOR_TYPE_WOOD_CHEST: {
      ChestState *st = actor->state;

      if (st->shake != 0) {
        SnapActorToGround(actor);
      }

      if (actor->flags & ACTOR_HITS_SPYRO) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD, actor);
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 1, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_WOOD_CHEST_SHARD + 2, actor);
        }

        g_pfnLevelEmitParticles(5, EMIT_SPARK_RISE, &actor->posX, 0);
        g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (int *)0x20);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
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
          SubtractVector(A256, at, &spy->posX);
          A256[2] = (A256[2] * 3) >> 2;

          if (VectorLength(A256, VEC_LEN_XYZ) < 0x440) {
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

            actor->mode = 2;
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
            if (CollideSphereWithWorldAndActors(&actor->posX, 0x100, 0x100) != 0) {
              int *nrm = &g_anCollisionProbeVec;
              int along;

              actor->posX = g_anCollisionResolvedPos;
              actor->posY = AS_I32(g_nCollisionResolvedY);
              actor->posZ = AS_I32(g_nCollisionResolvedZ);
              SetVector3Magnitude(nrm, 0x1000);
              along = ((st->vel[0] * nrm[0]) + (st->vel[1] * g_nCollisionProbeVecY) +
                       (st->velZ * g_nCollisionProbeVecZ)) >>
                      11;

              if (along < 0) {
                ScaleVector3ByRatio(nrm, 0x1000, -along);
                st->vel[0] += nrm[0];
                st->vel[1] += AS_I32(g_nCollisionProbeVecY);
                st->velZ += AS_I32(g_nCollisionProbeVecZ);
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

        A256[0] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[1] = (GetRandomU32() & 0xFE) - 0x7F;
        A256[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(A256, A256, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, A256, (int *)1);
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
            A272[0] = (GetRandomU32() & 0xFE) - 0x7F;
            A272[1] = (GetRandomU32() & 0xFE) - 0x7F;
            A272[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(A272, A272, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, A272, (int *)1);
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

    /* Type 329: seed pod. Wobbles when struck and holds a floating prize
     * above itself; Spyro takes the prize by touching it (or by being in a
     * grab state), otherwise the pod settles back after a while. */
    case ACTOR_TYPE_SEED_POD: {
      SeedPodState *st = actor->state;

      actor->soundRange = 0x20;
      switch (actor->mode) {
      case 0:
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
          st->prize->subMode = 4;
          st->prize->cullRadius = 0;
          st->prize->drawn = 0;
          st->prize->rotX = 0;
          st->prize->rotY = 0;
          g_pfnLevelEmitParticles(8, EMIT_GEM_COLOR_BURST, &actor->posX, (void *)(st->prize->type - 0x53));
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
          ZeroVector(A288);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, A288);
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
          st->prize->subMode = 2;
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
            CopyVector(A304, &st->prize->posX);
            A304[0] += g_anCosineLut[j * 43] >> 6;
            A304[1] += g_anSineLut[j * 43] >> 6;
            A304[2] += 0x28;
            A320[0] = g_anCosineLut[j * 43] >> 7;
            A320[1] = g_anSineLut[j * 43] >> 7;
            A320[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, A304, A320);
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_SPLASH:
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case ACTOR_TYPE_SPIN_CHEST: {
      SpinChestState *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
      int cur = g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0];
      int i;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, SFX_MODE_3D,
                      &actor->sfxSlot);
        if (actor->sfxSlot != 0x7F) {
          g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
      }
      if (actor->subMode == 0) {
        st->lid = g_pfnLevelSpawnActor(ACTOR_TYPE_HOMING_SPARK, actor);
        st->lid->cullRadius = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = 1;
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
      if (pitch < cur && actor->sfxSlot != 0x7F) {
        g_anSpuVoiceRowPitchStep[actor->sfxSlot & 0x7F][0] = -4;
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
        st->lid->subMode = 1;
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

    /* Type 392: homing spark. Launched at the nearest enemy in front of
     * Spyro (or away from him when there is none), and bursts on what it
     * hits. */
    case ACTOR_TYPE_HOMING_SPARK: {
      HomingSparkState *st = actor->state;
      int i;

      if (actor->subMode == 1) {
        Actor *best = 0;
        int bestAng = 0x180;
        Actor *a;

        for (a = g_pActorListBase; a < g_pActorPoolDynBase; a++) {
          int d;

          if (a == actor || a->mode >= ACTOR_MODE_DORMANT || a->collision == 0 ||
              a->collision >= 0) {
            continue;
          }
          SubtractVector(A288, &a->posX, &actor->posX);
          d = ABS2(A288[0]) + ABS2(A288[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(A288, VEC_LEN_XY) - 0x1000) <= 0x2000 &&
              (unsigned int)(A288[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(A288[0], A288[1], 1));
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
        actor->subMode = 2;
        actor->cullRadius = 0x20;
        actor->updateDist = 0xFF;
        actor->rotX = 0xA;
        actor->rotY = 0;
        continue;
      }
      if (actor->subMode != 2) {
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

    /* Type 403: lurker. Roams its path, then pops up and drags Spyro with a
     * tether actor (type 31). */
    case ACTOR_TYPE_LURKER: {
      LurkerState *st = actor->state;
      int dist;

      SubtractVector(A304, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &spy->posX);
      if (dist < 0x32C8) {
        dist = VectorLength(A304, VEC_LEN_XY);
      }

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 2 &&
          actor->mode != 0x1E) {
        EBB_BREAK();
        actor->flags = 0;
        st->stunTimer = 0x50;
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->knockZSpeed = 0x64;
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->knockSpeed = 0xF0;
        } else {
          st->knockSpeed = 0x140;
        }
        if (st->tether != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          if (g_pSparxActor != 0 && st->holdsSpyro != 0) {
            g_pSparxActor->subMode = 0;
          }
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 2);
      }

      switch (actor->mode) {
      case 0:
        if (dist < 0x2000 && TickCountdownByFrameDelta(&st->coolDown, 4) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x5DC
                               : -SPY_DZ(actor) < 0x5DC) &&
            AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], 0)) <
                0x28 &&
            dist < 0xC80 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x14) {
          int k;

          st->tether = g_pfnLevelSpawnActor(ACTOR_TYPE_LURKER_TETHER, actor);
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, A336);
          GetActorAttachPoint(actor, 1, A352);
          AddVector(&st->tether->posX, A336, A352);
          RShiftVector3(&st->tether->posX, 1);
          SubtractVector(A336, &st->tether->posX, &spy->posX);
          k = 0x1C000 / VectorLength(A336, VEC_LEN_XYZ);
          if (k < 5) {
            k = 5;
          }
          if (k >= 0x80) {
            k = 0x7F;
          }
          st->tether->scale = k;
          st->tether->rotZ = actor->rotZ;
          actor->mode = 0x14;
          continue;
        }
        if (st->mode == 0) {
          if (dist > 0x2000) {
            if (TickCountdownByFrameDelta(&st->roamTimer, 4) != 0) {
              FlyPath *p = st->path;
              int c = p->cur;

              do {
                st->path->cur = RandomIntInRange(0, p->count - 1);
                p = st->path;
              } while (c == p->cur);
              actor->mode = 0xA;
              continue;
            }
          } else if (dist >= 0xFA1) {
            FindNearestPathPointToSpyro(st->path, A368);
            if (st->path->cur != A368[0] &&
                dist - ApproxDist2D((int *)&g_anSpyroWorldPos,
                                     PATH_PT((unsigned char *)st->path,
                                             A368[0])) >=
                    0x5DD) {
              st->path->cur = A368[0];
              actor->mode = 0xA;
              continue;
            }
          }
        }
        if (AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) >= 0x13) {
          actor->mode = 0xF;
          continue;
        }
        break;

      case 2:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, 0xC, sixteen);
        if ((g_nGroundSurfaceType & 0x3F) == 0) {
          int h = ProbeActorGroundHeight(actor);
          int z = actor->posZ - actor->floorOffset;

          if (z - h > 0 ? z - h < 0x64 : h - z < 0x64) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_SPLASH, actor);

            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SPLASH], actor, SFX_MODE_3D, &actor->sfxSlot);
            o->scale = 0x34;
            st->coolDown = 0xA;
            actor->mode = 0x1E;
            continue;
          }
        }
        if (TickCountdownByFrameDelta(&st->stunTimer, 4) != 0 || g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        actor->rotZ += 5;
        if (TickCountdownByFrameDelta(&st->roamTimer, 4) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 10:
        ANIM_GO(actor, 1);
        if (ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos) < 0xDAC) {
          actor->mode = 0xF;
          continue;
        }
        if (TurnActorYawTowardAngle(
                actor,
                ArcTan2(FLY_X(st->path, st->path->cur) - actor->posX,
                              FLY_Y(st->path, st->path->cur) - actor->posY,
                              0),
                3, 2, 1) != 0 &&
            g_nActorAnimDone != 0) {
          st->hopSpeed = ApproxDist2D(&actor->posX,
                                  PATH_PT((unsigned char *)st->path,
                                          st->path->cur)) /
                    0x14;
          actor->mode++;
        }
        break;

      case 11:
        MoveActorAlongHeading(actor, st->hopSpeed, 0, 0, 5);
        if (g_nActorAnimDone != 0) {
          st->roamTimer = RandomIntInRange(0x28, 0x5A);
          ENTER_POSE(actor, 0);
        }
        break;

      case 15:
        ANIM_GO(actor, 1);
        if (TrackEntityYawTowardSpyro(actor, 8, 2, 1) != 0 && g_nActorAnimDone != 0) {
          ENTER_POSE(actor, 0);
        }
        break;

      case 20: {
        int d;
        int k;

        ANIM_GO(actor, 3);
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, A352);
        GetActorAttachPoint(actor, 1, A376);
        AddVector(&st->tether->posX, A352, A376);
        RShiftVector3(&st->tether->posX, 1);
        SubtractVector(A352, &st->tether->posX, (int *)&g_anSpyroWorldPos);
        d = VectorLength(A352, VEC_LEN_XYZ) + 0x1F4;
        if (d < 0x578) {
          d = 0x578;
        }
        if (d >= 0xC81) {
          d = 0xC80;
        }
        k = 0x1C000 / d;
        if (k < 5) {
          k = 5;
        }
        if (k >= 0x80) {
          k = 0x7F;
        }
        st->tether->scale = k;
        if (actor->nextFrame < 2) {
          st->tether->cullRadius = 0;
        } else {
          st->tether->cullRadius = 0x20;
        }
        if (actor->nextFrame >= 4 && dist < 0xC80) {
          SPY.triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
        }
        actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                     spy->posY - actor->posY, 0);
        st->tether->rotZ = ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0);
        st->tether->rotY =
            ArcTan2(ApproxDist2D(&st->tether->posX, &spy->posX),
                          SPY.posZ - st->tether->posZ, 0);
        st->tether->rotY = StepAngle8Toward(st->tether->rotY, 0, 0x20);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(st->tether);
          st->tether = 0;
          st->coolDown = 0xB4;
          ENTER_POSE(actor, 0);
        }
        break;
      }

      case 30:
        if (TickCountdownByFrameDelta(&st->coolDown, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        actor->posZ -= 0x64;
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
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

      if ((actor->flags & ACTOR_HITS_SPYRO) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, DROP_IN_PLACE, 0, 0);
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
        A376[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        A376[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        A376[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, A376);
      }

      g_pfnLevelEmitParticles(0x10, EMIT_STREAK_BURST, &actor->posX, (void *)0x18);

      if (actor->mode == 1) {
        DespawnActorRecord(st->passenger);
      }

      PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[2], actor, SFX_MODE_3D,
                    (void *)(long)actor->sfxSlot);
      DespawnActorRecord(actor);
      continue;
    }

    /* Name letter (NameLetterState): one letter of a floating name
     * (SpawnNameLetterActors), riding its sign at a fixed radius, or just
     * spinning in place behind a non-spinner. */
    case ACTOR_TYPE_LETTER_OTHER:
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
          A376[0] = (g_anCosineLut[bearing] * 3) >> 4;
          A376[1] = (g_anSineLut[bearing] * 3) >> 4;
          A376[2] = 0;
          A336[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          A336[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          A336[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(A392, A336, st->radius - 1);
          LShiftVector3(A336, 1);
          arm = st->radius - 1;
          A392[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          A408[0] = A376[0] * g_anCosineLut[arm * 2];
          A408[1] = A376[1] * g_anCosineLut[arm * 2];
          A408[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(A336, A336, st->phase);
          SubtractVector(A392, A392, A336);
          swing = &g_anCosineLut[bearing];
          actor->posX = A376[0] * *swing;
          actor->posY = A376[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, A408);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, A376);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, A392);
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

      if ((actor->flags & ACTOR_HIT_ANY) && actor->mode != 3) {
        EBB_BREAK();
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
        if (actor->nextAnim != 3) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->nextAnim = 3;
          actor->nextFrame = 0;
          OnActorAnimQueued(actor);
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
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x10);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4:
        if (g_anLevelTriggerStateSlots[st->chan] == 0) {
          st->speed = 0;
          st->lift = 0;
          SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
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
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 476: grabber. Creeps up on Spyro, seizes him, carries him off and
     * throws him; it drops him when hit. */
    case ACTOR_TYPE_GRABBER: {
      GrabberState *st = actor->state;
      int d0 = ApproxDist2D(&actor->posX, &spy->posX);

      EBB_BREAK();
      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != 4 &&
          actor->mode != 0x63) {
        Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_GRABBER, actor);

        CopyVector(&o->posX, &actor->posX);
        o->mode = 0x63;
        if (o->anim != 0xA) {
          ANIM_RESET(o, 0xA);
        }
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorCommittedKill(actor);
        st->knockSpeed = 0x15E;
        if (actor->mode >= 0xA) {
          g_nSpyroDrawSuppressed = 0;
        }
        st->knockHeading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                              actor->posY - SPY.posY, 0),
                                g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->knockZSpeed = 0xF0;
        if (actor->mode != 0) {
          *st->held = 0;
        }
        PlayActorMeshVoice(actor, 1, 0);
        ANIM_SET(actor, 9);
        actor->mode = 4;
        continue;
      }
      actor->flags = 0;
      if (actor->nextAnim == 1 &&
          GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[2]) == 0) {
        PlayActorMeshVoice(actor, 2, (int)&st->voice);
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (d0 > 0x2C00) {
          st->grabChance = 0;
          ANIM_GO(actor, 0);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          int k;
          int r;

          if (st->grabChance == 0) {
            if (actor->nextAnim == 0 && actor->nextFrame >= 0xF) {
              actor->frame = 0;
              actor->nextFrame = 1;
              actor->frameProgress = 0;
            }
          } else if (actor->nextAnim == 0 && actor->nextFrame >= 0x1C) {
            g_nActorAnimDone = 0;
            ANIM_ADVANCE(actor, 1);
          }
          if (st->grabChance == 0 && d0 < 0x2400) {
            st->grabChance = RandomIntInRange(0, 0x3F) + 0x50;
          }
          k = st->grabChance - (d0 >> 6);
          r = GetRandomU32() & 0xFF;
          if (TickCountdownByFrameDelta(&st->grabCoolDown, 4) != 0 && *st->held == 0 && r < k &&
              AbsAngleDelta8(((unsigned short)g_nCameraEulerYaw << 16 >> 20),
                            ArcTan2(actor->posX - g_anCameraPos[0],
                                          actor->posY - g_anCameraPos[1], 0)) <
                  0x28 &&
              SPY_DZ(actor) >= -0x1F3 &&
              (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x578
                                 : -SPY_DZ(actor) < 0x578)) {
            *st->held = 1;
            st->timer = sixteen;
            StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
            actor->mode = 1;
            continue;
          }
          if (*st->held != 0) {
            st->grabCoolDown = 0x3C;
          }
        }
        break;

      case 1: {
        int d1 = ApproxDist2D(&actor->posX, (int *)&g_anSpyroWorldPos);

        if (d1 < 0xFA0) {
          ANIM_GO(actor, 2);
        } else {
          ANIM_GO(actor, 3);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          Actor **lp;
          Actor *o;

          if (MoveActorAlongAngle(actor,
                            ArcTan2(spy->posX - actor->posX,
                                          spy->posY - actor->posY, 0),
                            0x15E, 0, 0, 0x15) != 0 ||
              spy->triggerEventId != 0) {
            actor->mode = 2;
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
                                            o->posY - actor->posY, 0)) < 0x19) {
              o->flags |= 0x80000;
            }
          }
          if (d1 < 0x44C) {
            st->hasSpyro = 0;
            actor->mode = 0xA;
            continue;
          }
        }
        break;
      }

      case 2:
        ANIM_GO(actor, 3);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->home[0] - actor->posX,
                                        st->home[1] - actor->posY, 0),
                          6, 0x14, 1) != 0) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        0xC8, 0, 0, 5);
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x100) {
          *st->held = 0;
          st->timer = 0x78;
          ENTER_POSE(actor, 0);
        }
        break;

      case 4:
        actor->rotX += 4;
        actor->rotY += 4;
        if (DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockZSpeed, 0xC, 0xE) == 3) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x30);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 6:
        if (g_nActorAnimDone != 0) {
          actor->mode = 1;
          continue;
        }
        break;

      case 10: {
        int dd = ApproxDist2D(&actor->posX, &spy->posX);
        int lim = 0x50;
        int v;

        if (dd >= 0x12D) {
          lim = 0xFA;
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
            TrackEntityYawTowardSpyro(actor, 6, 0, 0);
            if (MoveActorAlongAngle(actor,
                              ArcTan2(spy->posX - actor->posX,
                                            SPY.posY - actor->posY, 0),
                              lim, 0, 0, 0x15) != 0 &&
                g_nSpyroDrawSuppressed == 0) {
              actor->mode = 2;
              continue;
            }
          }
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (actor->nextFrame >= 0xD) {
          SPY.posX = actor->posX;
          g_nSpyroDrawSuppressed = 1;
          SPY.posY = actor->posY;
          v = actor->rotZ + 0x80;
          g_bSpyroPersistentEulerZ = v;
          g_nSpyroBodyYaw = (v & 0xFF) << 4;
          if (g_nSparxHealth == 0) {
            spy->triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
            actor->mode = 0xD;
            continue;
          }
          if (g_bLevelUseShortReleaseAnim != 0) {
            actor->mode = 0xE;
            continue;
          }
          actor->mode++;
        }
        break;
      }

      case 11:
        ANIM_GO(actor, 4);
        spy->posX = actor->posX;
        spy->posY = actor->posY;
        g_nSpyroDrawSuppressed = 1;
        spy->requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (g_nActorAnimDone != 0) {
          actor->mode++;
        }
        break;

      case 12:
        ANIM_GO(actor, 5);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= 0x29) {
            g_nSpyroDrawSuppressed = 0;
            SPY.triggerEvents = (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * 0x4B) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * 0x4B) >> 10);
            SubtractVector(A408, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A408, VectorLength(A408, VEC_LEN_XY), 0x64);
            g_nSpyroRequestVelocityZ = 0x37;
            g_anSpyroRequestVelocity.x = A408[0];
            g_nSpyroRequestVelocityY = A408[1];
          } else {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          }
        }
        if (g_nActorAnimDone != 0) {
          g_bLevelUseShortReleaseAnim = 1;
          actor->mode = 2;
          continue;
        }
        break;

      case 13:
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ANIM_GO(actor, 8);
        g_dwSpyroTriggerEventFlags |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT);
        break;

      case 14:
        ANIM_GO(actor, 7);
        if (g_nSpyroDrawSuppressed != 0) {
          if (actor->nextFrame >= 0x10) {
            g_nSpyroDrawSuppressed = 0;
            SPY.triggerEvents = (SPYRO_EV_CHARGE_HIT | SPYRO_EV_KNOCKBACK_2);
            SPY.posX =
                actor->posX + ((g_anCosineLut[actor->rotZ] * 0x4B) >> 10);
            SPY.posY =
                actor->posY + ((g_anSineLut[actor->rotZ] * 0x4B) >> 10);
            SubtractVector(A376, &SPY.posX, &actor->posX);
            ScaleVector3ByRatio(A376, VectorLength(A376, VEC_LEN_XY), 0x64);
            g_nSpyroRequestVelocityZ = 0x37;
            g_anSpyroRequestVelocity.x = A376[0];
            g_nSpyroRequestVelocityY = A376[1];
          } else {
            g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          }
        }
        if (g_nActorAnimDone != 0) {
          actor->mode = 2;
          continue;
        }
        break;

      case 99:
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

    /* Type 487. */
    case 487:
      if (actor->flags != 0 && actor->mode < 3) {
        actor->mode = 3;
        ANIM_RESET(actor, 3);
        continue;
      }
      switch (actor->mode) {
      case 0:
        break;

      case 1:
        ANIM_SET(actor, 1);
        if (actor->nextFrame >= 0xE) {
          actor->mode = 2;
          ANIM_RESET(actor, 2);
          continue;
        }
        break;

      case 2:
        break;

      case 3:
        if (actor->nextFrame >= 0x13) {
          actor->mode = 4;
          ANIM_RESET(actor, 4);
          continue;
        }
        break;

      case 4:
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
}
  }
}
