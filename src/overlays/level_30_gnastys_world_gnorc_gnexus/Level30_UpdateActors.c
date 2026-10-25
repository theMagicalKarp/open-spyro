/* Level30_UpdateActors (0x8007D938, 23,600 B) -- matching notes.
 *
 * The level_30 actor-update megafunction. Load-bearing source forms, beyond
 * the frame layout (15 scratch arrays + acc[3][3], in first-use order) and
 * the loop-top `one`/`two` constant carriers:
 *   - Register pins (`register T x asm("$N")`) where a global.c/local-alloc
 *     tie has no source-order lever: case 310's spark counter i ($16), case
 *     255's push ($7), the two dz sites' SPY.posZ read ($2) and case 398's
 *     svM pointer m ($16). Case 398's `more` needs its own `last` temp.
 *   - Single tail stores after an if/else (the 0xA0000 arm's
 *     `actor->flags = 0;`): reorg re-duplicates the store into the then-arm's
 *     `j` delay slot, which a per-arm copy cannot reproduce (jump.c
 *     cross-jumps it instead). The g_nSpyroRequestVelocityZ clear is the reverse: a `goto`
 *     into the AF4 branch's copy.
 *   - Case 398's head is written in source order and pinned by memory
 *     dependences, not scheduling luck: a do/while(0) after the g_nPortalCamPathReverseFlag
 *     store, g_bSpyroOtBinBias and g_pPortalCamPathData volatile (their stores keep order),
 *     the call's portal re-read through `*(PortalDesc **)st` (not in_struct,
 *     so it depends on the C98 store), and the SPY base held in `cf`.
 *   - Operand order via statement splits: `st->phase += dt` (not a tick
 *     temp), `bearing = arm << 1;` then the subtract, `y + x` of two
 *     separately computed 0xB5 products, `int top = posZ + 0x164` for dz, and
 *     the lean/roll adds through an int temp with (unsigned char) fields.
 *   - GetGroundTriggerRegionType takes no argument; the a0 its caller sees is the delay
 *     slot's `move a0,s4` stolen from the else-branch.
 */

#include "actor.h"
#include "model.h"
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

/* Level30_UpdateActors -- per-frame actor update for level 30 (Gnorc Gnexus).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Layout of this file:
 *   Core records      Actor, the per-type Model table, waypoint paths
 *   Per-class blocks  one state struct per actor class, each followed by the
 *                     globals and callees its case uses, in roughly case order
 *   Shared objects    the camera (CAM) and Spyro (SPY), each declared as one
 *                     aggregate over its whole address range
 *   Helper macros     absolute value and the three animation idioms
 *   The function      update-list preamble, then the per-class dispatch
 */

/* ==== Per-class state blocks ====
 *
 * Every actor class keeps its private state behind actor->state. The blocks
 * below sit next to the globals and callees their case needs.
 */


/* Callees used by the collectible and fairy behaviours. */

/* ==== Helper macros ====
 *
 * Absolute value. The two forms differ only at zero; both occur. */


/* ANIM_ADVANCE promotes the queued animation and queues `next` after it. */
#define ANIM_ADVANCE(a, next)                                                  \
  (a)->frameProgress = 0x10;                                                           \
  (a)->frameRate = 0x10;                                                           \
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
    (a)->frameProgress = 0x10;                                                         \
    (a)->frameRate = 0x10;                                                         \
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


/* ==== level_30 globals ====
 *
 * The hub's own state: which stage of the balloon sequence the world is in,
 * the scripted-camera block at 0x80078C4C, and the three ambience channels. */

