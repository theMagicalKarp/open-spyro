/* Level27_UpdateActors -- per-frame actor update for level 27
 * (Haunted Towers). 0x8007B510, 41,980 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. Most
 * classes are shared with levels 7/8/9/15/22/25/26/31/32/33; the level-27
 * classes are the rock thrower and its rocks (134/135), the breakable wall
 * (136), the charging turret with its scripted camera (142/241, as in
 * level 14), the fence guards (203) and their captain (204).
 *
 * Load-bearing source forms:
 *   - No loop-top carriers; the dormant check comes first. The scratch
 *     arrays are declared in the original's slot order and named by sp
 *     offset (g056..g576).
 *   - Case 203's fence projection reuses `len` for the push distance, which
 *     keeps t2 in use and puts the whole function's constant reloads in t3;
 *     its state-3 exits share one ENTER_POSE through a goto, and the hit
 *     flags are set through SPY.triggerEvents.
 *   - Case 204 calls GetActorAttachPoint through a copy of `actor` made before
 *     the frame-parity test, and its path-node draw loop reloads st->path
 *     through a local at the bottom.
 *   - Case 136 spaces the wall parts with -(i + 1) * d, and splits the
 *     rubble height into a temp so fold keeps the constant at the end.
 *   - Case 142 reads the camera's sine through a pointer local.
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


/* ==== Level 28 classes ==== */


/* ==== Level 24 classes ==== */


/* ==== Level 27 classes ==== */


