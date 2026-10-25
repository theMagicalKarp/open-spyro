/* Level28_UpdateActors -- per-frame actor update for level 28
 * (Jacques). 0x8007AF50, 34,648 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 2/4/9/15/25/32/33; the level-28 classes
 * are Jacques himself (150), the rocks he throws (47), the charging rams
 * (45/46) and the singing path walker (126). Types 234 and 507/508 are
 * one-line "remove when the animation ends" arms; 507/508's body is
 * cross-jumped into the tail of case 47's states 1 and 2.
 *
 * Load-bearing source forms:
 *   - Carriers as in level 9 (`sixteen` in s6, `one` in fp) with level 12's
 *     absolute spy (`spy = &g_anSpyroWorldPos` in s7), assigned in that order at
 *     the loop top. The scratch arrays are declared in the original's slot
 *     order and named by sp offset (g056..g512).
 *   - Case 110 is level 9's (`if (d > 0xA00) mode = 1; break;`) so it
 *     cross-jumps into case 331; case 160's hit block opens with an empty
 *     do/while(0) (A200).
 *   - Case 47 writes its states 1 and 2 as two identical cases, and the
 *     drop-height test as an explicit negate-if-negative.
 *   - Case 45 reads the heading through a non-volatile view of
 *     g_bSpyroPersistentEulerZ, picks the knock-back call with an if/else, and nests its
 *     look-away timer so the reset arm comes last.
 *   - Case 126's carried state (mode 3) comes after the free-walking code.
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


/* ==== Level 28 classes ==== */