/* ==== The function ==== */
void Level30_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  int two;
  char *base;
  int dt;
  int type;

  /* Scratch vectors shared by several cases. */
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
  int svK[3];
  int svL[3];
  int svM[3];
  int svN[3];
  int svO[3];
  int svP[3];
  int svQ[3];

  /* Build this frame's update list. A long frame (delta >= 3) gets an extra
   * pass. */
  BuildActiveActorList();
  TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, 0);

  if (g_nFrameStep >= 3) {
    TickGemCutsceneActorAnims(g_apSaveMenuAnimActorList, g_nFrameStep == 3 ? 0x80000001 : 0x80000000);
  }

  actorList = g_apSaveMenuAnimActorList;
  base = (char *)&g_nSparxHealth;

  /* Visit every actor on the list; dormant ones (state >= 0x80) are skipped. */
  while (actor = *actorList++) {
    one = 1;
    two = 2;
    spy = (SpyroObj *)(base - 0x164);

    if (actor->mode >= ACTOR_MODE_DORMANT) {
      continue;
    }

    g_nActorAnimDone = actor->animFlags & 2;
    g_nActorAnimNewFrame = actor->animFlags & 1;
    dt = g_nFrameStep;
    g_nActorFrameDelta = dt;

    switch (actor->type) {
    /* Portal name text: shown while the camera faces the portal. */
    case 1: {
      int shouldSpawnText;
      int cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == 0;

        actor->mode = one;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ,
            ArcTan2(CAM.posX - actor->posX, CAM.posY - actor->posY, 0));

        if (cameraAngleDiff <= 56) {
          if (actor->subMode == 1) {
            shouldSpawnText = 1;
          }
          actor->subMode = 0;
        } else if (cameraAngleDiff > 71) {
          if (actor->subMode == 0) {
            shouldSpawnText = 1;
          }
          actor->subMode = 1;
        } else {
          actor->mode = 0;
        }

        if (actor->mode && shouldSpawnText) {
          SpawnNameLetterActors(actor, 1);
        }
      } else {
        actor->mode = 0;
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
            if (parent != actor && parent->mode < ACTOR_MODE_DORMANT &&
                parent->type == 13) {
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
        g_pfnLevelEmitParticles(0x10, 0x4D, svE, 0);
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

    /* Explosion shard with a ground bounce (ShardState). */
    case 255:
    case 256: {
      ShardState *st = actor->state;

      if (st->life <= 0 || actor->drawn == 0) {
        g_pfnLevelEmitParticles(8, 0x46, &actor->posX, (int *)0x10);
        DespawnActorRecord(actor);
        continue;
      }

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
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, 1, &actor->posX, svD);
      }

      st->life -= 1;
      break;
    }

    /* Gem-shower shard: same flight, no ground interaction. */
    case 257: {
      ShardState *st = actor->state;

      if (st->life != 0 && actor->drawn != 0 && actor->posZ > st->floor) {
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
      } else {
        DespawnActorRecord(actor);
        continue;
      }

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
          AddVector(g_aRotBillboards[st->attach].pos,
                        g_aRotBillboards[st->attach].pos, &actor->posX);

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

                st->lean = -ArcTan2_8bit(
                    IntegerSqrt((nrm[0] * nrm[0]) + (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
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
            RefreshActorDerivedState(actor, 2);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, 2);
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
                  if (GetSoundVoiceStatusByOwner(g_pSparxActor,
                                    g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0]) ==
                      0) {
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
          dist = VectorLength(svG, 1);

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
              int slope = (signed char)ArcTan2_8bit(svH[2], VectorLength(svH, 0));

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
            PlaySoundEffect(g_pLevelSampleBankHeader[0x2F], actor, 8, &actor->sfxSlot);

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
                  if (GetSoundVoiceStatusByOwner(g_pSparxActor,
                                    g_apActorMeshTable[g_pSparxActor->type]->m_Sounds[0]) ==
                      0) {
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

            if (VectorLength(svI, 1) >= 0x1E1) {
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

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 25) + (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 25) + (unsigned char)st->roll;
              actor->rotY = v;
              st->spin += g_nFrameStep * 2;
            } else if (actor->type == ACTOR_TYPE_SPARX_FOOD) {
              actor->rotZ += 8;
              actor->rotY -= 6;
            } else {
              int v;

              v = ((unsigned short)g_anCosineLut[st->spin] << 16 >> 23) + (unsigned char)st->lean;
              actor->rotX = v;
              v = ((unsigned short)g_anSineLut[st->spin] << 16 >> 23) + (unsigned char)st->roll;
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

    /* Lift plate (PlateState): rises once its trigger actor is gone, waits,
     * then drops back. */
    case 110: {
      PlateState *st = actor->state;
      Actor *trigger = &g_pActorListBase[st->trigger];
      Actor *rider = &g_pActorListBase[st->rider];

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
              if (st->locked == 0 || rider->mode == 1) {
                int *at = &actor->posX;

                st->locked = 0;
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
          st->step = 0x1E;
        }
        break;

      case 4: {
        int height;
        int drop;
        int close;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 || (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
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

        st->step -= 1;
        if (st->step == 0) {
          int reach = (GetRandomU32() & 0x3F) + 0x140;
          int bearing = ArcTan2(spy->posX - rider->posX,
                                      spy->posY - rider->posY, 0);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          EBB_BREAK();
          CopyVector(st->from, &rider->posX);
          aim &= 0xFF;
          aim *= 0x10;
          actor->mode = 5;
          st->from[0] -= (LookupCosine(aim) * reach) >> 12;
          st->from[1] -= (LookupSine(aim) * reach) >> 12;
          st->yaw = ArcTan2(spy->posX - st->from[0],
                                  spy->posY - st->from[1], 0);
          st->step = 0;
        }
        break;
      }

      case 5: {
        int height;
        int drop;

        st->timer += 1;
        actor->posZ = st->baseZ + (LookupCosine(st->timer << 7) >> 6);

        if ((rider->mode == 0 || (rider->mode == 2 && rider->subMode >= 0x10)) &&
            ApproxDist2D(&rider->posX, &spy->posX) < 0x400 &&
            (height = rider->posZ - rider->floorOffset, drop = height - SPY.posZ,
             drop > 0 ? drop < 0x200 : (SPY.posZ - height) < 0x200) &&
            (g_nSpyroState == SPYRO_ST_IDLE || g_nSpyroState == SPYRO_ST_TEETER) && g_nSpyroStateTimer > 0) {
          actor->mode = 6;
          st->locked = 1;
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

        st->step += 1;
        SubtractVector(svF, st->from, here);
        turn = SignedAngleDelta8(st->yaw, actor->rotZ);
        w0 = LookupCosine((st->step - 1) << 7);
        wSpan = w0 - LookupCosine(st->step << 7);
        wDen = LookupCosine((st->step - 1) << 7) + 0x1000;
        actor->posX += (svF[0] * wSpan) / wDen;
        actor->posY += (svF[1] * wSpan) / wDen;
        actor->rotZ += (turn * wSpan) / wDen;
        FindGroundHeightBelow(here, 0x1000);
        EncodeCachedVecToActorDirCode(actor);

        if (st->step == 0x10) {
          actor->mode = 4;
          st->step = (GetRandomU32() & 0x1F) + 0x1E;
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

      if (*(int *)base >= 2) {
        svF[0] = 0;
        svF[1] = 0x64;
        svF[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
        svF[0] = 0;
        svF[1] = -0x64;
        svF[2] = 0;
        ApplyActiveGteRotation(svF, svF);
        AddVector(svF, svF, &actor->posX);
        g_pfnLevelEmitParticles(2, 0x42, svF, 0);
      }

      if (SPY.sparxHealth >= 3) {
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

      if (SPY.sparxHealth <= 0) {
        if (st->target != 0 && st->target->type == ACTOR_TYPE_BUTTERFLY) {
          st->target->mode = 0;
        }
        g_pSparxActor = 0;
        DespawnActorRecord(actor);
        continue;
      }

      if (st->target != 0 && ApproxDist2D(&actor->posX, (int *)(base - 0x164)) >= 0x2001) {
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
              svA[0] -= (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              svA[1] -= (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              svA[0] -= (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              svA[1] -= (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 22;
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
            dist = VectorLength(svA, 1);

            if (high == 0 &&
                (TickCountdownByFrameDelta((void *)st, 4) != 0 ||
                 (st->timer < 0x50 &&
                  AbsAngleDelta8((actor->rotZ + (pk->vel[0] * 0x6E)) & 0xFF,
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
            actor->rotY = ArcTan2(VectorLength(svA, 0), svA[2], 0);
            actor->rotY = StepAngle8Toward(actor->rotY & 0xFF, 0, 0x30);
            break;
          }

          case 1:
            g_nSparxHealth += 1;
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

            SubtractVector(svJ, &st->target->posX, &actor->posX);
            dist = VectorLength(svJ, 1);

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
                ScaleVector3ByRatio(svJ, dist, lim);
              } else {
                ScaleVector3ByRatio(svJ, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, svJ);
              TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(svJ, 0), svJ[2], 0);
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

        RefreshActorDerivedState(actor, 4);
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
          AddVector(g_aRotBillboards[st->attach].pos,
                        g_aRotBillboards[st->attach].pos, &actor->posX);
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

    /* Boss-door trigger: opens the gnexus door when Spyro walks up facing it.
     */
    case 192:
      if (actor->mode == 0) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000) {
          break;
        }
        actor->mode = 1;
        continue;
      }

      if (((unsigned int)g_nSpyroState < 2 || g_nSpyroState == SPYRO_ST_JOG ||
           g_nSpyroState == SPYRO_ST_RUN || g_nSpyroState == SPYRO_ST_CHARGE) &&
          ApproxDist2D(&actor->posX, &spy->posX) < 0x780 &&
          AbsAngleDelta8(g_bSpyroPersistentEulerZ,
                        ArcTan2(actor->posX - spy->posX,
                                      actor->posY - SPY.posY, 0)) < 0x20 &&
          AbsAngleDelta8(actor->rotZ,
                        ArcTan2(spy->posX - actor->posX,
                                      g_nSpyroWorldPosY - actor->posY, 0)) < 0x28) {
        actor->mode = 0;
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ResetSpyroLinearMotion();
        ZeroVector(&spy->headLook);
        g_pfnLevelBeginActorCutscene(actor);
        g_nGamestate0cDialogPage = 0x1E;
        g_bWorldBalloonistState1 = two;
        g_abWorldBalloonistState[2] = two;
        g_abWorldBalloonistState[3] = 2;
        g_abWorldBalloonistState[4] = 2;
        g_abWorldBalloonistState[5] = 2;
      }
      break;

    /* Breakable chest (ChestState): one hit showers gems. */
    case 194: {
      ChestState *st = actor->state;

      if (st->shake != 0) {
        SnapActorToGround(actor);
      }

      if (actor->flags & 0xB0000) {
        int i;

        actor->soundRange = 0x20;
        PlayActorMeshVoice(actor, 0, 0);

        for (i = 0; i < 2; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0xFF, actor);
          g_pfnLevelSpawnActor(0x100, actor);
        }

        for (i = 0; i < 6; i++) {
          if ((g_nActorPoolCapacity - g_nActorPoolLiveCount) < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(0x101, actor);
        }

        g_pfnLevelEmitParticles(5, 2, &actor->posX, 0);
        g_pfnLevelEmitParticles(0x10, 0x46, &actor->posX, (int *)0x20);
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        DespawnActorRecord(actor);
        continue;
      }

      break;
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

    /* Hub ambience (MusicState): starts the three voice channels and keeps
     * them alive. */
    case 208: {
      MusicState *st = actor->state;

      if (st->started == 0) {
        if (g_bAmbienceChannelMask != 0) {
          ModifyWorldChunkAnimFlags(0, 0xFC, 0);
          if (g_nSpyroState == SPYRO_ST_GLIDE && g_nSpyroStateFlags == 9) {
            SetWorldChunkAnimStep(0, 0x3B, 0);
          }
        }

        if (g_abLevelVisitedFlag[0x21] != 0) {
          ModifyWorldChunkAnimFlags(1, 0xFC, 0);
          if (g_nSpyroState == SPYRO_ST_GLIDE && g_nSpyroStateFlags == 9) {
            SetWorldChunkAnimStep(1, 0x3B, 0);
          }
        }

        if (g_abLevelVisitedFlag[0x22] != 0) {
          ModifyWorldChunkAnimFlags(2, 0xFC, 0);
          if (g_nSpyroState == SPYRO_ST_GLIDE && g_nSpyroStateFlags == 9) {
            SetWorldChunkAnimStep(2, 0x3B, 0);
          }
        }

        st->started = 1;
      }

      if (!((GetWorldChunkAnimState(0) >> 8) & 0xFF) && g_abLevelExited[0x1F] != 0 &&
          g_anLevelDragonsRescued[0x1E] > 0) {
        ModifyWorldChunkAnimFlags(0, 0xFC, 0);
      }

      if (!((GetWorldChunkAnimState(1) >> 8) & 0xFF) && g_abLevelExited[0x20] != 0) {
        ModifyWorldChunkAnimFlags(1, 0xFC, 0);
      }

      if (!((GetWorldChunkAnimState(2) >> 8) & 0xFF) && g_nWorldTreasureCollected >= 0x2EE0 &&
          g_nWorldDragonsRescued >= 0x50 && g_nWorldEggsRecovered >= 0xC) {
        ModifyWorldChunkAnimFlags(2, 0xFC, 0);
      }

      break;
    }

    /* Charging gnorc (ChargeState): winds up when hit, dashes, recovers. */
    case 236: {
      ChargeState *st;

      EBB_BREAK();
      st = actor->state;
      if ((actor->flags & 0xB0000) && actor->mode != 1) {
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, 0),
            spy->bodyRotZ, 0x20, 0x40);
        if (actor->flags & 0x10000) {
          st->speed = 0xC8;
        } else {
          st->speed = 0x190;
        }
        actor->flags = 0;
        SpawnActorDeathDrop(actor, 3, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = 1;

        if (actor->nextAnim != 1) {
          ANIM_ADVANCE(actor, 1);
        }
      } else {
        switch (actor->mode) {
        case 0:
          UpdateActorWander(actor, st);
          if (TickCountdownByFrameDelta(&st->rest, 4) != 0) {
            actor->mode = 2;
            if (actor->nextAnim != 2) {
              ANIM_ADVANCE(actor, 2);
            }
          } else {
            RefreshActorDerivedState(actor, 1);
          }
          break;

        case 1:
          if (st->speed >= 0x10) {
            st->speed -= 0xF;
            MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0x1F4, 5);
          }
          if (g_nActorAnimDone != 0) {
            RefreshActorDerivedState(actor, 4);
            BurstActorWithSound(actor, 0x10);
            DespawnActorRecord(actor);
            continue;
          }
          RefreshActorDerivedState(actor, 1);
          break;

        case 2:
          if (g_nActorAnimDone != 0) {
            st->rest = RandomIntInRange(0x5A, 0x12C);
            actor->mode = 0;
            if (actor->nextAnim != 0) {
              ANIM_ADVANCE(actor, 0);
            }
          } else {
            RefreshActorDerivedState(actor, 1);
          }
          break;

        default:
          RefreshActorDerivedState(actor, 1);
          break;
        }
      }

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
          SubtractVector(svK, at, &spy->posX);
          svK[2] = (svK[2] * 3) >> 2;

          if (VectorLength(svK, 1) < 0x440) {
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
              g_anSpyroRequestVelocity.y = g_nSpyroMotionY >> 8;
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

              if (SPY.requestVelX != 0 || g_anSpyroRequestVelocity.y != 0) {
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

    /* Gem: its own pickup update lives in the shared helper. */
    case ACTOR_TYPE_STATUE_SHARD:
      TickStatueBurstShard(actor);
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
          g_pfnLevelEmitParticles(0xA, 0x47, &actor->posX, 0);
          DespawnActorRecord(actor);
          continue;
        }
      }

      break;
    }

    /* Waypoint rider (RideState30): steps a closed path and calls out a voice
     * line per leg. */
    case 286: {
      RideState30 *st = actor->state;
      int voice = -1;

      actor->soundRange = st->volume;

      switch (st->pose) {
      case 0:
        voice = 6;
        break;

      case 1:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->leg) {
        case 0:
          if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
            st->wait = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->leg = 1;
          }
          break;

        case 1: {
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
            st->leg = 0;
            st->path->cur = (st->path->cur + 1) % st->path->count;
            st->left = ComputeStepTowardPoint(
                &st->path->nodes[st->path->cur].x,
                &st->path->nodes[(st->path->cur + 1) % st->path->count].x,
                st->step, st->speed);
          }

          if (actor->sfxSlot == 0x7F && st->left == (st->left / 48) * 48) {
            voice = 0xE;
          }
          break;
        }
        }
        break;

      case 3:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          st->wait = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->wait, 4) != 0) {
          int r;

          st->wait = 0x258 - (GetRandomU32() & 0x7F);
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

      case 8:
        voice = 0x1A;
        break;

      case 9:
        voice = 0x36;
        break;
      }

      if (voice >= 0) {
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, 8, &actor->sfxSlot);
      }

      break;
    }

    /* Sparkling shard: one sparx per frame. */
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

        svK[0] = (GetRandomU32() & 0xFE) - 0x7F;
        svK[1] = (GetRandomU32() & 0xFE) - 0x7F;
        svK[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(svK, svK, &actor->posX);
        g_pfnLevelEmitParticles(1, 0x42, svK, (int *)1);
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
            svL[0] = (GetRandomU32() & 0xFE) - 0x7F;
            svL[1] = (GetRandomU32() & 0xFE) - 0x7F;
            svL[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(svL, svL, &actor->posX);
            g_pfnLevelEmitParticles(1, 0x42, svL, (int *)1);
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

    /* Whirlwind pad: a short spin-up before the ride, then a cooldown. */
    case 331:
      switch (actor->mode) {
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
        int tick = actor->subMode + g_nFrameStep;

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
    case 405:
    case 477:
      if (g_nActorAnimDone == 0) {
        break;
      }
      DespawnActorRecord(actor);
      continue;

    /* Level-exit vortex (ExitState): drives the scripted camera along the
     * portal path and hands over to the world map. */
    case 398: {
      ExitState *st = actor->state;

      switch (actor->mode) {
      case 1: {
        register int *m asm("$16") = svM;
        int *cf = &SPY.requestMask;
        PortalDesc *portal;
        int *aim;
        int span;

        *cf = 0x80006147;
        g_nPortalCamPathReverseFlag = actor->subMode;
        EBB_BREAK();
        portal = st->portal;
        g_nPortalCamPathMaxStep = 0x60;
        g_nSpyroRequestState = SPYRO_ST_JUMP;
        g_bSpyroOtBinBias = 0x7F;
        g_pPortalCamPathData = portal;
        {
          PortalDesc *pd = *(PortalDesc **)st;

          AddVector(m, pd->posA, pd->posB);
        }
        RShiftVector3(m, 1);
        aim = cf + 5;
        SubtractVector(aim, m, (int *)((char *)cf - 0x1F4));
        AddVector(aim, aim, st->centre);
        span = VectorLength(aim, 1);

        if (span >= 0x81) {
          ScaleVector3ByRatio(aim, span, 0x80);
        } else {
          int more;

          if (actor->subMode != 0) {
            int last = st->portal->count - 1;

            more = last > st->portal->index;
          } else {
            more = st->portal->index;
          }
          if (more != 0) {
            actor->mode = 2;
          }
        }

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && *g_pTriggerRegionTable[g_nGroundSurfaceType] == 6) {
          actor->mode = 0;
          AS_I32(g_nCameraNextMode) = 0x80000012;
          SPY.requestState = SPYRO_ST_GLIDE;
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nGamePaused = 1;
          g_bSpyroRequestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);
          {
            int e = g_nCameraYaw;
            int yaw = g_nSpyroBodyYaw;

            g_nCameraYaw = e + yaw;
            *(Xform *)&g_nCameraSpringYaw = *(Xform *)&g_nCameraYaw;
            g_nSpyroBodyYaw = yaw + g_nPortalViewYawSpin;
          }
          g_bSpyroPersistentEulerZ = g_nSpyroBodyYaw >> 4;
          g_nCameraSpringYaw -= g_bSpyroPersistentEulerZ * 0x10;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nPortalViewPitchTilt;
          if (st->preset >= 0) {
            g_pCameraTargetParams = &g_aCameraXformPresets[st->preset];
          } else {
            g_pCameraTargetParams = 0;
          }
        } else {
          SPY.requestMask |= SPYRO_RQ_PORTAL_PATH_CAMERA;
        }
        break;
      }

      case 2:
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FORCE_STATE | SPYRO_RQ_FREEZE_INPUT | SPYRO_RQ_NO_CLIP);
        g_nPortalCamPathMaxStep = 0x60;
        g_nSpyroRequestState = SPYRO_ST_GLIDE;
        g_bSpyroOtBinBias = 0x7F;
        g_pPortalCamPathData = st->portal;

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && *g_pTriggerRegionTable[g_nGroundSurfaceType] == 6) {
          actor->mode = 0;
          AS_I32(g_nCameraNextMode) = 0x80000012;
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nGamePaused = 1;
          g_bSpyroRequestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);
          {
            int e = g_nCameraYaw;
            int yaw = g_nSpyroBodyYaw;

            g_nCameraYaw = e + yaw;
            *(Xform *)&g_nCameraSpringYaw = *(Xform *)&g_nCameraYaw;
            g_nSpyroBodyYaw = yaw + g_nPortalViewYawSpin;
          }
          g_bSpyroPersistentEulerZ = g_nSpyroBodyYaw >> 4;
          g_nCameraSpringYaw -= g_bSpyroPersistentEulerZ * 0x10;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nPortalViewPitchTilt;
          if (st->preset >= 0) {
            g_pCameraTargetParams = &g_aCameraXformPresets[st->preset];
          } else {
            g_pCameraTargetParams = 0;
          }
        } else {
          SPY.requestMask |= SPYRO_RQ_PORTAL_PATH_CAMERA;
        }
        break;
      }

      break;
    }

    /* Bobbing platform (BobState). */
    case 416: {
      BobState *st = actor->state;

      if (actor->mode == 0) {
        st->restZ = actor->posZ;
        actor->mode = 1;
      } else {
        actor->posZ =
            st->restZ + ((unsigned short)g_anSineLut[actor->subMode] << 16 >> 20);
        actor->subMode += g_nFrameStep;
      }

      break;
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
          svN[0] = (g_anCosineLut[bearing] * 3) >> 4;
          svN[1] = (g_anSineLut[bearing] * 3) >> 4;
          svN[2] = 0;
          svO[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svO[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          svO[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(svP, svO, st->radius - 1);
          LShiftVector3(svO, 1);
          arm = st->radius - 1;
          svP[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          svQ[0] = svN[0] * g_anCosineLut[arm * 2];
          svQ[1] = svN[1] * g_anCosineLut[arm * 2];
          svQ[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(svO, svO, st->phase);
          SubtractVector(svP, svP, svO);
          swing = &g_anCosineLut[bearing];
          actor->posX = svN[0] * *swing;
          actor->posY = svN[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, svQ);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, svN);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, svP);
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