/* ==== The function ==== */
#define spy (&SPY)
#define one 1
void Level27_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  char *base;
  int dt;

  /* Scratch vectors. */
  int g056[3];
  int g072[3];
  int g088[3];
  int g104[3];
  int g120[3];
  int g136[3];
  int g152[3][3];
  int g192[3];
  int g208[3];
  int g224[3];
  int g240[3];
  int g256[3];
  int g272[3];
  int g288[3];
  int g304[3];
  int g320[3];
  int g336[6];
  int g360[3];
  int g376[3];
  int g392[3];
  int g408[3];
  int g424[3];
  int g440[3];
  int g456[3];
  int g472[6];
  int g496[3];
  int g512[3];
  int g528[3];
  int g544[3];
  int g560[3];
  int g576[3];

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

    /* Dragon egg (ShotState). */
    case ACTOR_TYPE_DRAGON_EGG: {
      ShotState *st;

      st = (ShotState *)actor->state;

      if (actor->mode != 0 && actor->mode < 5) {
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        if (SPY.state != SPYRO_ST_IDLE && SPY.state != SPYRO_ST_CHARGE_BONK) {
          actor->mode = 5;
        }
      }
      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, 0xC, (int *)actor, (void *)0x602080);
      }

      switch (actor->mode) {
      case 1:
        g_nGameplayBlocked = 1;
        VectorToSVectorShr2(st->from, &actor->posX);
        st->heading =
            ArcTan2_8bit(actor->posX - SPY.posX, actor->posY - SPY.posY) +
            0x40;
        st->speed = 0;
        st->phase = 0;
        g_nHudWorldEggRollRequest.v = 1;
        actor->rotX = 0;
        actor->rotY = 8;
        actor->mode = actor->mode + 1;
        break;

      case 2:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed + step;
          st->heading = st->heading + step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 3);
          }
        } else {
          CopyVector(g056, &SPY.posX);
          {
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int s = LookupSine(SPY.bodyRotZ << 4);
            int cr = c * 724;
            int sr = s * 724;
            g056[0] = g056[0] + ((sr + cr) >> 12);
          }
          {
            int s = LookupSine(SPY.bodyRotZ << 4);
            int c = LookupCosine(SPY.bodyRotZ << 4);
            int sr = s * 724;
            int cr = c * 724;
            g056[1] = g056[1] + ((sr - cr) >> 12);
          }
          g056[2] = g056[2] + 0x300;
          VectorToSVectorShr2(st->to, g056);
          VectorToSVectorShr2(st->from, &actor->posX);
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = 5;
        }
        break;

      case 3:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x40) {
          SVectorToVectorShl2(g072, st->from);
          SVectorToVectorShl2(g088, st->to);
          SubtractVector(g088, g088, g072);
          ScaleVector3Sat(g088, g088, st->phase);
          RShiftVector3(g088, 6);
          AddVector(&actor->posX, g072, g088);
          actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
        } else {
          CopyVector(g104, &SPY.posX);
          g104[0] = g104[0] + (LookupCosine(SPY.bodyRotZ << 4) >> 3);
          g104[1] = g104[1] + (LookupSine(SPY.bodyRotZ << 4) >> 3);
          g104[2] = g104[2] + 0x300;
          VectorToSVectorShr2(st->from, g104);
          st->heading = SPY.bodyRotZ - 0x40;
          st->speed = 0x200;
          st->phase = 0;
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = 5;
        }
        break;

      case 4:
        st->phase = st->phase + g_nFrameStep;
        if (st->phase < 0x80) {
          int step = g_nFrameStep << 2;
          st->speed = st->speed - step;
          st->heading = st->heading - step;
          SVectorToVectorShl2(&actor->posX, st->from);
          actor->posX += (LookupCosine(st->heading << 4) * st->speed) >> 12;
          actor->posY += (LookupSine(st->heading << 4) * st->speed) >> 12;
          {
            int ph = st->phase;
            actor->rotZ = actor->rotZ + (g_nFrameStep << 2);
            actor->posZ = actor->posZ - (ph << 2);
          }
        } else {
          actor->mode = actor->mode + 1;
        }
        if ((g_dwPadPressed & PAD_CROSS) != 0) {
          actor->mode = 5;
        }
        break;

      case 5:
        SubtractVector(g120, &actor->posX, &CAM.posX);
        WorldToCameraRotate(g120, g120);
        g_pfnLevelEmitParticles(0x10, 0x4D, g120, 0);
        g_nGameplayBlocked = 0;
        ZeroVector(&SPY.headLook);
        CommitActorKillGems(0, st->killActor);
        g_nWorldEggsRecovered = g_nWorldEggsRecovered + 1;
        g_anLevelEggsRecovered[g_nLevelIntroIndex] += 1;
        g_nHudWorldEggRollRequest.v = 0;
        DespawnActorRecord(actor);
        break;

      default:
        break;
      }
      break;
    }

    /* Bubble vent: rises out of the ground and pops into four bubbles when it
     * reaches the surface. */
    case 38: {
      int *st = actor->state;

      switch (actor->mode) {
      case 0:
      case 1:
      case 3:
        if (g_nActorAnimDone != 0 && TickCountdownByFrameDelta(st, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 2: {
        int i;
        int a;

        CopyVector(g104, &actor->posX);
        if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
          DespawnActorRecord(actor);
          continue;
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x80, 0x20020) != 0) {
          i = 0;
          a = GetRandomU32() & 0x3F;
          for (; i < 4; i++, a += 0x40) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x26, actor)->rotX = a;
          }
          DespawnActorRecord(actor);
          continue;
        }
        if (TickCountdownByFrameDelta(st, 4) != 0 ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(g104, &actor->posX) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if ((*st & 7) == 0) {
          g_pfnLevelEmitParticles(1, 7, &actor->posX, (void *)0x10);
        }
        break;
      }
      }
      break;
    }

    /* Explosion shard with a ground bounce (ShardState). */
    case 67:
    case 68:
    case 133:
    case 215:
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
        g104[0] = GetRandomU32() & 3;
        g104[1] = GetRandomU32() & 3;
        g104[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, g104);
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
          CopyVector(g056, &actor->posX);
          g056[2] += 0x400;
          FindGroundHeightBelow(g056, 0x10000);
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
            UnpackWorldCollisionTri(st->groundTri, g152[0]);
            AddVector(g152[0], g152[0], g152[1]);
            AddVector(g152[0], g152[0], g152[2]);
            actor->posX = g152[0][0] / 3;
            actor->posY = g152[0][1] / 3;
            actor->posZ = g152[0][2] / 3;
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(g136, &actor->posX);
            g136[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
            if (FindGroundHeightBelow(g136, 0x1000) > 0) {
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

          ScaleVector3Sat(g192, (int *)st, g_nFrameStep);
          RShiftVector3(g192, 1);
          dist = VectorLength(g192, 1);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(g192, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(g192, at, g192);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (g192[2] < 0) {
            if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          g192[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(g192, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(g208, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(g192, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(g208[2], VectorLength(g208, 0));

              if ((g192[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, g192);
                actor->posZ = floor;
                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((g208[0] * g208[0]) + (g208[2] * g208[2])),
                      g208[1]);
                  st->roll = -ArcTan2_8bit(g208[2], g208[0]);
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
            PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

            if (ReflectVectorOffNormal((int *)st, g208, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            g192[2] -= 0xF0;
            CopyVector(at, g192);
            FindGroundHeightBelow(g192, 0x10000);
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
            SubtractVector(g224, &spy->posX, (int *)st);
            RShiftVector3(g224, 5);

            if (VectorLength(g224, 1) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(g224, g224, st->timer);
              AddVector(&actor->posX, (int *)st, g224);
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
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
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
              PlaySoundEffect(g_pLevelSampleBankHeader[0], actor, 0x10, 0);
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

          SubtractVector(g136, st->tgt, &actor->posX);

          angleDiff = SignedAngleDelta8(st->angle, actor->rotZ);

          cosWeight1 = LookupCosine((st->t20 - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->t20 << 7);
          totalWeight = LookupCosine((st->t20 - 1) << 7) + 0x1000;

          actor->posX += (g136[0] * cosDelta) / totalWeight;
          actor->posY += ((g136[1] * cosDelta) / totalWeight);
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
        g136[0] = 0;
        g136[1] = 0x64;
        g136[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g136, 0);
        g136[0] = 0;
        g136[1] = -0x64;
        g136[2] = 0;
        ApplyActiveGteRotation(g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, g136, 0);
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

            CopyVector(g056, &st->target->posX);

            if (high == 0) {
              g056[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              g056[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              g056[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              g056[1] -=
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

            SubtractVector(g056, g056, &actor->posX);
            dist = VectorLength(g056, 1);

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
              ScaleVector3ByRatio(g056, dist, limit);
            } else {
              ScaleVector3ByRatio(g056, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, g056);
            TurnActorYawTowardAngle(actor, ArcTan2(g056[0], g056[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(g056, 0), g056[2], 0);
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

            SubtractVector(g240, &st->target->posX, &actor->posX);
            dist = VectorLength(g240, 1);

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
                ScaleVector3ByRatio(g240, dist, lim);
              } else {
                ScaleVector3ByRatio(g240, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g240);
              TurnActorYawTowardAngle(actor, ArcTan2(g240[0], g240[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(g240, 0), g240[2], 0);
              actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            }
            break;
          }
          }
        } else {
          /* No target: hover at a jittered offset beside Spyro. */
          actor->cullRadius = 0x10;

          if (st->timer <= 0) {
            g136[0] = -0x258;
            g136[1] = (GetRandomU32() & 0x310) - 0x188;
            g136[2] = g_nCameraCurrentMode == CAM_MODE_CHARGE ? (GetRandomU32() & 0x7F) + 0x64
                                     : (GetRandomU32() & 0x1FF) - 0xC8;
            EBB_BREAK();
            st->drift[0] = g136[0];
            st->drift[1] = g136[1];
            st->drift[2] = g136[2];
            st->timer = GetRandomU32() & 0x7B;
          } else {
            int *at;

            st->timer -= g_nFrameStep;
            EBB_BREAK();

            if (g_nCameraCurrentMode == CAM_MODE_CHARGE) {
              g136[0] = st->drift[0] + (g_nSpyroMotionSpeed >> 2);
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
              if (g136[2] < 0x64) {
                g136[2] = 0x64;
              }
            } else {
              if (g_nCameraCurrentMode == (CAM_HELD | CAM_MODE_FREE_LOOK)) {
                g136[0] = st->drift[0] - 0x200;
              } else {
                g136[0] = st->drift[0];
              }
              g136[1] = st->drift[1];
              g136[2] = st->drift[2];
            }

            RotateVectorByMatrix(&SPY.bodyMtx, g136, g136);
            AddVector(g136, g136, &SPY.posX);
            at = &actor->posX;
            SubtractVector(g136, g136, at);
            RShiftVector3(g136, 2);
            AddVector(at, at, g136);

            if (CollideSphereWithWorldAndActors(at, 0x100, 0x100) != 0) {
              CopyVector(at, &g_anCollisionResolvedPos);
            }

            if (g136[2] >= 0x21) {
              g136[2] = 0x20;
            }
            if (g136[2] < -0x20) {
              g136[2] = -0x20;
            }

            actor->rotX = 0;
            actor->rotY = g136[2];
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

    /* Type 134: a rock thrower. Watches its patch, walks its path, throws
     * rocks (135) at Spyro, and can escort its partner. */
    case 134: {
      RockThrowerState *st = actor->state;

      TickActorFlameGlow(actor);
      if ((actor->flags & 0xB0000) && actor->mode != 3) {
        Actor *o;
        FallingChunkState *c;

        st->speed = 0x15E;
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, 0),
                                    g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->lift = 0x8C;
        st->timer = 0x3C;
        actor->flags = 0;
        o = g_pfnLevelSpawnActor(0x87, actor);
        RefreshActorDerivedState(actor, 4);
        GetActorAttachPoint(actor, 0, &o->posX);
        o->rotZ = actor->rotZ;
        c = o->state;
        c->noFloor = 1;
        c->age = 0;
        c->vx = (g_anCosineLut[(st->heading - 10) & 0xFF] * 0x4B) >> 10;
        c->vy = (g_anSineLut[(st->heading - 10) & 0xFF] * 0x4B) >> 10;
        c->vz = 0xDC;
        c->spin[0] = RandomSignedInRange(7, 0xF);
        c->spin[1] = RandomSignedInRange(7, 0xF);
        c->spin[2] = RandomSignedInRange(7, 0xF);
        c->grav = -0xC;
        if (st->flags & 2) {
          g_pActorListBase[st->partner].subMode = 1;
        }
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, 3);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->flags & 1) &&
            (((SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x1F4
                                 : -SPY_DZ(actor) < 0x1F4) &&
              ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range) ||
             (st->link != -1 && g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) ||
             IsPointInRotatedBox(&g_anSpyroWorldPos, st->vol) != 0)) {
          ENTER_POSE(actor, 1);
        }
        if (st->flags & 2) {
          actor->mode = 4;
          continue;
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && g_nGameplayBlocked == 0 &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range2 &&
            SPY_DZ(actor) < 0xC8 && CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          actor->subMode = 0;
          ENTER_POSE(actor, 2);
        }
        ANIM_GO(actor, 0);
        break;

      case 1:
        st->flags &= ~1;
        if (st->range != 0 &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1400) {
          actor->subMode = 0;
          ENTER_POSE(actor, 2);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, 5) ==
            0x100) {
          actor->mode = 10;
          continue;
        }
        break;

      case 2:
        TrackEntityYawTowardSpyro(actor, 8, 0, 0);
        if (actor->nextFrame >= 0x1A && g_nGameplayBlocked == 0 && actor->subMode == 0) {
          Actor *o = g_pfnLevelSpawnActor(0x87, actor);
          FallingChunkState *c;

          actor->subMode = 1;
          RefreshActorDerivedState(actor, 4);
          GetActorAttachPoint(actor, 0, &o->posX);
          o->rotZ = ArcTan2(g_anSpyroWorldPos - o->posX, g_nSpyroWorldPosY - o->posY, 0);
          c = o->state;
          c->noFloor = 0;
          c->age = 0;
          c->vx = (g_anCosineLut[o->rotZ] * 0x4B) >> 10;
          c->vy = (g_anSineLut[o->rotZ] * 0x4B) >> 10;
          c->vz = 0;
          c->spin[0] = RandomSignedInRange(7, 0xF);
          c->spin[1] = RandomSignedInRange(7, 0xF);
          c->spin[2] = RandomSignedInRange(7, 0xF);
          c->grav = 0;
          c->u0E = 0;
        }
        if (g_nActorAnimDone != 0) {
          actor->subMode = 0;
          st->timer = 0x28;
          ENTER_POSE(actor, 0);
        }
        break;

      case 3:
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->lift, 0xC, 0x10);
        if (g_nActorAnimDone != 0 && actor->nextAnim == 3) {
          ANIM_SET(actor, 4);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 4: {
        int spd = 0x78;
        int d = ApproxDist2D(&actor->posX, &g_pActorListBase[st->partner].posX);

        ANIM_GO(actor, 1);
        if (d < 0xA8C) {
          spd = 0x64;
        } else if (d > 0xC80) {
          spd = 0x8C;
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->partner].posX - actor->posX,
                                    g_pActorListBase[st->partner].posY - actor->posY,
                                    0),
                      4, 0, 0);
        MoveActorAlongHeading(actor, spd, 0, 0, 5);
        if (IsPointInRotatedBox(&g_anSpyroWorldPos, st->vol) != 0) {
          st->trig = one;
        }
        if (st->trig != 0) {
          st->flags = one;
          g_pActorListBase[st->partner].subMode = 1;
          actor->mode = 0;
          continue;
        }
        break;
      }

      case 10:
        if (TrackEntityYawTowardSpyro(actor, 5, 2, 1) != 0) {
          ENTER_POSE(actor, 0);
        }
        break;
      }
      RefreshActorDerivedState(actor, 1);
      continue;
    }

    /* Type 135: a falling chunk. Rides the floor (or falls), and bursts into
     * rubble when it hits a wall or has lived long enough. */
    case 135: {
      FallingChunkState *st = actor->state;
      int hit = 0;
      int i;

      if (actor->mode != 0) {
        continue;
      }
      if (st->grav != 0) {
        st->vz += st->grav;
        if (st->vz < -0xF0) {
          st->vz = -0xF0;
        }
      } else {
        int h = ProbeActorGroundHeight(actor);
        int z = actor->posZ - 0x78;

        st->vz = h - z;
        if (st->vz > 0x28) {
          st->vz = 0x28;
        }
        if (st->vz < -0x28) {
          st->vz = -0x28;
        }
        if (ABS(st->vz) < 0x1E) {
          st->vz = 0;
        }
        if (st->age > 0x28) {
          st->vz = 0;
        }
      }
      g256[0] = 0;
      g256[1] = 0;
      g256[2] = 0;
      g_pfnLevelEmitParticles(1, 1, &actor->posX, (void *)g256);
      CopyVector(g256, &actor->posX);
      actor->posX += st->vx;
      actor->posY += st->vy;
      actor->posZ += st->vz;
      actor->rotX += st->spin[0];
      actor->rotY += st->spin[1];
      actor->rotZ += st->spin[2];
      st->age += g_nFrameStep;
      if (st->noFloor == 0) {
        hit = RaiseSpyroTriggerInRadius(&actor->posX, 0xDC, 0x86);
      }
      if (actor->subMode != 2 && st->age < 0x5B && hit == 0 &&
          CastRayWorldAndActors(g256, &actor->posX) == 0) {
        continue;
      }
      if (hit != 0) {
        g_anSpyroRequestVelocity.x = st->vx >> 2;
        g_nSpyroRequestVelocityY = st->vy >> 2;
        g_nSpyroRequestVelocityZ = 0x28;
      }
      PlayActorMeshVoice(actor, 0, 0);
      for (i = 0; i < 6; i++) {
        if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
          break;
        }
        g_pfnLevelSpawnActor(0x1CA, actor);
        g_pfnLevelSpawnActor(0x1CB, actor);
      }
      for (i = 0; i < 0x12; i++) {
        g288[0] = RandomIntInRange(-0x5A, 0x5A);
        g288[1] = RandomIntInRange(-0x5A, 0x5A);
        g288[2] = RandomIntInRange(-0x19, 0x3C);
        CopyVector(g304, g288);
        LShiftVector3(g304, 2);
        AddVector(g304, g304, &actor->posX);
        g_pfnLevelEmitParticles(1, 0xD, g304, (void *)g288);
      }
      g_pfnLevelEmitParticles(10, 0x46, &actor->posX, (void *)0x10);
      DespawnActorRecord(actor);
      continue;
    }

    /* Type 136: a breakable wall of four parts. Any part struck (with a
     * charge, for the tough kind) blows the whole wall into rubble. */
    case 136: {
      BreakableWallState *st = actor->state;
      int i;

      if (st->init == 0) {
        int m = -1;

        st->init = one;
        for (i = 0; i < 4; i++) {
          int d = 0x190;
          int h = (actor->rotZ + 0x40) & 0xFF;

          st->part[i] = g_pfnLevelSpawnActor(0x84, actor);
          CopyVector(&st->part[i]->posX, &actor->posX);
          if (i < 2) {
            d = -(i + 1) * d;
          } else {
            d = (i - 1) * d;
          }
          st->part[i]->posX += (d * g_anCosineLut[h]) >> 12;
          st->part[i]->posY += (d * g_anSineLut[h]) >> 12;
          m--;
          RefreshActorDerivedState(st->part[i], 2);
        }
        if (st->kind == 1) {
          ANIM_SET(actor, 1);
        }
      }
      if (st->kind == 1) {
        st->lodHandle = SelectActorLodBand(actor, st->lodHandle);
      }
      actor->flags = 0;
      for (i = 0; i < 4; i++) {
        actor->flags |= st->part[i]->flags;
        if (st->part[i]->flags != 0 &&
            (st->kind != 1 || (st->part[i]->flags & 0x80000))) {
          int h = actor->rotZ;
          int spd = 0x64;
          short *cp;
          short *sp2;
          int j;

          if (st->kind == 1) {
            PlayActorMeshVoice(actor, 1, 0);
          } else {
            PlayActorMeshVoice(actor, 0, 0);
          }
          if (AbsAngleDelta8(actor->rotZ,
                            ArcTan2(g_anSpyroWorldPos - actor->posX,
                                          g_nSpyroWorldPosY - actor->posY, 0)) < 0x40) {
            h = (h + 0x80) & 0xFF;
          }
          if (st->part[i]->flags & 0x20000) {
            spd += 0x50;
          } else if (st->part[i]->flags & 0x80000) {
            spd += 0xF0;
          }
          i = 0;
          cp = &g_anCosineLut[h];
          sp2 = &g_anSineLut[h];
          g272[0] = 0;
          g272[1] = 0;
          g272[2] = 0;
          do {
            Actor *o = g_pfnLevelSpawnActor(0x85, actor);
            ShardState *c = o->state;
            int a;
            int dd;

            if (st->kind == 1) {
              if (o->anim != 1) {
                ANIM_RESET(o, 1);
              }
              o->renderList = 0xA0;
              *(int *)&o->tint0 = 0xA18618;
            }
            a = (actor->rotZ + 0x40) & 0xFF;
            dd = (i % 3 - 1) * 800;
            o->posX += (dd * g_anCosineLut[a]) >> 12;
            o->posY += (dd * g_anSineLut[a]) >> 12;
            dd = i / 3 * 700 - 0x44C;
            o->posZ += dd;
            for (j = 0; j < 6; j++) {
              CopyVector(g320, &o->posX);
              g320[0] += RandomIntInRange(-0x190, 0x190);
              g320[1] += RandomIntInRange(-0x190, 0x190);
              g320[2] += RandomIntInRange(-0x190, 0x190);
              g_pfnLevelEmitParticles(1, 1, g320, (void *)g272);
            }
            o->scale = RandomIntInRange(0, 0xC) + 0x24;
            c->vel[0] += (spd * *cp) >> 12;
            c->vel[1] += (spd * *sp2) >> 12;
            c->life -= 0x14;
            i++;
          } while (i < 9);
          for (i = 0; i < 4; i++) {
            DespawnActorRecord(st->part[i]);
          }
          CommitActorKillGems(0, actor);
          MarkActorCommittedKill(actor);
          DespawnActorRecord(actor);
        } else {
          st->part[i]->flags = 0;
        }
      }
      continue;
    }

    /* Type 227. */
    case 142:
    case 241: {
      L14State227 *st = actor->state;

      if (st->heading == -1) {
        st->heading = actor->rotZ;
      }
      switch (actor->mode) {
      case 0: {
        int range = 0xA8C;

        if (st->kind == 3) {
          range = 0xD48;
        }
        if (st->alert != 0) {
          actor->mode = 4;
          actor->posZ -= 0x1F4;
          continue;
        }
        if (actor->subMode != 0) {
          actor->subMode = 0;
          st->cool = 0;
          actor->mode = 3;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if ((st->link == -1 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) &&
            st->cool == 0 && g_nSpyroFlameBreathTimer == 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < range &&
            ABS2(SPY_DZ(actor)) < 0x7D0 &&
            AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, 0)) < 0x1E) {
          st->timer = 0x4B;
          actor->subMode = 0;
          actor->mode = 1;
          continue;
        }
        break;
      }

      case 1: {
        int dz;
        short *nv;

        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        CopyVector(g272, &SPY.posX);
        g_nGameplayBlocked = 1;
        g272[0] += (g_anCosineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        g272[1] += (g_anSineLut[SPY.bodyRotZ] * 0xE1) >> 10;
        g272[2] -= 0x17C;
        dz = g272[2] - actor->posZ;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        CopyVector(st->cam, &SPY.posX);
        st->cam[0] += (g_anCosineLut[SPY.bodyRotZ] * 0x4B) >> 10;
        st->cam[1] += (*(nv = &g_anSineLut[SPY.bodyRotZ]) * 0x4B) >> 10;
        st->cam[2] += 0x64;
        g_pSpyroRequestCameraAnchor = st->cam;
        g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
        g_anCameraActiveTargetParams.w[2] = 0x400;
        g_anCameraActiveTargetParams.w[1] = 0;
        g_anCameraActiveTargetParams.w[3] = 0;
        g_anCameraActiveTargetParams.w[4] = 0;
        g_anCameraActiveTargetParams.w[5] = 0;
        SPY.requestMask |= (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
        g_anCameraActiveTargetParams.w[0] = -(SPY.bodyRotZ + 0x40) << 4;
        switch (actor->subMode) {
        case 0:
          if (ApproxDist2D(&actor->posX, g272) > 0x50) {
            MoveActorAlongAngle(actor,
                          ArcTan2(g272[0] - actor->posX,
                                        g272[1] - actor->posY, 0),
                          0x64, 0, 0, 0);
          } else {
            actor->posX = g272[0];
            actor->posY = g272[1];
          }
          if (ABS2(dz) > 0x28) {
            if (dz < 0) {
              if (dz < -0x32) {
                dz = -0x32;
              }
            } else if (dz > 0 && dz > 0x32) {
              dz = 0x32;
            }
            actor->posZ += dz;
          } else {
            actor->posZ = g272[2];
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            ANIM_GO(actor, 1);
            st->count = 0;
            actor->subMode = 1;
          }
          break;

        case 1: {
          int f;
          int *p;
          int *q;

          if (actor->nextFrame >= 6 &&
              (++st->count == 2 || actor->type == 0xF1)) {
            int r = 0x140;

            if (actor->type == 0xF1) {
              r = 0x190;
            }
            CopyVector(g272, &SPY.posX);
            g272[0] += (r * g_anCosineLut[SPY.bodyRotZ]) >> 12;
            g272[1] += (r * g_anSineLut[SPY.bodyRotZ]) >> 12;
            g_pfnLevelEmitParticles(1, 0x1F, g272, 0);
            st->count = 0;
          }
          if (g_nActorAnimDone != 0) {
            ANIM_GO(actor, 0);
            st->timer = 0xA;
            actor->subMode = 2;
            PlayActorMeshVoice(actor, 0, 0);
          }
          f = 7;
          if (actor->type == 0xF1) {
            f = 0xE;
          }
          if (actor->nextFrame >= f && *(p = &g_nSpyroFlameBreathTimer) == 0) {
            if (st->kind == 1 || st->kind == 3) {
              *p = 0x9450C00;
            } else if (st->kind == 2) {
              *p = 0x30C;
            } else {
              *p = 0x384;
            }
            *(q = &g_dwSpyroRequestMask) |= 0x80020000;
            g_bSpyroRequestYaw = st->heading;
          }
          break;
        }

        case 2:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            actor->mode = 2;
            continue;
          }
          break;
        }
        break;
      }

      case 2: {
        int dz;

        g_nGameplayBlocked = 0;
        dz = st->home[2] - actor->posZ;
        if (st->link != -1 && g_pActorListBase[st->link].mode < ACTOR_MODE_DORMANT) {
          TurnActorYawTowardAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        6, 0, 0);
        } else {
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        }
        if (ApproxDist2D(&actor->posX, st->home) > 0x50) {
          MoveActorAlongAngle(actor,
                        ArcTan2(st->home[0] - actor->posX,
                                      st->home[1] - actor->posY, 0),
                        0x64, 0, 0, 0);
        } else {
          actor->posX = st->home[0];
          actor->posY = st->home[1];
        }
        if (ABS2(dz) > 0x28) {
          if (dz < 0) {
            if (dz < -0x32) {
              dz = -0x32;
            }
          } else if (dz > 0 && dz > 0x32) {
            dz = 0x32;
          }
          actor->posZ += dz;
        }
        if (ApproxDist2D(&actor->posX, st->home) < 0x51 && ABS2(dz) < 0x29) {
          actor->subMode = 0;
          actor->mode = 0;
          continue;
        }
        break;
      }

      case 3:
        if (WalkActorAlongWaypoints(actor, st->path, 0x200, 0x96, 0, 6, 0x10, 0) ==
            0x100) {
          actor->subMode = 0;
          CopyVector(st->home, &actor->posX);
          actor->mode = 0;
          continue;
        }
        break;

      case 4: {
        int h = (ArcTan2(actor->posX - st->path->nodes[0].x,
                               actor->posY - st->path->nodes[0].y, 0) +
                 8) &
                0xFF;

        ANIM_GO(actor, 2);
        if (actor->subMode != 0 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) {
          actor->subMode = 0;
          st->alert = 0;
          ANIM_GO(actor, 0);
          actor->mode = 2;
          continue;
        }
        SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0xBB8, h, 8, 0x78, 0xE,
                      0x80, 0xFF, 0xFF, 0, 0, 0);
        break;
      }
      }
      RefreshActorDerivedState(actor, 4);
      GetActorAttachPoint(actor, 0, g320);
      g_pfnLevelEmitParticles(2, 0x42, g320, (void *)2);
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
        CopyVector(g336, &actor->posX);
        g336[3] = GetRandomU32() & 3;
        g336[4] = GetRandomU32() & 3;
        g336[5] = 0x14;
        g360[0] = 0x80;
        g360[1] = 0x70;
        g360[2] = 0x40;
        g_pfnLevelEmitParticles(1, 0x11, g336, g360);
      }
      break;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case 160: {
      L26Critter *st = actor->state;

      if ((actor->flags & 0xB0000) && actor->mode != 1) {
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        {
          if (actor->flags & 0x10000) {
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

    /* Type 203: a fence guard. Keeps to its fence segment, wakes, charges
     * Spyro when he comes close, and breaks into six pieces when beaten. */
#define DZ203(a) ((a)->posZ - (a)->floorOffset - g_nSpyroWorldPosZ)
    case 203: {
      FenceGuardState *st = actor->state;
      int dist;
      int i;

      SubtractVector(g376, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
      if (dist < 0x32C8) {
        dist = VectorLength(g376, 0);
      }
      TickActorFlameGlow(actor);
      if (actor->flags & 0x80000) {
        ArcTan2(actor->posX - g_anSpyroWorldPos, actor->posY - g_nSpyroWorldPosY, 0);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        RefreshActorDerivedState(actor, 4);
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 6; i++) {
          Actor *o;
          FenceGuardPieceState *c;

          int h = *(unsigned char *)&g_bSpyroPersistentEulerZ;

          actor->rotY = h;
          if (i == 2) {
            actor->rotY = h + 0x14;
          } else if (i == 3) {
            actor->rotY = h - 0x14;
          } else if (i == 4) {
            actor->rotY = h + 0x19;
          } else if (i == 5) {
            actor->rotY = h - 0x19;
          }
          o = g_pfnLevelSpawnActor(0xD7, actor);
          c = o->state;
          GetActorAttachPoint(actor, i, &o->posX);
          o->rotZ = actor->rotZ;
          o->rotX = 0;
          o->rotY = 0;
          if (actor->mode == 0) {
            c->lift -= 0x1E;
          }
          if (o->anim != i) {
            ANIM_RESET(o, i);
          }
        }
        DespawnActorRecord(actor);
        continue;
      }
      actor->flags = 0;
      if (actor->subMode == 2) {
        actor->subMode = 0;
        actor->mode = 0;
        ANIM_RESET(actor, 0);
        continue;
      }
      if (actor->mode >= 2 && st->onPath == 1) {
        int dy, dx, len, k, n;

        dy = st->path[7] - st->path[3];
        dx = st->path[2] - st->path[6];
        len = IntegerSqrt(dy * dy + dx * dx);
        g408[0] = st->path[2];
        n = dy * g408[0] + dx * st->path[3];
        k = (dy * g_anSpyroWorldPos + dx * g_nSpyroWorldPosY - n) / len;
        g408[1] = st->path[3];
        g424[0] = st->path[6];
        g424[1] = st->path[7];
        g392[0] = g_anSpyroWorldPos - k * dy / len;
        g392[1] = g_nSpyroWorldPosY - k * dx / len;
        if (g392[0] < g408[0] && g392[0] < g424[0]) {
          if (g408[0] < g424[0]) {
            if (g392[0] < g408[0]) {
              g392[0] = g408[0];
            }
          } else if (g392[0] < g424[0]) {
            g392[0] = g424[0];
          }
        } else if (g392[0] > g408[0] && g392[0] > g424[0]) {
          if (g408[0] > g424[0]) {
            if (g392[0] > g408[0]) {
              g392[0] = g408[0];
            }
          } else if (g392[0] > g424[0]) {
            g392[0] = g424[0];
          }
        }
        if (g392[1] < g408[1] && g392[1] < g424[1]) {
          if (g408[1] < g424[1]) {
            if (g392[1] < g408[1]) {
              g392[1] = g408[1];
            }
          } else if (g392[1] < g424[1]) {
            g392[1] = g424[1];
          }
        } else if (g392[1] > g408[1] && g392[1] > g424[1]) {
          if (g408[1] > g424[1]) {
            if (g392[1] > g408[1]) {
              g392[1] = g408[1];
            }
          } else if (g392[1] > g424[1]) {
            g392[1] = g424[1];
          }
        }
        SubtractVector(g440, g392, &actor->posX);
        len = VectorLength(g440, 0);
        if (len < 0xC8) {
          actor->posX = g392[0];
          actor->posY = g392[1];
        } else {
          ScaleVector3ByRatio(g440, len, 0xC8);
          actor->posX += g440[0];
          actor->posY += g440[1];
        }
      }

      switch (actor->mode) {
      case 0:
        if (actor->subMode == 1 || st->wake != 0) {
          actor->subMode = 0;
          actor->mode = 1;
          ANIM_RESET(actor, 1);
          continue;
        }
        break;

      case 1:
        if (g_nActorAnimDone != 0) {
          actor->mode = 2;
          if (actor->nextAnim != 2) {
            actor->frameProgress = sixteen;
            actor->frameRate = sixteen;
            actor->nextAnim = 2;
            actor->nextFrame = 0;
            StubActorHook(actor);
          }
          continue;
        }
        break;

      case 2:
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        TickCountdownByFrameDelta(&st->timer, 4);
        if (st->onPath == 0 && st->timer < 0x3C && dist < 0x1194 &&
            (st->link == -1 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) &&
            CheckSpyroLineOfSightTo(&actor->posX) != 0) {
          ENTER_POSE(actor, 3);
        }
        break;

      case 3: {
        int range = 0xAF0;

        if (AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                          ArcTan2(actor->posX - g_anSpyroWorldPos,
                                        actor->posY - g_nSpyroWorldPosY, 0)) >= 0x47) {
          range = 0x7D0;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone == 0) {
          if (dist < 0x1771) {
            goto near3;
          }
        } else {
          st->timer = 0xB4;
        }
        ENTER_POSE(actor, 2);
      near3:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && dist < range &&
            DZ203(actor) < 0x320 && actor->nextFrame < 0x1C) {
          actor->frame = 0x1F;
          actor->nextFrame = 0x20;
          actor->frameProgress = 0;
        }
        if (actor->nextFrame == 0x1E) {
          actor->frame = 0xA;
          actor->nextFrame = 0xB;
          actor->frameProgress = 0;
        }
        if (dist < range && DZ203(actor) < 0x320 &&
            (DZ203(actor) > 0 ? DZ203(actor) < 0x320
                               : -DZ203(actor) < 0x320) &&
            actor->nextFrame >= 0x2D && actor->nextFrame < 0x33) {
          SPY.triggerEvents |= (SPYRO_EV_CHARGE_HIT | SPYRO_EV_HIT_PINNED);
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
      break;
    }

    /* Type 204: the fence-guard captain. Wakes the guards (203) whose boxes
     * Spyro walks into, sends them in one at a time, and calls them all back
     * when Spyro reaches the reset box. */
    case 204: {
      FenceCaptainState *st = actor->state;
      int i;

      if (actor->mode != 4) {
        g_pfnLevelEmitParticles(1, 0x41, (int *)actor, 0);
      }
      if ((actor->flags & 0xB0000) && actor->mode != 4) {
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        st->speed = 0xF0;
        if (actor->flags & 0x20000) {
          st->speed = 0x17C;
        }
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, 0),
                                    g_bSpyroPersistentEulerZ, 0x20, 0x40);
        st->zSpeed = 0x28;
        ENTER_POSE(actor, 4);
      }
      if (st->all1.halfWidth == 0 &&
          (g_nSpyroWorldPosZ - st->all1.z > 0 ? g_nSpyroWorldPosZ - st->all1.z < 0x7D0
                                       : st->all1.z - g_nSpyroWorldPosZ < 0x7D0) &&
          IsPointInRotatedBox(&g_anSpyroWorldPos, (int *)&st->all1) != 0) {
        for (i = 6; i >= 0; i--) {
          st->hit[i] = 1;
        }
      }
      if (st->all2.halfWidth == 0 &&
          (g_nSpyroWorldPosZ - st->all2.z > 0 ? g_nSpyroWorldPosZ - st->all2.z < 0x7D0
                                       : st->all2.z - g_nSpyroWorldPosZ < 0x7D0) &&
          IsPointInRotatedBox(&g_anSpyroWorldPos, (int *)&st->all2) != 0) {
        for (i = 6; i >= 0; i--) {
          st->hit[i] = 1;
        }
      }
      TickCountdownByFrameDelta(&st->timer, 4);
      for (i = 0; i < 7; i++) {
        if (st->hit[i] == 0 && st->box[i].halfWidth != 0 &&
            (g_nSpyroWorldPosZ - st->box[i].z > 0
                 ? g_nSpyroWorldPosZ - st->box[i].z < 0x7D0
                 : st->box[i].z - g_nSpyroWorldPosZ < 0x7D0) &&
            IsPointInRotatedBox(&g_anSpyroWorldPos, (int *)&st->box[i]) != 0) {
          st->hit[i] = 1;
        }
      }
      if (st->reset.halfWidth != 0 &&
          IsPointInRotatedBox(&g_anSpyroWorldPos, (int *)&st->reset) != 0) {
        st->cur = 0;
        for (i = 0; i < 7; i++) {
          if (st->link[i] != -1) {
            FenceGuardState *g = g_pActorListBase[st->link[i]].state;

            st->hit[i] = 0;
            g_pActorListBase[st->link[i]].subMode = 2;
            g->timer = g->timer0;
          }
        }
      }

      switch (actor->mode) {
      case 0:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (st->timer == 0 && st->link[st->cur] != -1 &&
            st->hit[st->cur] != 0) {
          st->timer =
              ((FenceGuardState *)g_pActorListBase[st->link[st->cur]].state)->reactDelay;
          st->puff = 0x32;
          ENTER_POSE(actor, 1);
        }
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0xDAC &&
            (DZ203(actor) > 0 ? DZ203(actor) < 0x418
                              : -DZ203(actor) < 0x418)) {
          st->stage = 0;
          ENTER_POSE(actor, 3);
        }
        break;

      case 1:
        RefreshActorDerivedState(actor, 4);
        if (TickCountdownByFrameDelta(&st->puff, 4) == 0) {
          int k;
          int *p;
          Actor *who = actor;

          if (g_dwGamestateFrames & 1) {
            k = 0;
            p = g392;
          } else {
            k = 1;
            p = g408;
          }
          GetActorAttachPoint(who, k, p);
          g_pfnLevelEmitParticles(1, 0x1D, p, actor);
        }
        if ((st->timer == 0 && st->hit[st->cur + 1] != 0) ||
            g_nActorAnimDone != 0) {
          st->cur++;
          ENTER_POSE(actor, 0);
        }
        break;

      case 3:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        if (g_nActorAnimDone != 0) {
          unsigned char *p = st->path;
          int b = p[1];

          do {
            st->path[1] = RandomIntInRange(1, p[0]) - 1;
            p = st->path;
          } while (p[1] == b);
          actor->mode = 5;
          continue;
        }
        if (st->stage <= 0) {
          st->stage = 1;
          g_pfnLevelSpawnActor(0x26, actor);
        } else if (st->stage < 2 && actor->nextFrame >= 2) {
          st->stage = 2;
          g_pfnLevelSpawnActor(0x26, actor);
        } else if (st->stage < 0x3C && actor->nextFrame >= 6) {
          st->stage = 0x3C;
          g_pfnLevelSpawnActor(0x26, actor);
        }
        break;

      case 4:
        DriveActorTrackedMove(actor, &st->speed, st->heading, &st->zSpeed, 0xA, 0x10);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, 4);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case 5: {
        int b = st->path[1];

        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xA0, 0, 8, 0x28, 0xFF, 5) &
            0x100) {
          st->path[1] = b;
          ENTER_POSE(actor, 0);
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, 1);
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
          SubtractVector(g392, at, &spy->posX);
          g392[2] = (g392[2] * 3) >> 2;

          if (VectorLength(g392, 1) < 0x440) {
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
      SubtractVector(g392, &SPY.posX, &actor->posX);
      distance = VectorLength(g392, 0);
      if (distance < st->radius && g392[2] >= -0x3FF && g392[2] < st->spanZ &&
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

        g408[0] = (GetRandomU32() & 0xFE) - 0x7F;
        g408[1] = (GetRandomU32() & 0xFE) - 0x7F;
        g408[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(g408, g408, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, g408, (int *)1);
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
            g456[0] = (GetRandomU32() & 0xFE) - 0x7F;
            g456[1] = (GetRandomU32() & 0xFE) - 0x7F;
            g456[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(g456, g456, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, g456, (int *)1);
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
          g472[0] = g_aSmokeJetEmitters[i >> 1].pos[0];
          g472[1] = g_aSmokeJetEmitters[i >> 1].pos[1];
          g472[2] = g_aSmokeJetEmitters[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, g472, g472);
          AddVector(g472, g472, &actor->posX);
          g472[3] = g_aSmokeJetEmitters[i >> 1].vel[0];
          g472[4] = g_aSmokeJetEmitters[i >> 1].vel[1];
          g472[5] = g_aSmokeJetEmitters[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, g472 + 3, g472 + 3);
          g472[3] += (GetRandomU32() & 0xF) - 8;
          g472[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, 0x4A, g472, g472 + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          g472[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          g472[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          g472[2] = (GetRandomU32() & 0x7F) + 0xC0;
          g472[3] = g472[0] >> 4;
          g472[4] = g472[4] >> 4;
          g472[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, g472, g472);
          AddVector(g472, g472, &actor->posX);
          RotateVectorByMatrix(actor->mtx, g472 + 3, g472 + 3);
          g496[0] = (GetRandomU32() & 7) + 0x10;
          g496[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, 0x10, g472, g496);
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
          ZeroVector(g496);
          g_pfnLevelEmitParticles(1, 0, &st->prize->posX, g496);
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
            CopyVector(g512, &st->prize->posX);
            g512[0] += g_anCosineLut[j * 43] >> 6;
            g512[1] += g_anSineLut[j * 43] >> 6;
            g512[2] += 0x28;
            g528[0] = g_anCosineLut[j * 43] >> 7;
            g528[1] = g_anSineLut[j * 43] >> 7;
            g528[2] = 0x10;
            g_pfnLevelEmitParticles(1, 0, g512, g528);
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

    /* Types 363/364: a one-shot effect that is removed when its animation
     * ends. */
    case 405:
    case 477:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case 390: {
      L26Spinner *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
      int cur = g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0];
      int i;

      if ((actor->flags & 0x90000) == 0x10000) {
        actor->updateDist = 0xFF;
        PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, 8,
                      &actor->sfxSlot);
        if (actor->sfxSlot != 0x7F) {
          g_anSpuVoiceRowPitch[actor->sfxSlot & 0x7F][0] = pitch + st->speed * 4;
        }
      }
      if (st->speed < 0x19) {
        StopSoundVoicesByOwner(actor, 1);
      }
      if (actor->subMode == 0) {
        st->lid = g_pfnLevelSpawnActor(0x188, actor);
        st->lid->cullRadius = 0x1A;
        actor->updateDist = 0x10;
        actor->subMode = 1;
        continue;
      }
      if (st->timer == 0 || --st->timer == 0) {
        if (actor->flags & 0x10000) {
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
      if (st->speed > 0xE0 || (actor->flags & 0x80000)) {
        st->lid->subMode = 1;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 2; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 6; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
        }
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      }
      actor->flags = 0;
      break;
    }

    /* Type 392: homing spark. Launched at the nearest enemy in front of
     * Spyro (or away from him when there is none), and bursts on what it
     * hits. */
    case 392: {
      L26Spark *st = actor->state;
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
          SubtractVector(g496, &a->posX, &actor->posX);
          d = ABS2(g496[0]) + ABS2(g496[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(g496, 0) - 0x1000) <= 0x2000 &&
              (unsigned int)(g496[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(g496[0], g496[1], 1));
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
          ScaleVector3ByRatio(st->vel, VectorLength(st->vel, 0), 0x50);
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
             DispatchActorContactAtSphere(&actor->posX, 0x100, 0, 0x30000, actor, 0) != 0)) {
          DispatchActorContactAtSphere(&actor->posX, 0x400, 0, 0x30000, actor, 0);
          for (i = 0; i < 2; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x136, actor);
            g_pfnLevelSpawnActor(0x137, actor);
          }
          for (i = 0; i < 6; i++) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(0x135, actor);
          }
          g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x18);
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
    case 401: {
      L31Rock *st = actor->state;

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
      if (actor->flags & 0x80000) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);
        for (i = 0; i < 4;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x136, actor);
          i++;
          g_pfnLevelSpawnActor(0x137, actor);
        }
        for (i = 0; i < 0xA;) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x135, actor);
          i++;
        }
        g_pfnLevelEmitParticles(0x20, 0x46, &actor->posX, 0x18);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
      } else {
        st->handle = SelectActorLodBand(actor, st->handle);
        if ((actor->flags & 0x30000) && st->shake == 0) {
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
        g512[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g512[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g512[2] = 0x18;
        g_pfnLevelEmitParticles(1, 0, &actor->posX, g512);
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
          g512[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g512[1] = (g_anSineLut[bearing] * 3) >> 4;
          g512[2] = 0;
          g544[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g544[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g544[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(g560, g544, st->radius - 1);
          LShiftVector3(g544, 1);
          arm = st->radius - 1;
          g560[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          g576[0] = g512[0] * g_anCosineLut[arm * 2];
          g576[1] = g512[1] * g_anCosineLut[arm * 2];
          g576[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(g544, g544, st->phase);
          SubtractVector(g560, g560, g544);
          swing = &g_anCosineLut[bearing];
          actor->posX = g512[0] * *swing;
          actor->posY = g512[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, g576);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g512);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, g560);
          actor->posZ = actor->posZ + ((*swing * 3) >> 3) + 0x600;
        }
      } else {
        unsigned char now = actor->subMode + 8;

        actor->subMode = now;
        actor->rotZ = actor->floorOffset + ((g_anCosineLut[now] * 3) >> 9);
      }

      break;
    }

    /* Types 458/459: tumbling debris. Bounces off walls and floors,
     * falling and spinning until its life runs out. */
    case 458:
    case 459: {
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
        if (st->vel[2] < -128) {
          st->vel[2] = -128;
        }
        actor->posZ += st->vel[2];
        actor->rotX += st->spin0;
        actor->rotY += st->spin1;
        actor->rotZ += st->spin2;
        st->life--;
        continue;
      }
      DespawnActorRecord(actor);
      continue;
    }
}
  }
}
