/* Level24_UpdateActors -- per-frame actor update for level 24 (Dream
 * Weavers home). 0x8007E3C0, 43,620 bytes.
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class. The
 * level-24 classes are the cannon (91) and its comets (104), the guards
 * (92/130) and the talking NPC (191); the rest are shared with the other
 * level heads.
 *
 * Load-bearing source forms:
 *   - No loop-top carriers: every hoisted constant spills, the preheader is
 *     empty and jump2 turns the loop entry into a `j` to the bottom test.
 *   - Case 91 state 10: the comet's state is written timer, steps, count,
 *     dvz, speed, spark. The steps read must precede every cs store after
 *     the timer (it aliases them), and the count store ahead of dvz keeps
 *     the speed constant out of the slot the call's a2 = 800 needs, so the
 *     second rotZ index stays in a2.
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


/* ==== Level 22 classes ==== */


/* ==== Level 28 classes ==== */


/* ==== Level 24 classes ==== */

extern CometTrailPoint *g_apMotionTrailSlices[]; /* trails */


/* ==== The function ==== */
#define spy (&SPY)
#define one 1
void Level24_UpdateActors(void) {
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
  int g184[2];
  int g192[3];
  volatile int g208v;
  int g216[3];
  int g232[3][3];
  int g272[3];
  int g288[3];
  int g304[3];
  int g320[3];
  int g336[3];
  int g352[3];
  int g368[3];
  int g384[6];
  int g408[3];
  int g424[3];
  int g440[3];
  int g456[6];
  int g480[3];
  int g496[3];
  int g512[3];
  int g528[3];
  int g544[3];
  int g560[3];

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
    /* Portal name text: shown while the camera faces the portal. */
    case ACTOR_TYPE_PORTAL_NAME: {
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
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->sfxSlot);
            DespawnActorRecord(actor);
            continue;
          }
        } else if (actor->subMode != 0 || parent->mode >= ACTOR_MODE_DORMANT) {
          st->release = 9;
          st->count = 1;

          for (parent = g_pActorListBase; parent < actor; parent++) {
            if (parent->mode < ACTOR_MODE_DORMANT && parent->type == ACTOR_TYPE_FOLLOWER &&
                *(int *)parent->state == st->parent) {
              st->release += 9;
              st->count += 1;
            }
          }

          if (st->count == 1) {
            g_nSfxOverrideFlags = 2;
            g_nSfxOverridePitch.v =
                SAMPLE_PITCH(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE]);
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_SEQUENCE_TONE], actor, SFX_MODE_3D, &actor->sfxSlot);
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
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)0x602080);
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
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, g120, 0);
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

    /* Comet spark: replays its animation subMode times, then shares the
     * one-shot effects' exit. */
    case ACTOR_TYPE_COMET_SPARK:      if (actor->subMode != 0) {
        actor->subMode--;
        actor->frame = 0;
        actor->nextFrame = 1;
        actor->frameProgress = 0;
      }

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 91: a cannon. Watches for Spyro to stand in its sights and
     * blasts him, or lobs a comet (104) at whatever its trigger names. */
    case ACTOR_TYPE_COMET_CANNON: {
      CometCannonState *st = actor->state;
      Actor *p = &g_pActorListBase[st->link];

      RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
      if (actor->nextAnim >= 3 && actor->nextFrame >= 0xE) {
        ANIM_GO(actor, 0);
      }
      if (st->init == 0) {
        st->init = one;
        st->heading = actor->rotZ << 4;
      }
      if (st->camFlag != 0) {
        g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_LOCK_ON);
        g_pScriptedCameraActor = actor;
      }

      switch (actor->mode) {
      case 0:
        if (actor->subMode != 0 && p->mode < 0x7F) {
          st->target = actor->subMode;
          actor->subMode = 0;
          st->idx = (st->idx + 1) % 3;
          actor->mode = 10;
          continue;
        }
        if (p->mode >= ACTOR_MODE_DORMANT) {
          int range = 0x4E2;
          int a = 0x20;
          int b = 0x50;

          if (st->camFlag != 0) {
            range = 0x578;
            a = 0x2C;
            b = 0x41;
          }
          TickCountdownByFrameDelta(&st->timer, 4);
          if (actor->flags & ACTOR_HIT_FLAME) {
            int h;

            actor->flags = 0;
            g104[0] = -0x3E8;
            g104[1] = 0;
            g104[2] = 0x172;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
            RotateVectorByMatrix(actor->mtx, g104, g104);
            AddVector(g104, g104, &actor->posX);
            h = ArcTan2(g104[0] - g_anSpyroWorldPos, g104[1] - g_nSpyroWorldPosY, 0);
            if (st->timer == 0 && AbsAngleDelta8(h, g_bSpyroPersistentEulerZ) < 0x30) {
              actor->mode = 1;
              continue;
            }
          }
          st->camFlag = 0;
          if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < range) {
            int d1 = WrapAngleDelta8(actor->rotZ, *(unsigned char *)&g_bSpyroPersistentEulerZ);
            int ad = ABS(d1);

            if (0x40 - a < ad && ad < a + 0x40) {
              int d2 = WrapAngleDelta8(ArcTan2(g_anSpyroWorldPos - actor->posX,
                                                   g_nSpyroWorldPosY - actor->posY, 0),
                                     actor->rotZ);

              if (b < ABS(d2) &&
                  ((d1 < 0 && d2 < 0) || (d1 > 0 && d2 > 0))) {
                st->camFlag = one;
              }
            }
          }
        } else {
          st->heading = actor->rotZ << 4;
        }
        actor->flags = 0;
        break;

      case 1:
        st->timer = 0x4B;
        actor->mode = 3;
        continue;

      case 2:
        if (st->timer >= 0xB) {
          int i;

          actor->flags = 0;
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 3, g056);
          g136[0] = -0x10;
          g136[1] = 0;
          g136[2] = -0xE;
          RotateVectorByMatrix(actor->mtx, g136, g136);
          i = 0;
          do {
            i++;
            g136[0] += (GetRandomU32() & 0xF) - 8;
            g136[1] += (GetRandomU32() & 0xF) - 8;
            g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, g056, (void *)g136);
          } while (i < 5);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          StopSoundVoicesByOwner(actor, VOICE_STOP_LOOPING);
          actor->mode = 3;
          continue;
        }
        break;

      case 3: {
        Actor *o;
        CometState *cs;
        Actor *best;
        Actor *q;
        int bestScore;

        PlayActorMeshVoice(actor, 1, 0);
        st->idx = (st->idx + 1) % 3;
        o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET, actor);
        cs = o->state;
        CopyVector(&o->posX, &actor->posX);
        o->rotZ = actor->rotZ;
        MoveActorAlongHeading(o, 0x258, 0, 0, 0);
        o->posZ += 0x352;
        cs->speed = 0x320;
        cs->dvz = -6;
        cs->vz = 0x96;
        cs->timer = 0x8C;
        cs->steps = 0;
        cs->count = 0;
        cs->spark = 0;
        cs->life = 0;
        cs->lifeOn = 0;
        best = 0;
        cs->trail = (CometTrailPoint *)((char *)st + (st->idx * 0xFDC + 0x28));
        o->cullRadius = 0;
        o->updateDist = 0xFF;
        bestScore = 0xA410;
        actor->flags = 0;
        for (q = g_pActorListBase; q < g_pActorPoolDynBase; q++) {
          int ang;
          int d;

          if (q == actor || q->mode >= ACTOR_MODE_DORMANT || q->type == ACTOR_TYPE_DRAGON_STATUE ||
              q->type == ACTOR_TYPE_SAVE_HOST || q->collision == 0 || q->collision >= 0) {
            continue;
          }
          SubtractVector(g152, &q->posX, &actor->posX);
          d = ABS(g152[0]) + ABS(g152[1]);
          if (d > 0x10FFF) {
            continue;
          }
          ang = (ArcTan2(q->posX - actor->posX, q->posY - actor->posY,
                               0) -
                 actor->rotZ) &
                0xFF;
          if (ang > 0x80) {
            ang -= 0x100;
          }
          ang = ABS(ang);
          if (ang >= 7) {
            continue;
          }
          if (d <= 0xBFFF) {
            d = VectorLength(g152, VEC_LEN_XY);
          } else {
            d = ApproxDist2D(&q->posX, &actor->posX);
          }
          if (d > 0xAFFF) {
            continue;
          }
          if (d > 0x5800) {
            d -= 0x5800;
          } else if (d < 0x4000) {
            d = 0x4000 - d;
          } else {
            d = 0;
          }
          d += ang << 12;
          if (d < bestScore) {
            best = q;
            bestScore = d;
          }
        }
        if (best != 0) {
          CopyVector(g168, &best->posX);
          if (best->type == ACTOR_TYPE_LUNGING_GUARD || best->type == ACTOR_TYPE_BIG_LUNGING_GUARD) {
            g168[2] += 0x320;
          }
          o->rotZ = ArcTan2(best->posX - actor->posX,
                                   best->posY - actor->posY, 0);
          cs->dvz = -8;
          cs->speed = 0x258;
          {
            int t = ComputeLobVelocityZ(&o->posX, g168, 0x258, cs->dvz, g184);

            cs->life = g184[0];
            cs->vz = t;
          }
        }
        ANIM_GO(actor, 3);
        actor->mode = 0;
        continue;
      }

      case 10:
        if (actor->nextAnim == 3) {
          g_nActorAnimDone = 0;
          actor->anim = 3;
          actor->frameProgress = 0;
          actor->frame = 0;
          actor->nextFrame = 1;
        }
        ANIM_GO(actor, 3);
        if (actor->drawn == 0 ||
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->target].posX - actor->posX,
                                        g_pActorListBase[st->target].posY - actor->posY,
                                        0),
                          6, 0x14, one) != 0) {
          Actor *o;
          CometState *cs;
          int v;
          int i;

          CopyVector(g192, &g_pActorListBase[st->target].posX);
          o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET, actor);
          o->rotZ = ArcTan2(g192[0] - o->posX, g192[1] - o->posY, 0);
          CopyVector(&o->posX, &actor->posX);
          g192[2] += 0x320;
          cs = o->state;
          o->posX += (g_anCosineLut[o->rotZ] * 0x7D) >> 8;
          o->posY += (g_anSineLut[o->rotZ] * 0x7D) >> 8;
          o->posZ += 0x578;
          o->cullRadius = 0;
          o->updateDist = 0xFF;
          cs->timer = 0x78;
          cs->steps = st->steps;
          cs->count = 0;
          cs->dvz = -7;
          cs->speed = 0x320;
          cs->spark = 0;
          cs->vz = ComputeLobVelocityZ(&o->posX, g192, 0x320, cs->dvz, (int *)&g208v);
          cs->trail = (CometTrailPoint *)((char *)st + (st->idx * 0xE38 + 0x28));
          cs->lifeOn = one;
          v = cs->vz;
          cs->life = g208v;
          CopyVector(g216, &o->posX);
          for (i = 0; i < *(int *)&g208v; i++) {
            g216[0] += (cs->speed * g_anCosineLut[o->rotZ]) >> 12;
            g216[1] += (cs->speed * g_anSineLut[o->rotZ]) >> 12;
            g216[2] += v;
            v += cs->dvz;
          }
          SubtractVector(g216, g192, g216);
          AddVector(&o->posX, &o->posX, g216);
          actor->mode = 0;
          continue;
        }
        continue;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 92: a guard. Wakes its partner when Spyro comes into range,
     * paces its path, lunges, and spins away when struck. */
    case ACTOR_TYPE_LUNGING_GUARD: {
      LungingGuardState *st = actor->state;
      int dist;

      SubtractVector(g104, &spy->posX, &actor->posX);
      dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
      if (dist < 0x32C8) {
        dist = VectorLength(g104, VEC_LEN_XY);
      }
      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 3 &&
          (st->kind != 1 || !(actor->flags & ACTOR_HIT_CHARGE))) {
        st->lift = 0;
        st->knock = 0xC8;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knock = 0x15E;
        }
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, 0),
                                    g_bSpyroPersistentEulerZ, 0x20, 0x40);
        actor->flags = 0;
        st->spin = RandomSignedInRange(8, 0xE);
        actor->subMode = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ANIM_GO(actor, st->base + 3);
        actor->mode = 3;
        continue;
      }
      if (st->init == 0) {
        st->init = one;
        if (st->kind == 0) {
          st->base = 4;
          ANIM_SET(actor, st->base);
        }
      }
      if (actor->mode < 3 && (st->range != 0 || (actor->flags & ACTOR_HIT_BLAST))) {
        if (st->range > 0 && actor->mode == 0 &&
            (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range ||
             IsPointInRotatedBox((int *)spy, st->vol) != 0) &&
            TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->range = -st->range;
          if (st->range == 0) {
            st->range = -1;
          }
          g_pActorListBase[st->partner].subMode = actor - g_pActorListBase;
          *(int *)g_pActorListBase[st->partner].state = st->msg;
          st->timer2 = st->reload;
        } else if (actor->flags & ACTOR_HIT_BLAST) {
          actor->flags = 0;
          if (st->flags & 4) {
            st->range = -st->range;
          } else {
            st->range = 0;
          }
          if (st->kind != 0) {
            actor->scale = 0x20;
            actor->mode = 0x14;
            continue;
          }
          BurstActorWithSound(actor, 0x28);
          actor->scale = 0x40;
          st->base = 0;
          ANIM_SET(actor, st->base);
          actor->mode = 10;
          continue;
        } else if (st->range < 0) {
          TickCountdownByFrameDelta(&st->timer2, 4);
        }
      }
      actor->flags = 0;
      if (st->flags == 1) {
        st->flags = 0;
        actor->mode = 0x1E;
        continue;
      }

      switch (actor->mode) {
      case 0:
        if ((st->flags & 2) &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range2) {
          ANIM_GO(actor, st->base + 1);
          actor->mode = 1;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x190
                               : -SPY_DZ(actor) < 0x190)) {
          if (st->kind == 0) {
            if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x5DC) {
              actor->mode = 2;
              continue;
            }
          } else if (st->kind == 1 &&
                     ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1F40) {
            actor->mode = 2;
            continue;
          }
        }
        break;

      case 1:
        st->flags &= ~2;
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0xC00 ||
            PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, one) ==
                0x100) {
          ANIM_GO(actor, st->base);
          actor->mode = 0;
          continue;
        }
        break;

      case 2:
        if (st->kind == 0) {
          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          g_anSpyroRequestVelocity.x = (g_anCosineLut[actor->rotZ] * 0x23) >> 10;
          g_nSpyroRequestVelocityY = (g_anSineLut[actor->rotZ] * 0x23) >> 10;
          g_nSpyroRequestVelocityZ = 0x46;
          if (g_nActorAnimDone != 0) {
            st->timer = 0x78;
            ANIM_GO(actor, st->base);
            actor->mode = 0;
            continue;
          }
          break;
        }
        ANIM_GO(actor, st->base + 2);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (g_nActorAnimDone != 0 || dist > 0x2328) {
          st->timer = 0x96;
          ANIM_GO(actor, st->base);
          actor->mode = 0;
          continue;
        }
        if (dist < 0x9C4 && actor->anim == actor->nextAnim &&
            actor->nextFrame < 0x12) {
          actor->frame = 0x15;
          actor->nextFrame = 0x16;
          actor->frameProgress = 0;
        }
        if (actor->nextFrame == 0x15) {
          actor->frame = 7;
          actor->nextFrame = 8;
          actor->frameProgress = 0;
        }
        break;

      case 3: {
        int r;

        actor->rotZ += st->spin;
        if (++actor->subMode >= 3) {
          if (st->spin > 0) {
            st->spin--;
            if (st->spin < 0) {
              st->spin = 0;
            }
          } else if (st->spin < 0) {
            st->spin++;
            if (st->spin > 0) {
              st->spin = 0;
            }
          }
        }
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        r = DriveActorTrackedMove(actor, &st->knock, st->heading, &st->lift, 0xC, 0x10);
        if (st->kind == 0) {
          if (r == 3 && (unsigned)(actor->nextFrame - 0xB) < 9) {
            g_nActorAnimDone = 0;
            actor->frameProgress = 0x20;
            actor->frameRate = 0x20;
            actor->anim = actor->nextAnim;
            actor->nextAnim = actor->nextAnim;
            actor->frame = actor->nextFrame;
            actor->nextFrame = 0x14;
            OnActorAnimQueued(actor);
          }
        } else if (r == 3 && (unsigned)(actor->nextFrame - 0xB) < 9) {
          g_nActorAnimDone = 0;
          actor->frameProgress = 0x20;
          actor->frameRate = 0x20;
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 0x16;
          OnActorAnimQueued(actor);
        }
        ANIM_RETIME(actor);
        break;
      }

      case 10:
        TickCountdownByFrameDelta(&st->timer2, 4);
        actor->scale -= g_nFrameStep * 2;
        if (actor->scale < 0x21) {
          actor->scale = 0x20;
          st->kind = !st->kind;
          actor->mode = 0;
          continue;
        }
        break;

      case 20:
        TickCountdownByFrameDelta(&st->timer2, 4);
        actor->scale += g_nFrameStep * 2;
        if (actor->scale >= 0x38) {
          actor->scale = 0x20;
          BurstActorWithSound(actor, 0x20);
          st->base = 4;
          ANIM_SET(actor, st->base);
          st->kind = !st->kind;
          actor->mode = 0;
          continue;
        }
        break;

      case 30: {
        Actor *p = &g_pActorListBase[st->partner];
        int h = ArcTan2(p->posX - actor->posX, p->posY - actor->posY, 0);

        SteerActorAroundPoint(actor, &p->posX, 0x5DC,
                      WrapAddAngle8(ArcTan2(g_anSpyroWorldPos - p->posX,
                                                  g_nSpyroWorldPosY - p->posY, 0),
                                    0x80),
                      4, 0x32, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0xD);
        if (p->mode != 0xA) {
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            TurnActorYawTowardAngle(p, h, 4, 0, 0);
          }
          break;
        }
        st->timer = 0x3C;
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
    }

    /* Type 104: a comet. Flies its arc leaving a fading trail and drops a
     * spark (77) now and then; it is removed once the trail has faded. */
    case ACTOR_TYPE_COMET: {
      CometState *st = actor->state;
      int s = actor->mode;

      if (s < 3) {
        int n;
        int i;

        if (s < 0) {
          goto out104;
        }
        if (s == 0) {
          st->spark += g_nFrameStep;
          if (st->spark >= 0xC) {
            Actor *o = g_pfnLevelSpawnActor(ACTOR_TYPE_COMET_SPARK, actor);

            CopyVector(&o->posX, &actor->posX);
            o->subMode = 7;
            o->rotY = ArcTan2(st->speed, st->vz, 0);
            o->rotZ = actor->rotZ;
            o->cullRadius = 0x50;
            st->spark = 0;
          }
          do {
            actor->posX += (st->speed * g_anCosineLut[actor->rotZ]) >> 12;
            actor->posY += (st->speed * g_anSineLut[actor->rotZ]) >> 12;
            actor->posZ += st->vz;
            st->vz += st->dvz;
            if (st->steps != 0) {
              st->steps--;
            }
            if (st->lifeOn != 0) {
              if (st->life == 0) {
                actor->mode = 1;
              }
              st->life--;
            }
          } while (st->steps != 0);
          if (TickCountdownByFrameDelta(&st->timer, 2) != 0) {
            actor->mode = 1;
          }
          if (actor->posX < 0 || actor->posY < 0 || actor->posZ < 0) {
            actor->cullRadius = 0;
            actor->drawn = 0;
            actor->mode = 1;
            continue;
          }
          DispatchActorContactAtSphere(&actor->posX, 0x258, 0, ACTOR_HIT_BLAST, actor, 2);
          if (CollideSphereWithWorldAndActors(&actor->posX, 0x168, 0x168) != 0) {
            actor->mode = 1;
          }
          if (actor->mode == 1) {
            actor->cullRadius = 0;
            actor->drawn = 0;
          }
        }
        n = 0;
        if (actor->mode < 2 && g_nMotionTrailRibbonCount < 3 && st->count < 0x78) {
          int k = st->count;

          CopyVector(st->trail[k].pos, &actor->posX);
          st->trail[k].age = 0;
          st->count++;
        }
        for (i = 0; i < st->count; i++) {
          st->trail[i].age += g_nFrameStep;
          if (st->trail[i].age < 0x51) {
            n++;
          }
        }
        if (n == 0 && actor->mode != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if (g_nMotionTrailRibbonCount < 3) {
          int k = g_nMotionTrailRibbonCount;

          g_anMotionTrailSliceCounts[k] = st->count;
          g_nMotionTrailRibbonCount = k + 1;
          g_apMotionTrailSlices[k] = st->trail;
        }
        if (actor->mode == 1) {
          actor->mode = 2;
        }
      }
    out104:
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
        g056[0] = GetRandomU32() & 3;
        g056[1] = GetRandomU32() & 3;
        g056[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, g056);
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
          CopyVector(g136, &actor->posX);
          g136[2] += 0x400;
          FindGroundHeightBelow(g136, 0x10000);
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
            UnpackWorldCollisionTri(st->groundTri, g232[0]);
            AddVector(g232[0], g232[0], g232[1]);
            AddVector(g232[0], g232[0], g232[2]);
            actor->posX = g232[0][0] / 3;
            actor->posY = g232[0][1] / 3;
            actor->posZ = g232[0][2] / 3;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == 3) {
            CopyVector(g152, &actor->posX);
            g152[2] += 0x400;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(g152, 0x1000) > 0) {
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

          ScaleVector3Sat(g272, (int *)st, g_nFrameStep);
          RShiftVector3(g272, 1);
          dist = VectorLength(g272, VEC_LEN_XYZ);

          if (dist >= 0xDD) {
            ScaleVector3ByRatio(g272, dist, 0xDC);
          }

          at = &actor->posX;
          AddVector(g272, at, g272);

          if (st->vel[2] >= -0xDB) {
            st->vel[2] -= g_nFrameStep * 5;
          }

          if (g272[2] < 0) {
            if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
              CollectPickupActor(actor);
            }
            DespawnActorRecord(actor);
            continue;
          }

          g272[2] += 0xF0;

          if (CollideSphereWithWorldAndActors(g272, 0xF0, 0xF0) != 0) {
            if (GetGroundTriggerRegionType() == 0) {
              if (actor->type != ACTOR_TYPE_EXTRA_LIFE && actor->type != ACTOR_TYPE_SPARX_FOOD) {
                CollectPickupActor(actor);
              }
              DespawnActorRecord(actor);
              continue;
            }

            CopyVector(g288, &g_anCollisionProbeVec);
            CopyVector(at, &g_anCollisionResolvedPos);
            actor->posZ -= 0xF0;

            if (st->bounce == 0) {
              int floor = FindGroundHeightBelow(g272, 0x400);
              int slope =
                  (signed char)ArcTan2_8bit(g288[2], VectorLength(g288, VEC_LEN_XY));

              if ((g272[2] - 0x190) < floor && slope < 0x18) {
                actor->subMode = 1;
                CopyVector(at, g272);
                actor->posZ = floor;
                if (actor->type != ACTOR_TYPE_SPARX_FOOD) {
                  st->lean = -ArcTan2_8bit(
                      IntegerSqrt((g288[0] * g288[0]) + (g288[2] * g288[2])),
                      g288[1]);
                  st->roll = -ArcTan2_8bit(g288[2], g288[0]);
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

            if (ReflectVectorOffNormal((int *)st, g288, (int *)st) != 0) {
              st->bounce -= 1;
              st->vel[0] = (st->vel[0] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[1] = (st->vel[1] >> 3) + (GetRandomU32() & 0x3F) - 0x20;
              st->vel[2] = (st->vel[2] >> 2) + (GetRandomU32() & 0xF);
            }
          } else {
            g272[2] -= 0xF0;
            CopyVector(at, g272);
            FindGroundHeightBelow(g272, 0x10000);
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
            SubtractVector(g304, &spy->posX, (int *)st);
            RShiftVector3(g304, 5);

            if (VectorLength(g304, VEC_LEN_XYZ) >= 0x1E1) {
              CopyVector(&actor->posX, &spy->posX);
              st->timer = 0x20;
            } else {
              ScaleVector3Sat(g304, g304, st->timer);
              AddVector(&actor->posX, (int *)st, g304);
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
      SaveHostState *st = (SaveHostState *)actor->state;

      Actor *tgtA = &g_pActorListBase[st->armer];
      Actor *tgtB = &g_pActorListBase[st->anchor];

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
              if (st->prompted == 0 || tgtB->mode == 1) {
                st->prompted = 0;
                actor->posZ = st->baseZ + 0x80;
                actor->rotZ = ArcTan2(SPY.posX - actor->posX,
                                             SPY.posY - actor->posY, 0) -
                               128;
                actor->cullRadius = 0x20;
                actor->mode = 3;
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

      case 3: {
        if (actor->scale >= 0x21) {
          actor->scale -= 4;
          actor->rotZ = actor->rotZ + 8;
        } else {
          actor->mode = 4;
          st->timer = 0;
          st->wanderTimer = 30;
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
            st->prompted = 1;
            BeginSaveMenuFromWorld(actor);
            break;
          }
        }

        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0x2400 ||
            (ABS2(actor->posZ - actor->floorOffset - SPY.posZ) > 0xC00)) {
          actor->mode = 6;
        } else {
          st->wanderTimer--;
          if (st->wanderTimer == 0) {
            int baseDistance = (GetRandomU32() & 0x3F) + 0x140;
            char finalAngle = (ArcTan2(SPY.posX - tgtB->posX,
                                             SPY.posY - tgtB->posY, 0) +
                               (GetRandomU32() & 0xF) - 8);

            CopyVector(st->wanderTo, &tgtB->posX);

            actor->mode = 5;

            st->wanderTo[0] -= (LookupCosine(finalAngle * 0x10) * baseDistance) >> 12;
            st->wanderTo[1] -= (LookupSine(finalAngle * 0x10) * baseDistance) >> 12;

            st->faceYaw = ArcTan2(SPY.posX - st->wanderTo[0],
                                      SPY.posY - st->wanderTo[1], 0);
            st->wanderTimer = 0;
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
            st->prompted = 1;
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

          st->wanderTimer++;

          SubtractVector(g136, st->wanderTo, &actor->posX);

          angleDiff = SignedAngleDelta8(st->faceYaw, actor->rotZ);

          cosWeight1 = LookupCosine((st->wanderTimer - 1) << 7);
          cosDelta = cosWeight1 - LookupCosine(st->wanderTimer << 7);
          totalWeight = LookupCosine((st->wanderTimer - 1) << 7) + 0x1000;

          actor->posX += (g136[0] * cosDelta) / totalWeight;
          actor->posY += ((g136[1] * cosDelta) / totalWeight);
          actor->rotZ += (angleDiff * cosDelta) / totalWeight;

          FindGroundHeightBelow(&actor->posX, 0x1000);
          EncodeCachedVecToActorDirCode(actor);

          if (st->wanderTimer == 0x10) {
            actor->mode = 4;
            st->wanderTimer = (GetRandomU32() & 0x1F) + 0x1E;
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
        g136[0] = 0;
        g136[1] = 0x64;
        g136[2] = 0;
        RotateVectorByMatrix(&actor->mtx[0], g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g136, 0);
        g136[0] = 0;
        g136[1] = -0x64;
        g136[2] = 0;
        ApplyActiveGteRotation(g136, g136);
        AddVector(g136, g136, &actor->posX);
        g_pfnLevelEmitParticles(2, EMIT_TWINKLE, g136, 0);
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

            CopyVector(g152, &st->target->posX);

            if (high == 0) {
              g152[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              g152[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              g152[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              g152[1] -=
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

            SubtractVector(g152, g152, &actor->posX);
            dist = VectorLength(g152, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(g152, dist, limit);
            } else {
              ScaleVector3ByRatio(g152, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, g152);
            TurnActorYawTowardAngle(actor, ArcTan2(g152[0], g152[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(g152, VEC_LEN_XY), g152[2], 0);
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

            SubtractVector(g320, &st->target->posX, &actor->posX);
            dist = VectorLength(g320, VEC_LEN_XYZ);

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
                ScaleVector3ByRatio(g320, dist, lim);
              } else {
                ScaleVector3ByRatio(g320, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, g320);
              TurnActorYawTowardAngle(actor, ArcTan2(g320[0], g320[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(g320, VEC_LEN_XY), g320[2], 0);
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

    /* Type 126: a path walker. Turns to face Spyro when he is near, walks
     * its path, sings its tune while it rests, and can be carried and
     * thrown by its rider. */
    case ACTOR_TYPE_SINGING_PATH_WALKER: {
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
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
        if (g_nActorAnimDone != 0) {
          st->timer = st->period;
          switch (st->mode) {
          case 0:
          case 1:
          case 3:
            ModifyWorldChunkAnimFlags(st->sound, CHUNK_ANIM_RESUME, 0);
            break;
          case 2:
            if (g_pActorListBase[st->partner].mode >= 6 &&
                ((int *)g_pActorListBase[st->partner].state)[18] == 0) {
              st->flag = one;
            } else {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(st->sound, CHUNK_ANIM_RESUME, 0);
            }
            break;
          case 4:
            if (CHUNK_ANIM_STEP(GetWorldChunkAnimState(1)) > 0 ||
                !(GetWorldChunkAnimState(1) & CHUNK_ANIM_HALTED)) {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(2, CHUNK_ANIM_RESUME, 0);
              st->sound = 2;
            } else {
              st->timer = 0x1C2;
              ModifyWorldChunkAnimFlags(1, CHUNK_ANIM_RESUME, 0);
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
        RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION | ACTOR_REFRESH_MATRIX);
        if (st->next == 0x63) {
          ENTER_POSE(actor, 0xC);
        }
        if (st->flag != 0 && (GetWorldChunkAnimState(st->sound) & CHUNK_ANIM_HALTED)) {
          st->flag = 0;
          st->timer = 0x14A;
          ModifyWorldChunkAnimFlags(st->sound, CHUNK_ANIM_RESUME, 0);
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
            ModifyWorldChunkAnimFlags(st->sound, CHUNK_ANIM_RESUME, 0);
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
            if ((s & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(s) < 0x14) {
              SetWorldChunkAnimStep(st->sound, 0x1E, 0);
            }
            ModifyWorldChunkAnimFlags(st->sound, CHUNK_ANIM_RESUME, 0);
            ENTER_POSE(actor, 0xC);
          }
          break;
        }
        continue;

      case 12:
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
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

    /* Type 130: the big guard (as type 92, plus two followers it wakes and
     * a growl while it lunges). */
    case ACTOR_TYPE_BIG_LUNGING_GUARD: {
      BigLungingGuardState *st = actor->state;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) && actor->mode != 3 &&
          (st->kind != 1 || !(actor->flags & ACTOR_HIT_CHARGE))) {
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
        st->lift = 0x96;
        st->knock = 0x118;
        if (actor->flags & ACTOR_HIT_CHARGE) {
          st->knock = 0x17C;
        }
        st->heading = StepAndLerpAngle8(ArcTan2(actor->posX - g_anSpyroWorldPos,
                                                  actor->posY - g_nSpyroWorldPosY, 0),
                                    g_bSpyroPersistentEulerZ, 0x20, 0x40);
        actor->flags = 0;
        actor->probeDist = 0xFF;
        st->spin = RandomSignedInRange(8, 0xE);
        actor->subMode = 0;
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ANIM_GO(actor, st->base + 3);
        actor->mode = 3;
        continue;
      }
      if (st->init == 0) {
        st->init = one;
        if (st->flip != 0 && actor->dropKind == 0xFF) {
          st->kind = !st->kind;
        }
        if (st->kind == 0) {
          st->base = 4;
          ANIM_SET(actor, st->base);
        }
      }
      if (actor->mode < 3 && (st->range != 0 || (actor->flags & ACTOR_HIT_BLAST))) {
        if (st->range > 0 && (actor->mode == 0 || actor->mode == 2) &&
            (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range ||
             IsPointInRotatedBox((int *)spy, st->vol) != 0) &&
            TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->range = -st->range;
          if (st->range == 0) {
            st->range = -1;
          }
          g_pActorListBase[st->partner].subMode = actor - g_pActorListBase;
          *(int *)g_pActorListBase[st->partner].state = st->msg;
          st->timer2 = st->reload;
          if (st->follow1 != -1) {
            ((int *)g_pActorListBase[st->follow1].state)[19] = 0x28;
          }
          if (st->follow2 != -1) {
            ((int *)g_pActorListBase[st->follow2].state)[19] = 0x50;
          }
        } else if (actor->flags & ACTOR_HIT_BLAST) {
          actor->flags = 0;
          if (st->flags & 4) {
            st->range = -st->range;
          } else {
            st->range = 0;
          }
          if (st->kind == 0) {
            st->kind = !st->kind;
            BurstActorWithSound(actor, 0x28);
            actor->scale = 0x40;
            st->base = 0;
            ANIM_SET(actor, st->base);
            actor->mode = 10;
            continue;
          }
          st->kind = !st->kind;
          actor->scale = 0x20;
          actor->mode = 0x14;
          continue;
        } else if (st->range < 0) {
          TickCountdownByFrameDelta(&st->timer2, 4);
        }
      }
      actor->flags = 0;

      switch (actor->mode) {
      case 0:
        if ((st->flags & 2) &&
            ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < st->range2) {
          ANIM_GO(actor, st->base + 1);
          actor->mode = 1;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 &&
            (SPY_DZ(actor) > 0 ? SPY_DZ(actor) < 0x190
                               : -SPY_DZ(actor) < 0x190)) {
          if (st->kind == 0) {
            if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x4B0) {
              actor->mode = 2;
              continue;
            }
          } else if (st->kind == 1 &&
                     ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0x1F40) {
            actor->mode = 2;
            continue;
          }
        }
        break;

      case 1:
        st->flags &= ~2;
        if (ApproxDist2D(&actor->posX, &g_anSpyroWorldPos) < 0xC00 ||
            PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 8, 0x28, 0xFF, 5) ==
                0x100) {
          ANIM_GO(actor, st->base);
          actor->mode = 0;
          continue;
        }
        break;

      case 2:
        if (st->kind == 0) {
          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          g_anSpyroRequestVelocity.x = (g_anCosineLut[actor->rotZ] * 0x23) >> 10;
          g_nSpyroRequestVelocityY = (g_anSineLut[actor->rotZ] * 0x23) >> 10;
          g_nSpyroRequestVelocityZ = 0x46;
          if (g_nActorAnimDone != 0) {
            st->timer = 0x78;
            ANIM_GO(actor, st->base);
            actor->mode = 0;
            continue;
          }
          break;
        } else {
          int dist = ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);

          ANIM_GO(actor, st->base + 2);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (g_nActorAnimDone != 0 || dist > 0x2710) {
            st->timer = 0x96;
            ANIM_GO(actor, st->base);
            actor->mode = 0;
            continue;
          }
          if (dist < 0x6A4 && actor->anim == actor->nextAnim &&
              actor->nextFrame < 0xE) {
            actor->frame = 0x10;
            actor->nextFrame = 0x11;
            actor->frameProgress = 0;
          }
          if (actor->nextFrame == 0xF) {
            actor->frame = 5;
            actor->nextFrame = 6;
            actor->frameProgress = 0;
          }
          if (actor->nextFrame == 6 &&
              GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[0]) == 0) {
            PlayActorMeshVoice(actor, 0, (int)&st->sound);
          }
          break;
        }

      case 3: {
        int r;

        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        actor->rotZ += st->spin;
        if (++actor->subMode >= 3) {
          if (st->spin > 0) {
            st->spin--;
            if (st->spin < 0) {
              st->spin = 0;
            }
          } else if (st->spin < 0) {
            st->spin++;
            if (st->spin > 0) {
              st->spin = 0;
            }
          }
        }
        r = DriveActorTrackedMove(actor, &st->knock, st->heading, &st->lift, 0xC, 0x10);
        if (r == 3 && actor->nextFrame < 0xA) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 0xA;
          OnActorAnimQueued(actor);
        }
        if (r == 0 && actor->nextFrame == 9) {
          g_nActorAnimDone = 0;
          actor->frameProgress = sixteen;
          actor->frameRate = sixteen;
          actor->anim = actor->nextAnim;
          actor->nextAnim = actor->nextAnim;
          actor->frame = actor->nextFrame;
          actor->nextFrame = 5;
          OnActorAnimQueued(actor);
        }
        ANIM_RETIME(actor);
        break;
      }

      case 10:
        TickCountdownByFrameDelta(&st->timer2, 4);
        actor->scale -= g_nFrameStep * 2;
        if (actor->scale < 0x21) {
          actor->scale = 0x20;
          actor->mode = 0;
          continue;
        }
        break;

      case 20:
        TickCountdownByFrameDelta(&st->timer2, 4);
        actor->scale += g_nFrameStep * 2;
        if (actor->scale >= 0x38) {
          StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
          actor->scale = 0x20;
          BurstActorWithSound(actor, 0x20);
          st->base = 4;
          ANIM_SET(actor, st->base);
          actor->mode = 0;
          continue;
        }
        break;

      case 30: {
        Actor *p;
        int h;

        p = &g_pActorListBase[st->partner];
        ApproxDist2D(&actor->posX, &g_anSpyroWorldPos);
        h = ArcTan2(p->posX - actor->posX, p->posY - actor->posY, 0);
        SteerActorAroundPoint(actor, &p->posX, 0x5DC,
                      WrapAddAngle8(ArcTan2(g_anSpyroWorldPos - p->posX,
                                                  g_nSpyroWorldPosY - p->posY, 0),
                                    0x80),
                      4, 0x32, 0xE, 0x80, 0xFF, 0xFF, 0, 0, 0xD);
        TurnActorYawTowardAngle(p, h, 4, 0, 0);
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      continue;
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
        CopyVector(g384, &actor->posX);
        g384[3] = GetRandomU32() & 3;
        g384[4] = GetRandomU32() & 3;
        g384[5] = 0x14;
        g368[0] = 0x80;
        g368[1] = 0x70;
        g368[2] = 0x40;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_RGB, g384, g368);
      }
      break;
    }

    /* Type 160: wandering critter; knocked away when hit. */
    case ACTOR_TYPE_WANDERING_CRITTER: {
      CritterKnockState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != 1) {
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
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
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

    /* Type 191: a talking NPC. Starts a conversation when Spyro walks up
     * facing it; the line depends on how far through the level he is. */
    case ACTOR_TYPE_HOME_TALKER + 4: {
      if (actor->mode == 0) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1000) {
          actor->mode = 1;
        }
      } else if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
                  SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_CHARGE) &&
                 ApproxDist2D(&actor->posX, &spy->posX) < 0x780 &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - spy->posX,
                                             actor->posY - g_nSpyroWorldPosY, 0)) <
                     0x20 &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(spy->posX - actor->posX,
                                             g_nSpyroWorldPosY - actor->posY, 0)) <
                     0x28) {
        actor->mode = 0;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ResetSpyroLinearMotion();
        ZeroVector(&spy->headLook);
        g_pfnLevelBeginActorCutscene(actor);
        g_abWorldBalloonistState[1] = 2;
        g_abWorldBalloonistState[2] = 2;
        g_abWorldBalloonistState[3] = 2;
        g_abWorldBalloonistState[4] = 2;

        if (g_nWorldTreasureCollected < 0x1770) {
          if (g_abWorldBalloonistState[5] == 0) {
            g_nActorCutsceneDialogPage = 0x18;
            g_abWorldBalloonistState[5] = 1;
          } else {
            g_nActorCutsceneDialogPage = 0x1A;
          }
        } else if (g_abWorldBalloonistState[5] < 2) {
          if (g_abWorldBalloonistState[5] == 0) {
            g_nActorCutsceneDialogPage = 0x1B;
          } else {
            g_nActorCutsceneDialogPage = 0x1C;
          }
          g_abWorldBalloonistState[5] = 2;
        } else {
          g_nActorCutsceneDialogPage = 0x1E;
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
          SubtractVector(g408, at, &spy->posX);
          g408[2] = (g408[2] * 3) >> 2;

          if (VectorLength(g408, VEC_LEN_XYZ) < 0x440) {
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
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x258 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 7) + 7;
        }
        break;

      case 2:
        switch (st->sub) {
        case 0:
          if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
            st->timer = 0x200 - (GetRandomU32() & 0x7F);
            voice = (GetRandomU32() & 1) + 0xD;
            st->sub = 1;
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
            st->sub = 0;
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
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          st->timer = 0x270 - (GetRandomU32() & 0x7F);
          voice = (GetRandomU32() & 1) + 0xF;
        }
        break;

      case 4:
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
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

      case 8:
        voice = 0x1A;
        break;

      case 9:
        voice = 0x36;
        break;
      }

      if (voice >= 0) {
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, SFX_MODE_3D, &actor->sfxSlot);
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
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1000 &&
            ABS2((actor->posZ - actor->floorOffset) - SPY.posZ) < 0x400) {
          st->timer = 0xB4;
        }
        break;
      }
      if (flags & 0x10)
        actor->subMode = 1;
      PlaySoundEffect(g_pLevelSampleBankHeader[SFX_WHIRLWIND], actor, SFX_MODE_3D, &actor->sfxSlot);
      if ((GetRandomU32() & 1) == 0)
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_PLUME, (int *)actor, 0);
      SubtractVector(g408, &SPY.posX, &actor->posX);
      distance = VectorLength(g408, VEC_LEN_XY);
      if (distance < st->radius && g408[2] >= -0x3FF && g408[2] < st->spanZ &&
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

        g424[0] = (GetRandomU32() & 0xFE) - 0x7F;
        g424[1] = (GetRandomU32() & 0xFE) - 0x7F;
        g424[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(g424, g424, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, g424, (int *)1);
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
            g440[0] = (GetRandomU32() & 0xFE) - 0x7F;
            g440[1] = (GetRandomU32() & 0xFE) - 0x7F;
            g440[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(g440, g440, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, g440, (int *)1);
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
      case 0:
        *(int *)&actor->tint0 = g_anTintByDropKind[actor->dropKind];
        actor->subMode = 1;
        break;

      case 1:
        if (actor->flags & ACTOR_HIT_BLAST) {
          actor->subMode = 2;
          actor->updateDist = 0xFF;
          actor->cullRadius = 0x7F;
          if (st->timer < 0x5A) {
            st->timer = 0x5A;
          }
        } else if (actor->flags & ACTOR_HIT_FLAME) {
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
        if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) {
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
        if ((actor->flags & ACTOR_HIT_BLAST) && t < 0x5A) {
          st->timer = 0x5A;
        }
        for (i = 0; i < 6; i++) {
          g456[0] = g_aSmokeJetEmitters[i >> 1].pos[0];
          g456[1] = g_aSmokeJetEmitters[i >> 1].pos[1];
          g456[2] = g_aSmokeJetEmitters[i >> 1].pos[2];
          RotateVectorByMatrix(actor->mtx, g456, g456);
          AddVector(g456, g456, &actor->posX);
          g456[3] = g_aSmokeJetEmitters[i >> 1].vel[0];
          g456[4] = g_aSmokeJetEmitters[i >> 1].vel[1];
          g456[5] = g_aSmokeJetEmitters[i >> 1].vel[2];
          RotateVectorByMatrix(actor->mtx, g456 + 3, g456 + 3);
          g456[3] += (GetRandomU32() & 0xF) - 8;
          g456[4] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, g456, g456 + 3);
        }
        n = st->timer / 40 + 1;
        for (i = 0; i < n; i++) {
          g456[0] = (GetRandomU32() & 0x1FF) - 0xFF;
          g456[1] = (GetRandomU32() & 0x1FF) - 0xFF;
          g456[2] = (GetRandomU32() & 0x7F) + 0xC0;
          g456[3] = g456[0] >> 4;
          g456[4] = g456[4] >> 4;
          g456[5] = (GetRandomU32() & 0xF) + 8;
          RotateVectorByMatrix(actor->mtx, g456, g456);
          AddVector(g456, g456, &actor->posX);
          RotateVectorByMatrix(actor->mtx, g456 + 3, g456 + 3);
          g480[0] = (GetRandomU32() & 7) + 0x10;
          g480[1] = (GetRandomU32() & 7) + 6;
          g_pfnLevelEmitParticles(1, EMIT_FADING_SPARK, g456, g480);
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
            RaiseSpyroTriggerInRadius(&actor->posX, (st->timer + 1) << 8, 0x86) != 0) {
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
          ZeroVector(g480);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, g480);
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
            CopyVector(g496, &st->prize->posX);
            g496[0] += g_anCosineLut[j * 43] >> 6;
            g496[1] += g_anSineLut[j * 43] >> 6;
            g496[2] += 0x28;
            g512[0] = g_anCosineLut[j * 43] >> 7;
            g512[1] = g_anSineLut[j * 43] >> 7;
            g512[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, g496, g512);
          }
          DespawnActorRecord(st->prize);
          actor->killGroup &= ACTOR_KILL_GROUP_MASK;
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

    /* Portal guide (PortalState). */
    case ACTOR_TYPE_PORTAL_GUIDE: {
      PortalState *st = actor->state;
      PortalCamPath *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case 1: {

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
        AddVector(g480, p->posA, p->posB);
        RShiftVector3(g480, 1);

        SubtractVector(&SPY.requestVelX, g480, &SPY.posX);
        AddVector(&SPY.requestVelX, &SPY.requestVelX, st->offset);

        mag = VectorLength(&SPY.requestVelX, VEC_LEN_XYZ);
        if (mag > 128) {
          ScaleVector3ByRatio(&SPY.requestVelX, mag, 128);
        } else {
          if (actor->subMode != 0) {
            progressed = (st->desc->index < (st->desc->count - 1));
          } else {
            progressed = st->desc->index;
          }

          if (progressed) {
            actor->mode = 2;
          }
        }

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = 0;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          SPY.requestState = SPYRO_ST_GLIDE;
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nGamePaused = 1;
          SPY.requestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.bodyYaw;

          SPY.bodyYaw += g_nPortalViewYawSpin;
          SPY.bodyRotZ = SPY.bodyYaw >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nPortalViewPitchTilt;
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
      case 2: {
        SPY.requestMask = SPYRO_RQ_PENDING | 0x4000 | 0x2000 | 0x100;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = 0x60;
        SPY.requestState = SPYRO_ST_GLIDE;
        SPY.otBinBias = 0x7F;

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = 0;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          g_nCdStreamState = 0;
          g_nLevelTransitionTallyActive = 1;
          g_nLevelTransitionPhase = 0;
          g_nGamestate = GS_LEVEL_LOAD;
          g_nGamePaused = 1;
          SPY.requestYaw = st->dest;
          ComputeCameraToTargetAngle(&CAM.posX);

          CAM.sim.v[0] += SPY.bodyYaw;

          SPY.bodyYaw += g_nPortalViewYawSpin;
          SPY.bodyRotZ = SPY.bodyYaw >> 4;
          CAM.sphere = CAM.sim;
          CAM.sphere.v[0] -= (SPY.bodyRotZ) << 4;
          ComputeCameraOrbitOffset(&CAM.posX);
          AddVector(&CAM.posX, &CAM.posX, &SPY.posX);
          UpdateCameraEulerAngles();
          g_nLevelIntroParallaxCountdown = g_nPortalViewPitchTilt;
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

      if (actor->mode == 0) {
        *base = actor->posZ;
        actor->mode = 1;
        break;
      }

      actor->posZ = *base + (g_anSineLut[actor->subMode] >> 4);
      actor->subMode += g_nFrameStep;
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
        g496[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        g496[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        g496[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, g496);
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
          g496[0] = (g_anCosineLut[bearing] * 3) >> 4;
          g496[1] = (g_anSineLut[bearing] * 3) >> 4;
          g496[2] = 0;
          g528[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g528[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          g528[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(g544, g528, st->radius - 1);
          LShiftVector3(g528, 1);
          arm = st->radius - 1;
          g544[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          g560[0] = g496[0] * g_anCosineLut[arm * 2];
          g560[1] = g496[1] * g_anCosineLut[arm * 2];
          g560[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(g528, g528, st->phase);
          SubtractVector(g544, g544, g528);
          swing = &g_anCosineLut[bearing];
          actor->posX = g496[0] * *swing;
          actor->posY = g496[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, g560);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, g496);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, g544);
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