/* ==== The function ==== */
#undef sixteen
void Level28_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int sixteen;
  int one;
  short oneq;
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
  int g216[3][3];
  int g256[3];
  int g272[3];
  int g288[3];
  int g304[3];
  int g320[3];
  int g336[3];
  int g352[3];
  int g368[3];
  int g384[3];
  int g400[3];
  int g416[3];
  int g432[3];
  int g448[3];
  int g464[3];
  int g480[3];
  int g496[3];
  int g512[3];

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
    sixteen = 0x10;
    oneq = 1;
    one = 1;
    spy = (SpyroObj *)&g_anSpyroWorldPos;

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
        actor->mode = oneq;
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
                g_abLevelExited[g_nLevelIndex] = 1;
                g_nCdStreamState = 0;
                g_nEnteredPortalIdx = -1;
                g_nPortalTransitionActive = one;
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]) +
                st->count;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, 8, &actor->sfxSlot);
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
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, 8, &actor->sfxSlot);
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

          CopyVector(g056, &spy->posX);
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
          CopyVector(g104, &spy->posX);
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
        g_anLevelEggsRecovered[g_nLevelIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        continue;
      }
      }

      break;
    }

    /* Types 45/46: a charging ram. Paces, charges when Spyro comes close
     * and is not looking, and walks home if it overshoots; 45 is the big
     * one (shorter reach, only a charge or a flame-charge hurts it). */
    case 45:
    case 46: {
      ChargingRamState *st = actor->state;
      int range = 0x708;
      int dist;

      SubtractVector(g104, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &spy->posX);
      if (dist < 0x32C8) {
        dist = VectorLength(g104, 0);
      }
      if (actor->type == 0x2D) {
        range = 0x5DC;
      }
      if (st->init == 0) {
        st->init = one;
        actor->frame = actor->nextFrame = (int)GetRandomU32() % 10;
        if (st->mode != 0) {
          actor->mode = 2;
          actor->nextAnim = 2;
          actor->anim = 2;
        }
      }
      if (actor->type == 0x2D) {
        TickActorFlameGlow(actor);
      }
      if (actor->mode != 4 &&
          ((actor->type == 0x2D && (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER))) ||
           (actor->type == 0x2E && (actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER))))) {
        st->knock = 0x190;
        st->lift = 0xAA;
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, 0),
                                    spy->bodyRotZ, 0x20, 0x40);
        actor->flags = 0;
        if (st->heavy != 0) {
          SpawnActorDeathDrop(actor, 4, 0, 0);
        } else {
          SpawnActorDeathDrop(actor, 3, 0, 0);
        }
        MarkActorCommittedKill(actor);
        actor->mode = 4;
        ANIM_RESET(actor, 4);
        continue;
      }
      actor->flags = 0;
      RefreshActorDerivedState(actor, 5);
      if (dist > 0xC00) {
        st->charged = 0;
      }

      switch (actor->mode) {
      case 0:
      case 2:
        TickCountdownByFrameDelta(&st->timer, 4);
        if (st->timer == 0 && dist < 0x2000 && actor->type == 0x2E &&
            st->mode != 0 &&
            AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, 0)) < 0x26 &&
            AbsAngleDelta8(g_nCameraEulerYaw >> 4,
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], 0)) <
                0x23) {
          if (actor->nextAnim < 2) {
            actor->mode = 6;
            ANIM_RESET(actor, 6);
            continue;
          }
          ENTER_POSE(actor, 6);
        }
        TrackEntityYawTowardSpyro(actor, 7, 5, 0);
        if (st->timer == 0 && dist < range) {
          st->timer = 0xA0;
          st->saved = actor->mode;
          if (st->mode == 0) {
            ENTER_POSE(actor, 1);
          }
          ENTER_POSE(actor, 3);
        }
        continue;

      case 1:
      case 3:
        if (actor->nextFrame < 10) {
          int h = actor->rotZ;

          if (actor->type == 0x2D) {
            h = (h + 0x1E) & 0xFF;
          }
          g_anSpyroRequestVelocity.x = (g_anCosineLut[h] * 5) >> 8;
          g_nSpyroRequestVelocityZ = 0x1E;
          g_nSpyroRequestVelocityY = (g_anSineLut[h] * 5) >> 8;
        }
        TrackEntityYawTowardSpyro(actor, 7, 5, 0);
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, st->saved);
        }
        continue;

      case 4:
        DriveActorTrackedMove(actor, &st->knock, st->heading, &st->lift, 0xC, sixteen);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 5);
          BurstActorWithSound(actor, 0x20);
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 5:
        if (st->charged != 0) {
          actor->mode = 100;
          continue;
        }
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->timer2 = 0x1E;
          st->heading = *(unsigned char *)&g_bSpyroPersistentEulerZ;
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0),
                          4, 0x1E, one) != 0) {
          if (MoveActorAlongHeading(actor, 0x64, 0x300, 0x200, 0x25) != 0 ||
              ApproxDist2D(&actor->posX, st->home) > 0x2800) {
            actor->mode = 100;
            continue;
          }
        }
        if (AbsAngleDelta8(spy->bodyRotZ,
                          ArcTan2(actor->posX - spy->posX,
                                        actor->posY - spy->posY, 0)) >= 0x31) {
          if (st->timer3 != 0) {
            if (TickCountdownByFrameDelta(&st->timer3, 4) != 0) {
              st->timer3 = 0;
              actor->mode = 100;
              continue;
            }
          } else {
            st->timer3 = 0x28;
          }
        } else {
          st->timer3 = 0;
        }
        if (st->charged == 0 && dist < range - 0x258 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        spy->posY - actor->posY, 0)) < 0x40) {
          st->timer = 0xA0;
          st->charged = one;
          st->saved = 6;
          RefreshActorDerivedState(actor, 1);
          ENTER_POSE(actor, 3);
        }
        continue;

      case 6:
        if (actor->frame >= 3) {
          ANIM_QUEUE(actor, 5);
        }
        continue;

      case 100: {
        int h = ArcTan2_8bit(st->home[0] - actor->posX,
                              st->home[1] - actor->posY);

        RefreshActorDerivedState(actor, 1);
        if (TurnActorYawTowardAngle(actor, h, 4, 0x14, one) != 0) {
          MoveActorAlongHeading(actor, 0x5A, 0x300, 0x200, 0x27);
        }
        ANIM_GO(actor, 5);
        if (ApproxDist2D(&actor->posX, st->home) < 0x80) {
          ENTER_POSE(actor, 2);
        }
        continue;
      }
      }
      continue;
    }

    /* Type 47: a rock. Rides between its thrower's hands, is thrown at
     * Spyro's feet and smashes on landing. */
    case 47: {
      CarriedRockState *st = actor->state;

      if (st->mode != 0x63) {
        RefreshActorDerivedState(actor, 5);
        FindGroundHeightBelow(&actor->posX, 0x10000);
        EncodeCachedVecToActorDirCode(actor);
        CopyVector(g056, &actor->posX);
        g056[2] += 0xC8;
        if ((actor->mode == 0 || actor->mode == 9) &&
            RaiseSpyroTriggerInRadius(g056, 0xDC, 1) != 0) {
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
      }

      switch (actor->mode) {
      case 0: {
        int d;

        if (st->partner->mode == 0xA) {
          ((int *)st->partner->state)[13] = 0;
          ((int *)st->partner->state)[21] = 0;
          ((int *)st->partner->state)[17] = 0;
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        if (st->partner->nextFrame == 4) {
          st->mode = 0;
        }
        if (st->mode == 1) {
          GetActorAttachPoint(st->partner, 1, g136);
          GetActorAttachPoint(st->partner, 2, g152);
          actor->posX = g136[0] + ((g152[0] - g136[0]) >> 1);
          actor->posY = g136[1] + ((g152[1] - g136[1]) >> 1);
          actor->posZ = g136[2] + ((g152[2] - g136[2]) >> 1);
          continue;
        }
        if (st->mode == 0x63) {
          continue;
        }
        CopyVector(g168, &spy->posX);
        if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        g168[2] = FindGroundHeightBelow(&spy->posX, 0x2800) + 0x12C;
        {
          int dz = g168[2] - st->partner->posZ;

          if (dz < 0) {
            dz = -dz;
          }
          if (dz > 0x1000) {
            g168[2] = actor->posZ - 0x12C;
          }
        }
        actor->rotZ =
            ArcTan2(spy->posX - actor->posX, SPY.posY - actor->posY, 0);
        st->timer = 8;
        st->timer2 = 0x78;
        ((int *)st->partner->state)[17] = 0;
        st->speed = 0x12C;
        d = ApproxDist2D(&actor->posX, g168);
        if (d < st->speed) {
          st->vz = -d;
        } else {
          d = d / st->speed;
          st->vz = (g168[2] - actor->posZ) / d;
          st->spinY = 2;
          st->u06 = 0;
          st->spinX = 1;
        }
        actor->mode = 9;
        continue;
      }

      case 1:
        RefreshActorDerivedState(actor, 5);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 2:
        RefreshActorDerivedState(actor, 5);
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 9:
        actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
        actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
        actor->posZ += st->vz;
        actor->rotX += st->spinX;
        actor->rotY += st->spinY;
        if (TickCountdownByFrameDelta(&st->timer, 1) != 0) {
          CopyVector(g056, &actor->posX);
          g056[2] += 0xC8;
          if (CollideSphereWithWorldAndActors(g056, 0xDC, 0xDC) != 0) {
            st->u06 = 0;
            st->speed = 0;
            st->spinX = 0;
            st->spinY = 0;
            PlayActorMeshVoice(actor, 0, 0);
            actor->mode = 2;
            ANIM_RESET(actor, 2);
            continue;
          }
        }
        if (TickCountdownByFrameDelta(&st->timer2, 2) != 0) {
          st->u06 = 0;
          st->speed = 0;
          st->spinX = 0;
          st->spinY = 0;
          PlayActorMeshVoice(actor, 0, 0);
          actor->mode = 2;
          ANIM_RESET(actor, 2);
          continue;
        }
        continue;
      }
      continue;
    }

    case 507:
    case 508:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      continue;

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
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
        g168[0] = GetRandomU32() & 3;
        g168[1] = GetRandomU32() & 3;
        g168[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, g168);
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
          RefreshActorDerivedState(actor, 4);
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
          CopyVector(g184, &actor->posX);
          g184[2] += 0x400;
          FindGroundHeightBelow(g184, 0x10000);
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
            UnpackWorldCollisionTri(st->groundTri, g216[0]);
            AddVector(g216[0], g216[0], g216[1]);
            AddVector(g216[0], g216[0], g216[2]);
            actor->posX = g216[0][0] / 3;
            actor->posY = g216[0][1] / 3;
            actor->posZ = g216[0][2] / 3;
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(g200, &actor->posX);
            g200[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(g200, 0x1000) > 0) {
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

          ScaleVector3Sat(g256, (int *)st, g_nFrameStep);
          RShiftVector3(g256, 1);
          dist = VectorLength(g256, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(g256, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(g256, at, g256);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (g256[2] < 0) {
            if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          g256[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(g256, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(g272, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(g256, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(g272[2], VectorLength(g272, 0));

              if ((g256[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, g256);
                actor->posZ = floor;
                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((g272[0] * g272[0]) + (g272[2] * g272[2])),
                      g272[1]);
                  st->roll = -ArcTan2_8bit(g272[2], g272[0]);
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
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, 8, &actor->sfxSlot);

            if (ReflectVectorOffNormal((int *)st, g272, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            g256[2] -= 0xF0;
            CopyVector(at, g256);
            FindGroundHeightBelow(g256, 0x10000);
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
            SubtractVector(g288, &spy->posX, (int *)st);
            RShiftVector3(g288, 5);

            if (VectorLength(g288, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(g288, g288, st->timer);
              AddVector(&actor->posX, (int *)st, g288);
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
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, 0x10, 0);
              RefreshActorDerivedState(actor, 4);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)8);
              g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x10);
              if (g_nExtraLives < 0x63) {
                g_nExtraLives += 1;
              }
              CommitActorKillGems(0, actor);
              DespawnActorRecord(actor);
              continue;
            }

            if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
              PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, 0x10, 0);
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
        if (ApproxDist2D(&rider->posX, &SPY.posX) > 0xA00) {
          actor->mode = 1;
        }
        break;

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
                g_pfnLevelEmitParticles(0x18, 0x50, at, (int *)0xC);
                RefreshActorDerivedState(actor, 1);
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
          SubtractVector(g184, st->wanderTo, here);
          turn = SignedAngleDelta8(st->faceYaw, actor->rotZ);
          w0 = LookupCosine((st->wanderTimer - 1) << 7);
          wSpan = w0 - LookupCosine(st->wanderTimer << 7);
          wDen = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;
          actor->posX += (g184[0] * wSpan) / wDen;
          actor->posY += (g184[1] * wSpan) / wDen;
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
          g_pfnLevelEmitParticles(0x18, 0x50, &actor->posX, (int *)0xC);
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
        g184[0] = 0;
        g184[1] = 0x64;
        g184[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g184, g184);
        AddVector(g184, g184, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g184, 0);
        g184[0] = 0;
        g184[1] = -0x64;
        g184[2] = 0;
        ApplyActiveGteRotation(g184, g184);
        AddVector(g184, g184, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g184, 0);
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

            CopyVector(g304, &st->target->posX);

            if (high == 0) {
              g304[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              g304[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              g304[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              g304[1] -=
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

            SubtractVector(g304, g304, &actor->posX);
            dist = VectorLength(g304, 1);

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
              ScaleVector3ByRatio(g304, dist, limit);
            } else {
              ScaleVector3ByRatio(g304, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, g304);
            TurnActorYawTowardAngle(actor, ArcTan2(g304[0], g304[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(g304, 0), g304[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_anSparxHealthBlock[0] += 1;
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, 0x10, 0);
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

            SubtractVector(g320, &st->target->posX, &actor->posX);
            dist = VectorLength(g320, 1);

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

              if (GetSoundVoiceStatusByOwner(actor, g_pLevelSampleBankHeader[SFX_GEM_BOUNCE]) == 0) {
                g_nSfxOverrideFlags = 1;
                g_nSfxOverrideVolR = 0x3600;
                g_nSfxOverrideVolL = 0x3600;
                PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, 8, &actor->sfxSlot);
              }
            } else {
              int lim = 0x136;

              if (lim < dist) {
                ScaleVector3ByRatio(g320, dist, lim);
              } else {
                ScaleVector3ByRatio(g320, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g320);
              TurnActorYawTowardAngle(actor, ArcTan2(g320[0], g320[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(g320, 0), g320[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* No target: hover at a jittered offset beside Spyro. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            g184[0] = -0x258;
            g184[1] = (GetRandomU32() & 0x310) - 0x188;
            g184[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = g184[0];
            st->drift[1] = g184[1];
            st->drift[2] = g184[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              g184[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              g184[1] = st->drift[1];
              g184[2] = st->drift[2];
              if (g184[2] < 0x64) {
                g184[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                g184[0] = st->drift[0] - 0x200;
              } else {
                g184[0] = st->drift[0];
              }
              g184[1] = st->drift[1];
              g184[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, g184, g184);
            AddVector(g184, g184, &SPY.posX);
            at = &actor->posX;
            SubtractVector(g184, g184, at);
            RShiftVector3(g184, 2);
            AddVector(at, at, g184);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (g184[2] >= 0x21) {
              g184[2] = 0x20;
            }
            if (g184[2] < -0x20) {
              g184[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = g184[2];
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

    /* Type 126: a path walker. Turns to face Spyro when he is near, walks
     * its path, sings its tune while it rests, and can be carried and
     * thrown by its rider. */
    case 126: {
      PathWalkerState *st = actor->state;

      if (st->init == 0) {
        st->init = one;
        st->path->cur = st->startCur;
        if (st->mode == 6) {
          DespawnActorRecord(actor);
          continue;
        }
        if (st->mode != 0) {
          ENTER_POSE(actor, 2);
        }
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) &&
          (actor->mode < 8 || actor->mode == 0x63)) {
        actor->flags = 0;
        if (st->mode == 3) {
          ((int *)g_pActorListBase[st->rider].state)[18] = one;
          st->path->count -= 4;
          st->mode = one;
        }
        actor->mode = 8;
        ANIM_RESET(actor, 8);
        continue;
      }
      RefreshActorDerivedState(actor, 1);
      FindGroundHeightBelow(&actor->posX, 0x10000);
      EncodeCachedVecToActorDirCode(actor);

      switch (actor->mode) {
      case 0:
        if (st->mode == 3) {
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000) {
          st->next = 3;
          st->legStep[0] = 0xFF;
          ENTER_POSE(actor, 1);
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          st->next = 4;
          ENTER_POSE(actor, 1);
        }
        continue;

      case 1:
        if (g_nActorAnimDone != 0) {
          ANIM_QUEUE(actor, st->next);
        }
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (st->mode != 3) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000) {
            ENTER_POSE(actor, 3);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000 &&
              g_nActorAnimDone != 0 && (GetRandomU32() & 4)) {
            ANIM_QUEUE(actor, 4);
          }
          continue;
        }
        if (st->rideMode == one) {
          GetActorAttachPoint(&g_pActorListBase[st->rider], 1, g336);
          GetActorAttachPoint(&g_pActorListBase[st->rider], 2, g352);
          actor->posX = g336[0] + ((g352[0] - g336[0]) >> 1);
          actor->posY = g336[1] + ((g352[1] - g336[1]) >> 1);
          actor->posZ = g336[2] + ((g352[2] - g336[2]) >> 1);
          actor->rotZ =
              ArcTan2(actor->posX - g_pActorListBase[st->rider].posX,
                            actor->posY - g_pActorListBase[st->rider].posY, 0);
          continue;
        }
        if (st->rideMode == 2) {
          actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
          actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
          CopyVector(g368, &actor->posX);
          if (CollideSphereWithWorldAndActors(g368, 0x100, 0x100) != 0) {
            actor->mode = 7;
            ANIM_RESET(actor, 7);
            continue;
          }
          actor->posZ += st->vz;
          st->vz += st->dvz;
          ANIM_GO(actor, 6);
        }
        continue;

      case 3:
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1000) {
          if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
            st->call = (int)GetRandomU32() % 32 + 100;
            PlayActorMeshVoice(actor, (int)GetRandomU32() % 3 + 2, 0);
          }
          TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, sixteen, 0);
          continue;
        }
        if (TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, 0x16, 0) != 0) {
          ENTER_POSE(actor, 2);
        }
        continue;

      case 4:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1000) {
          ENTER_POSE(actor, 3);
        }
        if (g_nActorAnimDone != 0 && (GetRandomU32() & 4)) {
          ENTER_POSE(actor, 2);
        }
        continue;

      case 7:
        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 3);
          actor->mode = 0x63;
        }
        continue;

      case 8:
        if ((unsigned)(FindGroundHeightBelow(&actor->posX, 0x10000) - 0x81) < 0x177F) {
          actor->posZ -= 0x40;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        RefreshActorDerivedState(actor, 5);
        if (g_nActorAnimDone != 0) {
          st->timer = st->period;
          switch (st->mode) {
          case 0:
          case 1:
          case 3:
            ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
            break;
          case 2:
            if (g_pActorListBase[st->partner].mode >= 6 &&
                ((int *)g_pActorListBase[st->partner].state)[18] == 0) {
              st->flag = one;
            } else {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
            }
            break;
          case 4:
            if (((GetWorldChunkAnimState(1) >> 8) & 0xFF) > 0 ||
                !(GetWorldChunkAnimState(1) & 2)) {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(2, 0xFC, 0);
              st->sound = 2;
            } else {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(1, 0xFC, 0);
              st->sound = one;
            }
            break;
          }
          PlayActorMeshVoice(actor, 0, 0);
          ANIM_QUEUE(actor, 9);
        }
        continue;

      case 9:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        RefreshActorDerivedState(actor, 5);
        if (st->next == 0x63) {
          ENTER_POSE(actor, 0xC);
        }
        if (st->flag != 0 && (GetWorldChunkAnimState(st->sound) & 2)) {
          st->flag = 0;
          st->timer = 0x14A;
          ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x50;
          PlayActorMeshVoice(actor, 1, 0);
          ENTER_POSE(actor, 0xA);
        }
        continue;

      case 10:
        if (st->next == 0x63) {
          ENTER_POSE(actor, 0xC);
        }
        switch (st->mode) {
        case 0:
        case 1:
        case 3:
        case 4:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
            ENTER_POSE(actor, 0xC);
          }
          break;
        case 2:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            int s;

            if (g_pActorListBase[st->partner].mode >= 6) {
              ((int *)g_pActorListBase[st->partner].state)[3] = 0x63;
            }
            s = GetWorldChunkAnimState(st->sound);
            if ((s & 2) && ((s >> 8) & 0xFF) < 0x14) {
              SetWorldChunkAnimStep(st->sound, 0x1E, 0);
            }
            ModifyWorldChunkAnimFlags(st->sound, 0xFC, 0);
            ENTER_POSE(actor, 0xC);
          }
          break;
        }
        continue;

      case 12:
        StopSoundVoicesByOwner(actor, 1);
        if (g_nActorAnimDone != 0) {
          st->next = 2;
          actor->mode = 2;
          actor->flags = 0;
          ANIM_RESET(actor, 2);
          continue;
        }
        continue;

      case 99:
        if (TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, sixteen, 4) == 2) {
          st->rideMode = 0;
          actor->rotY = 0;
          ((int *)g_pActorListBase[st->rider].state)[16] = one;
          ENTER_POSE(actor, 2);
        }
        continue;
      }
      continue;
    }

    /* Type 150: Jacques. Hops from stop to stop around his path ring,
     * throwing rocks (47) when Spyro looks at him from afar. */
    case 150: {
      JacquesState *st = actor->state;

      if (st->init == 0) {
        actor->updateDist = 0x20;
        st->stops[0] = 3;
        st->stops[1] = 6;
        st->stops[2] = 7;
        st->stops[3] = 10;
        st->u54 = 0;
        st->init = one;
        st->stops[4] = 11;
        st->stops[5] = 13;
        st->stops[6] = 0;
        ModifyWorldChunkAnimFlags(7, 0xFC, 0);
        ModifyWorldChunkAnimFlags(8, 0xFC, 0);
      }
      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) && actor->mode != 3 && actor->mode != 6) {
        actor->flags = 0;
        switch (st->path->cur) {
        case 11:
          ModifyWorldChunkAnimFlags(9, 0xFC, 0);
          break;
        case 13:
          ModifyWorldChunkAnimFlags(10, 0xFC, 0);
          break;
        case 0:
          ENTER_POSE(actor, 3);
        }
        ANIM_GO(actor, 0);
        actor->mode = 10;
        continue;
      }
      RefreshActorDerivedState(actor, 4);
      FindGroundHeightBelow(&spy->posX, 0xFFFF);
      ApproxDist2D(&actor->posX, &spy->posX);

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 0x10, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
            st->standGround == 0) {
          if (st->path->cur == 13) {
            ModifyWorldChunkAnimFlags(10, 0xFC, 0);
          }
          ANIM_GO(actor, 0);
          actor->mode = 10;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x2800 &&
            TickCountdownByFrameDelta(&st->timer, 4) != 0 && actor->drawn != 0 &&
            AbsAngleDelta8(g_nCameraEulerYaw >> 4,
                          ArcTan2(actor->posX - g_anCameraPos[0],
                                        actor->posY - g_anCameraPos[1], 0)) <
                0x1E) {
          ENTER_POSE(actor, 2);
        }
        continue;

      case 2:
        TrackEntityYawTowardSpyro(actor, 0x10, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
            st->standGround == 0) {
          if (st->path->cur == 13) {
            ModifyWorldChunkAnimFlags(10, 0xFC, 0);
          }
          st->u54 = 0;
          ANIM_GO(actor, 0);
          actor->mode = 10;
          continue;
        }
        if (g_nActorAnimDone != 0) {
          st->timer = 0x4B;
          ENTER_POSE(actor, 0);
        }
        if (actor->nextFrame == 0 && st->child == 0) {
          st->child = g_pfnLevelSpawnActor(0x2F, actor);
          st->child->groundCode = -1;
          ((CarriedRockState *)st->child->state)->partner = actor;
          ((CarriedRockState *)st->child->state)->mode = 1;
        }
        continue;

      case 3:
        if (g_nActorAnimDone != 0) {
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        continue;

      case 10:
        if (st->freezesSpyro != 0) {
          g_nGameplayBlocked = one;
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        }
        if (st->path->cur != st->stops[st->idx]) {
          TweenActorAlongPath(actor, st->path, st->prevLeg, st->legStep, 8, 4);
          continue;
        }
        ZeroVector(st->prevLeg);
        actor->rotY = 0;
        switch (st->path->cur) {
        case 3:
        case 6:
        case 10:
        case 13:
          st->atStop = one;
          st->standGround = 0;
          st->timer = 0;
          st->stopActor = &g_pActorListBase[38];
          break;
        case 0:
        case 7:
        case 11:
          st->atStop = one;
          st->standGround = one;
          st->timer = 0;
          st->stopActor = &g_pActorListBase[41];
          break;
        }
        st->timer = 0x8C;
        g_nGameplayBlocked = 0;
        st->idx++;
        actor->flags = 0;
        actor->mode = 0;
        continue;
      }
      continue;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case 160: {
      L26Critter *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 1) {
        EBB_BREAK();
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        {
          if (actor->flags & ACTOR_HIT_FLAME) {
            st->speed = 0x8C;
          } else {
            st->speed = 0xFA;
          }
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 1;
        ANIM_RESET(actor, 1);
        continue;
      }

      switch (actor->mode) {
      case 0:
        UpdateActorWander(actor, st);
        break;

      case 1:
        if (st->speed >= 0x10) {
          st->speed -= 0xF;
          MoveActorAlongAngle(actor, st->knock, st->speed, 0, 0x2BC, 5);
        }
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
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

            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_REWARD], actor, 0x10, 0);
            RefreshActorDerivedState(actor, 4);

            for (i = 0; i < 6; i++) {
              g_pfnLevelEmitParticles(1, 0xC, actor, (int *)(0x8080 + (i << 24)));
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

    /* Type 174: the locked chest (LockedChestState). Shakes when hit; with
     * the key held and Spyro facing it up close, the key flies into its lock
     * and the chest bursts and drops its reward. */
    case ACTOR_TYPE_LOCKED_CHEST: {
      LockedChestState *st = actor->state;
      Actor *key = &g_pActorListBase[st->idx];

      if (actor->flags & ACTOR_HIT_SUPER) {
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
        } else if (actor->flags & ACTOR_HITS_SPYRO) {
          st->shake = 1;
          st->rotX = actor->rotX;
          st->rotY = actor->rotY;
          st->restZ = actor->posZ;
        }
      }

      switch (actor->subMode) {
      case 0:
        if (g_nLevelKeyState == 0 || g_nLevelKeyState == 3) {
          g_nLevelKeyState = 0;
          actor->updateDist = sixteen;
          key->updateDist = 0x40;
          key->cullRadius = 0x18;
        } else {
          g_nLevelKeyState = 1;
          actor->subMode = 1;
          key->scale = 0x40;
          key->groundCode = 0;
          key->visZone = 0xFF;
          key->subMode = 2;
        }
        break;

      case 1: {
        int height;
        int drop;

        if (g_nLevelKeyState == 1 &&
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
            key->rotX = 0;
            key->rotY = 0;
            key->rotZ = spy->bodyRotZ + 0x80;
          }
        }
        break;
      }

      case 2:
        if (g_dwPadPressed & PAD_CROSS) {
          st->shake = 0xD2;
        }
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        key->cullRadius = 0x18;
        st->shake += g_nFrameStep;
        g_nGameplayBlocked = 1;

        if (st->shake < 0xC0) {
          g368[0] = 0x200;
          g368[1] = 0;
          g368[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g368, g368);
          AddVector(g368, g368, &actor->posX);
          SubtractVector(g368, g368, &spy->posX);
          g368[2] -= 0x100;
          RShiftVector3(g368, 5);
          ScaleVector3Sat(g368, g368, st->shake);
          DivideVector3(g368, g368, 6);
          AddVector(g368, g368, &spy->posX);
          g368[2] += 0x100;
          g368[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, g368);
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
          g384[0] = 0x200 - (st->shake - 0xC0) * 8;
          g384[1] = 0;
          g384[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, g384, g384);
          AddVector(&key->posX, g384, &actor->posX);
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
          g_pLockedChestReward = SpawnActorDeathDrop(actor, 1, 0, 0);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
          g_nLevelKeyState = 2;
          g_nCameraScriptStickRequestX = 0;
          g_nCameraScriptStickRequestY[0] = 0;
          g_nSpyroSwingEulerPitchTarget = 0;
        }

        RefreshActorDerivedState(key, 4);
        if (st->shake % 4 < 2) {
          g_pfnLevelEmitParticles(1, 0xC, (int *)key, (void *)0x8080);
        } else {
          g_pfnLevelEmitParticles(1, 0xC, (int *)key, (void *)0x5008080);
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

      if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) {
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

    case 234:
      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      continue;

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
        if (ApproxDist2D(at, &spy->posX) < 0x800) {
          SubtractVector(g400, at, &spy->posX);
          g400[2] = (g400[2] * 3) >> 2;

          if (VectorLength(g400, 1) < 0x440) {
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
          g_pfnLevelEmitParticles(10, 0x47, &actor->posX, 0);
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
      PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, 8, &actor->sfxSlot);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, 6, (int *)actor, 0);
      SubtractVector(g400, &spy->posX, &actor->posX);
      distance = VectorLength(g400, 0);
      if (distance < st->radius && g400[2] >= -0x3FF && g400[2] < st->spanZ &&
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

        g416[0] = (GetRandomU32() & 0xFE) - 0x7F;
        g416[1] = (GetRandomU32() & 0xFE) - 0x7F;
        g416[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(g416, g416, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, g416, (int *)1);
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
            g432[0] = (GetRandomU32() & 0xFE) - 0x7F;
            g432[1] = (GetRandomU32() & 0xFE) - 0x7F;
            g432[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(g432, g432, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, g432, (int *)1);
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
        if (actor->flags & ACTOR_HITS_SPYRO) {
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
          ZeroVector(g448);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, g448);
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
        if (actor->flags & ACTOR_HIT_SUPER) {
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
            CopyVector(g464, &st->prize->posX);
            g464[0] += g_anCosineLut[j * 43] >> 6;
            g464[1] += g_anSineLut[j * 43] >> 6;
            g464[2] += 0x28;
            g480[0] = g_anCosineLut[j * 43] >> 7;
            g480[1] = g_anSineLut[j * 43] >> 7;
            g480[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, g464, g480);
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
        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0xA00) {
          actor->mode = 1;
        }
        break;

      case 1:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x380) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0x200
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
            SetSpawnAnchorFromActor(actor, actor->rotZ);
            actor->mode = 2;
            actor->anim = 1;
            actor->subMode = 0;
          }
        }
        break;

      case 2:
        actor->subMode += g_nFrameStep;
        if (actor->subMode >= 0x30) {
          actor->mode = 0;
          actor->anim = 0;
        }
        break;
      }
      break;

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

      if ((actor->flags & ACTOR_HITS_SPYRO) == 0) {
        break;
      }

      rider = SpawnActorDeathDrop(actor, 5, 0, 0);
      if (st->rideMode == 1) {
        ((unsigned char *)rider->state)[0xE] = one;
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
        g448[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g448[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g448[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, g448);
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
          g448[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g448[1] = (g_anSineLut[bearing] * 3) >> 4;
          g448[2] = 0;
          g464[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g464[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g464[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(g496, g464, st->radius - 1);
          LShiftVector3(g464, 1);
          arm = st->radius - 1;
          g496[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          g512[0] = g448[0] * g_anCosineLut[arm * 2];
          g512[1] = g448[1] * g_anCosineLut[arm * 2];
          g512[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(g464, g464, st->phase);
          SubtractVector(g496, g496, g464);
          swing = &g_anCosineLut[bearing];
          actor->posX = g448[0] * *swing;
          actor->posY = g448[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, g512);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g448);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, g496);
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
