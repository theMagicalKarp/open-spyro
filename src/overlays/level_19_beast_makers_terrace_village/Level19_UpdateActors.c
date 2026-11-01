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
 *     the original slot order (o152, o256..o528 are slots shared by arms).
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
 *   - Case 14 samples the floor through svF (idle) and o152 (mode 3).
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


/* Type 421: life chest. Shakes and pops its lid on a timer, spawning a
 * child actor; breaks into fragments when damaged. */


/* 0x18-byte spherical camera coordinates; g_aCameraXformPresets holds the presets. */


/* Glow node owned by Sparx; follows its anchor position. */


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


/* ==== Level 19 classes ==== */

/* ENTER_POSE for arms whose fall-out runs a shared exit after the switch. */
#define ENTER_POSE_BRK(a, n)                                                   \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    ANIM_ADVANCE(a, n);                                                        \
  }                                                                            \
  break

/* Type 35 state: a guard that scatters when hit and fires type 494 bolts. */
typedef struct BoltGuardState {
  int mode;    /* 0x00 1 = guards, 2 = follows a linked actor */
  int timer;   /* 0x04 */
  int timer2;  /* 0x08 */
  int heading; /* 0x0C */
  int speed;   /* 0x10 */
  int turn;    /* 0x14 */
  int u18;     /* 0x18 */
  void *path;  /* 0x1C */
  int range;   /* 0x20 in 1024s; the next link for a followed actor */
  int countdown;     /* 0x24 */
  int link;    /* 0x28 actor index, -1 for none */
  short spinX; /* 0x2C */
  short spinY; /* 0x2E */
  int box[3];  /* 0x30 */
} BoltGuardState;

/* Type 343 state: a bead that streams from its owner's attach point 1 to
 * point 0, starting subMode * 42 frames late and growing as it goes. */
typedef struct AttachBeadState {
  Actor *owner; /* 0x00 */
  int t;        /* 0x04 */
} AttachBeadState;

/* Type 467 state: a bolt-throwing sentry with three guards (`kids`). */
typedef struct BoltSentryState {
  int mode;      /* 0x00 */
  int *path;     /* 0x04 */
  int range;     /* 0x08 in 1024s */
  int timer;     /* 0x0C */
  int u10;       /* 0x10 */
  Actor *bolt;   /* 0x14 */
  int u18;       /* 0x18 */
  int heading;   /* 0x1C */
  int speed;     /* 0x20 */
  int u24;       /* 0x24 */
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
  short mode;    /* 0x12 */
} AimedBoltState;

/* ENTER_POSE_BRK with the pose constant split from the queue store (A285). */
#define ENTER_POSE_BRK_B(a, n)                                                 \
  (a)->mode = (n);                                                            \
  if ((a)->nextAnim != (n)) {                                                     \
    do {                                                                       \
    } while (0);                                                               \
    ANIM_ADVANCE(a, n);                                                        \
  }                                                                            \
  break

/* ==== The function ==== */
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
  int svB[3];
  int svC[3];
  int svD[3];
  int svE[3];
  int svF[3];
  int o152[3];
  int acc[3][3];
  int svG[3];
  int svH[3];
  int svI[3];
  int o256[3];
  int o272[3];
  int o288[3];
  int o304[3];
  int o320[3];
  int o336[3];
  int o352[3];
  int o368[3];
  int o384[3];
  int o400[3];
  int o416[3];
  int o432[3];
  int o448[3];
  int o464[3];
  int o480[3];
  int o496[3];
  int o512[3];
  int o528[3];

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

      if (actor->mode != LEVELEXIT_FAR ? dist < 0x2800 : dist < 0x2400) {
        if (actor->mode == LEVELEXIT_FAR) {
          actor->posZ += 0x600;
          SpawnNameLetterActors(actor, 0);
          actor->posZ -= 0x600;
        }
        actor->mode = oneq;
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
          moved = MoveActorAlongHeading(actor, 30, 0, 50, MOVE_LIFT);

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

    /* Type 32: rides its carrier until the carrier reaches pose 4, then
     * behaves as a dragon egg. */
    case ACTOR_TYPE_CARRIED_EGG: {
      Actor **st = actor->state;

      if (actor->subMode != 0 || (*st)->mode != 4) {
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

      if ((unsigned int)(actor->mode - 2) < 3) {
        AimCameraStickTowardPoint(&actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_SPARK, actor, (int *)0x602080);
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
        SubtractVector(svE, &actor->posX, &CAM.posX);
        WorldToCameraRotate(svE, svE);
        g_pfnLevelEmitParticles(0x10, EMIT_DRIFT_STREAK, svE, 0);
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
      d = ApproxDist2D(&actor->posX, spy);
      if (d < 0x32C8) {
        d = VectorLength(v, VEC_LEN_XY);
      }
      TickActorFlameGlow(actor);
      if (actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER) && actor->mode != BGUARD_KNOCKED) {
        int i;

        st->turn = 0xE6;
        st->speed = 0x12C;
        st->heading =
            StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                        actor->posY - g_nSpyroWorldPosY, 0),
                          g_bSpyroPersistentEulerZ, 0x20, 0x40);
        actor->flags = 0;
        st->spinX = RandomSignedInRange(6, 0xA);
        st->spinY = RandomSignedInRange(6, 0xA);
        for (i = 0; i < 0xA; i++) {
          if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
            break;
          }
          g_pfnLevelSpawnActor(ACTOR_TYPE_GROUND_BOUNCE_SHARD, actor);
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ANIM_SET(actor, 6);
        PlayActorMeshVoice(actor, 0, 0);
        actor->mode = BGUARD_KNOCKED;
        continue;
      }
      actor->flags = 0;
      switch (actor->mode) {
      case BGUARD_WATCH:
        ANIM_GO(actor, 0);
        if (st->mode == one &&
            (IsPointInRotatedBox(&spy->posX, st->box) != 0 || d < st->range << 10) &&
            ABS2(SPY_DZ(actor)) < 0xBB8) {
          if (actor->group != 0xFF) {
            SetActorGroupStateExcept(actor, BGUARD_RUN, BGUARD_KNOCKED);
          }
          actor->mode = BGUARD_RUN;
          continue;
        }
        if (st->mode == 2 && st->link != -1 && d < st->range << 10 &&
            ABS2(SPY_DZ(actor)) < 0xBB8) {
          st->timer2 = 0x3C;
          actor->mode = BGUARD_FACE_LINK;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0xFA0 &&
            ABS2(SPY_DZ(actor)) < 0x320 && TickCountdownByFrameDelta(&st->countdown, 4) != 0) {
          ENTER_POSE(actor, BGUARD_WIND_UP);
        }
        break;

      case BGUARD_ROUSE:
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, BGUARD_WIND_UP);
        }
        break;

      case BGUARD_FIRE:
        TrackEntityYawTowardSpyro(actor, 0xE, 0, 1);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          int i;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          for (i = 0; i < 2; i++) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
            AimedBoltState *cs;

            c->scale = 0x48;
            c->rotZ = actor->rotZ;
            cs = c->state;
            cs->mode = one;
            cs->timer = 0x1E;
            GetActorAttachPoint(actor, i, &c->posX);
          }
          st->timer = 8;
        }
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0 && g_nActorAnimDone != 0) {
          st->timer = 0xB4;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        break;

      case BGUARD_KNOCKED:
        actor->rotX += st->spinX;
        actor->rotY += st->spinY;
        if (DriveActorTrackedMove(actor, &st->speed, st->heading, &st->turn, 0xC,
                          0x10) == 3) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case BGUARD_WIND_UP:
        if (g_nActorAnimDone != 0) {
          st->timer2 = 0x28;
          ENTER_POSE(actor, BGUARD_FIRE);
        }
        break;

      case BGUARD_RUN:
        ANIM_GO(actor, 4);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x5A, 0, 8, 0x10, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            0x100) {
          st->mode = 0;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) < 0xC00) {
          st->mode = 0;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        break;

      case BGUARD_FACE_LINK:
        ANIM_GO(actor, 0);
        if (g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT ||
            ApproxDist2D(&actor->posX, &SPY.posX) < 0x1400 || st->link == -1) {
          st->mode = 0;
          ENTER_POSE(actor, BGUARD_WATCH);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->link].posX - actor->posX,
                                    g_pActorListBase[st->link].posY - actor->posY, 0),
                      6, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0) {
          st->timer2 = 0x19;
          actor->mode = BGUARD_FIRE_AT_LINK;
          continue;
        }
        break;

      case BGUARD_FIRE_AT_LINK:
        ANIM_GO(actor, 2);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(g_pActorListBase[st->link].posX - actor->posX,
                                    g_pActorListBase[st->link].posY - actor->posY, 0),
                      6, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          int i;

          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          for (i = 0; i < 2; i++) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
            AimedBoltState *cs;

            c->scale = 0x48;
            cs = c->state;
            c->rotZ = actor->rotZ;
            cs->mode = 2;
            cs->timer = 0x2D;
            CopyVector(cs->target, &g_pActorListBase[st->link].posX);
            cs->target[2] += 0xFA;
            GetActorAttachPoint(actor, i, &c->posX);
          }
          st->timer = 8;
        }
        if (TickCountdownByFrameDelta(&st->timer2, 4) != 0 && g_nActorAnimDone != 0) {
          BoltGuardState *ts = g_pActorListBase[st->link].state;

          if (g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT || st->link == -1) {
            st->mode = 0;
            ENTER_POSE(actor, BGUARD_WATCH);
          }
          st->link = ts->range;
          st->timer2 = 0x3C;
          actor->mode = BGUARD_FACE_LINK;
          continue;
        }
        break;
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
        svA[0] = GetRandomU32() & 3;
        svA[1] = GetRandomU32() & 3;
        svA[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svA);
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

      if (actor->drawn != 0 || toSpyro <= 0x2000 || actor->subMode != COLLECT_REST ||
          st->mode != COLLREST_STILL) {
        switch (actor->subMode) {
        case COLLECT_SETTLE:
          /* Idle: sample the floor normal and face along it. */
          CopyVector(svF, &actor->posX);
          svF[2] += 0x400;
          FindGroundHeightBelow(svF, 0x10000);
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
          actor->subMode = COLLECT_REST;
          goto enemy_pose;

        case COLLECT_REST: {

          if (st->mode == COLLREST_STILL) {
          } else if (st->mode == COLLREST_SNAP) {
            SnapActorToGround(actor);
            EncodeCachedVecToActorDirCode(actor);
            if (actor->type >= ACTOR_TYPE_GEM_1) {
              {
                int *nrm = &g_anCollisionProbeVec;

                st->lean =
                    -ArcTan2_8bit(IntegerSqrt((nrm[0] * nrm[0]) +
                                                 (g_nCollisionProbeVecZ * g_nCollisionProbeVecZ)),
                                   g_nCollisionProbeVecY);
                st->roll = -ArcTan2_8bit(g_nCollisionProbeVecZ, nrm[0]);
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
            EncodeCachedVecToActorDirCode(actor);
          } else if (st->mode == COLLREST_FIND_TRI) {
            CopyVector(o152, &actor->posX);
            o152[2] += 0x400;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(o152, 0x1000) > 0) {
              st->groundTri = g_nLastCollisionHitChunkId;
              st->mode = COLLREST_ON_TRI;
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
                actor->subMode = COLLECT_REST;
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
            PlaySoundEffect(g_pLevelSampleBankHeader[SFX_GEM_BOUNCE], actor, SFX_MODE_3D, &actor->voiceRow);

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

          if ((st->timer >= 0x21 && toSpyro < 0x200 &&
               (unsigned int)((SPY.posZ - actor->posZ) + 0x17F) < 0x37F) ||
              (actor->subMode == COLLECT_HOMING && st->timer >= 0x41)) {
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
      case SAVEHOST_WAIT_AWAY:
        actor->collision = 0;
        if (ApproxDist2D(&rider->posX, &SPY.posX) > 0xA00) {
          actor->mode = SAVEHOST_WAIT_ARMED;
        }
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

            CopyVector(o256, &st->target->posX);

            if (high == 0) {
              o256[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 20;
              o256[1] -=
                  (unsigned short)g_anSineLut[st->target->rotZ] << 16 >> 20;
            } else {
              o256[0] -=
                  (unsigned short)g_anCosineLut[st->target->rotZ] << 16 >> 22;
              o256[1] -=
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

            SubtractVector(o256, o256, &actor->posX);
            dist = VectorLength(o256, VEC_LEN_XYZ);

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
              ScaleVector3ByRatio(o256, dist, limit);
            } else {
              ScaleVector3ByRatio(o256, dist, reach - 5);
            }
            AddVector(&actor->posX, &actor->posX, o256);
            TurnActorYawTowardAngle(actor, ArcTan2(o256[0], o256[1], 0), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(o256, VEC_LEN_XY), o256[2], 0);
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

            SubtractVector(o272, &st->target->posX, &actor->posX);
            dist = VectorLength(o272, VEC_LEN_XYZ);

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
                ScaleVector3ByRatio(o272, dist, lim);
              } else {
                ScaleVector3ByRatio(o272, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, o272);
              TurnActorYawTowardAngle(actor, ArcTan2(o272[0], o272[1], 0), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(o272, VEC_LEN_XY), o272[2], 0);
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

    

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_REBOUND_FX:      if (g_nActorAnimDone != 0) {
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
          SubtractVector(o288, at, &spy->posX);
          o288[2] = (o288[2] * 3) >> 2;

          if (VectorLength(o288, VEC_LEN_XYZ) < 0x440) {
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
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000 &&
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
      SubtractVector(o288, &spy->posX, &actor->posX);
      distance = VectorLength(o288, VEC_LEN_XY);
      if (distance < st->radius && o288[2] >= -0x3FF && o288[2] < st->spanZ &&
          (SPY.state == SPYRO_ST_LIFT || SPY.posZ + 0xC00 < actor->posZ + st->spanZ)) {
        spyro = &SPY;
        spyro->requestMask = SPYRO_RQ_PENDING;
        SPY.pScriptedActor = actor;
        SPY.triggerEvents |= SPYRO_EV_LIFT;
        SPY.requestTargetZ = actor->posZ + st->spanZ;
        CopyVector(spy->firstContactPoint, &actor->posX);
        if (st->flags & WHIRL_F_TARGET_CAM) {
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

        o304[0] = (GetRandomU32() & 0xFE) - 0x7F;
        o304[1] = (GetRandomU32() & 0xFE) - 0x7F;
        o304[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(o304, o304, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, o304, (int *)1);
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
            o320[0] = (GetRandomU32() & 0xFE) - 0x7F;
            o320[1] = (GetRandomU32() & 0xFE) - 0x7F;
            o320[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(o320, o320, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, o320, (int *)1);
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
          ZeroVector(o336);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, o336);
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
            CopyVector(o352, &st->prize->posX);
            o352[0] += g_anCosineLut[j * 43] >> 6;
            o352[1] += g_anSineLut[j * 43] >> 6;
            o352[2] += 0x28;
            o368[0] = g_anCosineLut[j * 43] >> 7;
            o368[1] = g_anSineLut[j * 43] >> 7;
            o368[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, o352, o368);
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
        if (ApproxDist2D(&actor->posX, &SPY.posX) > 0xA00) {
          actor->mode = CHECKPT_ARMED;
        }
        break;

      case CHECKPT_ARMED:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x380) {
          int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

          if (dz > 0 ? dz < 0x200
                     : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x200) {
            SetSpawnAnchorFromActor(actor, actor->rotZ);
            actor->mode = CHECKPT_FLASH;
            actor->anim = 1;
            actor->subMode = 0;
          }
        }
        break;

      case CHECKPT_FLASH:
        actor->subMode += g_nFrameStep;
        if (actor->subMode >= 0x30) {
          actor->mode = CHECKPT_REARM;
          actor->anim = 0;
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
        actor->cullRadius = 0;
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

        actor->cullRadius = 0x20;
        st->t += g_nFrameStep;
        k = (st->t << 5) & 0x3FF;
        GetActorAttachPoint(st->owner, 0, a);
        GetActorAttachPoint(st->owner, 1, b);
        SubtractVector(c, b, a);
        actor->rotY = ArcTan2(VectorLength(c, VEC_LEN_XY), c[2], 0);
        actor->rotZ = ArcTan2(c[0], c[1], 0);
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
    case ACTOR_TYPE_HAZARD2_DEATH_FX:      if (g_nActorAnimDone != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      break;

    /* Type 390: spinning chest. Each charge spins it faster (with a rising
     * voice); spun fast enough, or flamed, it bursts. */
    case ACTOR_TYPE_SPIN_CHEST: {
      SpinChestState *st = actor->state;
      int pitch = SAMPLE_PITCH(g_apActorMeshTable[actor->type]->m_Sounds[1]);
      int cur = g_anSpuVoiceRowPitch[actor->voiceRow & 0x7F][0];
      int i;

      if ((actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER)) == ACTOR_HIT_FLAME) {
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
          SubtractVector(o400, &a->posX, &actor->posX);
          d = ABS2(o400[0]) + ABS2(o400[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(o400, VEC_LEN_XY) - 0x1000) <= 0x2000 &&
              (unsigned int)(o400[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, 1),
                              ArcTan2(o400[0], o400[1], 1));
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

    /* Type 402: cannon. Smokes while it builds up, fires, then flies its
     * ball along a path to the target, waking the linked actor. */
    case ACTOR_TYPE_CANNON: {
      CannonState *st = actor->state;

      if (st->init == 1) {
        int pitch;

        SubtractVector(o416, PATH_PT(st->path, 1), PATH_PT(st->path, 0));
        pitch = ArcTan2(VectorLength(o416, VEC_LEN_XY), o416[2], 0);
        actor->rotY = pitch - 0x40;
        if (((unsigned int)(pitch + 0x40) & 0xFF) < 0x60) {
          actor->rotY = 0xE0;
        }
        actor->rotZ = ArcTan2(o416[0], o416[1], 0);
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
        actor->updateDist = 0xFF;
        actor->cullRadius = 0x7F;
        continue;

      case CANNON_FUSE:
        st->timer += g_nFrameStep;
        if (st->timer < 0x5A) {
          o432[0] = 0;
          o432[1] = 0;
          o432[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, o432, o432);
          AddVector(o432, o432, &actor->posX);
          o448[0] = 0;
          o448[1] = 6;
          o448[2] = -0xC;
          RotateVectorByMatrix(actor->mtx, o448, o448);
          o448[0] += (GetRandomU32() & 0xF) - 8;
          o448[1] += (GetRandomU32() & 0xF) - 8;
          g_pfnLevelEmitParticles(1, EMIT_YELLOW_DUST, o432, o448);
          if (actor->voiceRow != VOICE_ROW_NONE) {
            continue;
          }
          PlayActorMeshVoice(actor, 0, 0);
          continue;
        }
        if (st->timer < 0xB4 &&
            (st->timer & 3) <= ((st->timer + g_nFrameStep) & 3)) {
          o432[0] = 0;
          o432[1] = 0;
          o432[2] = 0x14C;
          RotateVectorByMatrix(actor->mtx, o432, o432);
          AddVector(o432, o432, &actor->posX);
          o448[0] = 0;
          o448[1] = ((st->timer - 0x5A) >> 3) + 9;
          o448[2] = -0x12 - ((st->timer - 0x5A) >> 2);
          RotateVectorByMatrix(actor->mtx, o448, o448);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, o432, o448);
          if (actor->rotX < 0x80) {
            actor->rotX = -((st->timer - 0x5A) >> 5);
          } else {
            actor->rotX = (st->timer - 0x5A) >> 5;
          }
          continue;
        }
        if (st->timer <= 0xB4) {
          continue;
        }
        actor->subMode = CANNON_FLIGHT;
        actor->cullRadius = 0x40;
        actor->updateDist = 0x40;
        st->path[1] = 1;
        st->timer = 0;
        ZeroVector(st->vel);
        PlayActorMeshVoice(actor, 1, 0);
        continue;

      case CANNON_FLIGHT:
        if (st->timer == 4) {
          SubtractVector(o464, PATH_PT(st->path, st->path[1]), &actor->posX);
          CopyVector(st->vel, o464);
          st->path[1]++;
          if (st->path[1] == st->path[0]) {
            Actor *target = &g_pActorListBase[st->link];

            if (st->link > 0) {
              target->updateDist = 0xFF;
              target->flags |= ACTOR_HIT_SUPER;
            }
            DespawnActorRecord(actor);
          }
          st->timer = 0;
        } else {
          if (st->timer == 3) {
            SubtractVector(o464, PATH_PT(st->path, st->path[1]), &actor->posX);
            RShiftVector3(o464, 1);
          } else {
            SubtractVector(o464, PATH_PT(st->path, st->path[1]),
                          (int *)(st->path + ((st->path[1] << 4) - 8)));
            ScaleVector3Sat(o464, o464, g_anSineLut[st->timer * 2 - 6]);
            ScaleVector3Sat(o480, st->vel, g_anSineLut[st->timer * 2 - 5]);
            AddVector(o464, o464, o480);
            RShiftVector3(o464, 0xA);
          }
          st->timer++;
        }
        VectorLength(o464, VEC_LEN_XYZ);
        actor->rotZ = ArcTan2_8bit(o464[0], o464[1]);
        actor->rotY = -ArcTan2_8bit(o464[2], VectorLength(o464, VEC_LEN_XY));
        AddVector(&actor->posX, &actor->posX, o464);
        ZeroVector(o464);
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, o464);
        continue;
      }
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
        ((CollectibleState *)rider->state)->mode = one;
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
        o480[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        o480[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        o480[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, o480);
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
          o480[0] = (g_anCosineLut[bearing] * 3) >> 4;
          o480[1] = (g_anSineLut[bearing] * 3) >> 4;
          o480[2] = 0;
          o496[0] = (unsigned short)g_anCosineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          o496[1] = (unsigned short)g_anSineLut[(bearing + 0x40) & 0xFF] << 16 >> 21;
          o496[2] = 0;
          bearing = (bearing + 0x80) & 0xFF;
          actor->rotZ = bearing + ((g_anCosineLut[actor->subMode] * 3) >> 9);
          ScaleVector3Sat(o512, o496, st->radius - 1);
          LShiftVector3(o496, 1);
          arm = st->radius - 1;
          o512[2] += (g_anCosineLut[arm * 2] * 3) >> 3;
          o528[0] = o480[0] * g_anCosineLut[arm * 2];
          o528[1] = o480[1] * g_anCosineLut[arm * 2];
          o528[2] = 0;
          bearing = arm << 1;
          bearing = (bearing - (st->phase * 4)) & 0xFF;
          ScaleVector3Sat(o496, o496, st->phase);
          SubtractVector(o512, o512, o496);
          swing = &g_anCosineLut[bearing];
          actor->posX = o480[0] * *swing;
          actor->posY = o480[1] * *swing;
          actor->posZ = 0;
          SubtractVector(&actor->posX, &actor->posX, o528);
          RShiftVector3(&actor->posX, 0xA);
          AddVector(&actor->posX, &actor->posX, o480);
          AddVector(&actor->posX, &actor->posX, &st->parent->posX);
          SubtractVector(&actor->posX, &actor->posX, o512);
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

      if ((actor->flags & ACTOR_HITS_SPYRO) && (unsigned int)(actor->mode - 3) >= 2) {
        actor->flags = 0;
        st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                actor->posY - spy->posY, 0),
                                  spy->bodyRotZ, 0x20, 0x40);
        st->lift = 0x46;
        st->life = 0x32;
        if (actor->flags & ACTOR_HIT_FLAME) {
          st->speed = 0x96;
        } else {
          st->speed = 0x12C;
          st->lift = 0x82;
        }
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        PlayActorMeshVoice(actor, 0, 0);
        ENTER_POSE(actor, WARDRUM_KNOCKED);
      }
      if (TickCountdownByFrameDelta(&st->beat, 4) == 2) {
        st->fade = 0x14;
        st->wait = st->reload;
        StopSoundVoicesByOwner(actor, VOICE_STOP_ALL);
      }
      if (st->beat == 0 && CHUNK_ANIM_STEP(GetWorldChunkAnimState(st->chunkAnim)) < 2) {
        g_anLevelTriggerStateSlots[st->chunkAnim] = 0;
      }
      if (TickCountdownByFrameDelta(&st->fade, 4) == 0) {
        ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_KEEP(CHUNK_ANIM_BRANCH), CHUNK_ANIM_BRANCH);
      }

      switch (actor->mode) {
      case WARDRUM_WAIT:
        if (TickCountdownByFrameDelta(&st->wait, 4) == 2) {
          ENTER_POSE(actor, WARDRUM_BEAT);
        }
        break;

      case WARDRUM_BEAT:
        if (actor->nextFrame >= 6 && st->beat == 0) {
          ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          st->beat = 0x64;
          g_anLevelTriggerStateSlots[st->chunkAnim] = 1;
          PlaySoundEffect(g_apActorMeshTable[actor->type]->m_Sounds[1], actor, SFX_MODE_3D,
                        &st->voice);
        }
        if (g_nActorAnimDone != 0) {
          ENTER_POSE(actor, WARDRUM_WAIT);
        }
        break;

      case WARDRUM_KNOCKED:
        actor->rotY += g_nFrameStep * 2;
        if (TickCountdownByFrameDelta(&st->life, 4) != 0 ||
            DriveActorTrackedMove(actor, &st->speed, st->knock, &st->lift, 0xC, 0xC) ==
                3) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          actor->cullRadius = 0;
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
        actor->mode = RAM_KNOCKED;
        continue;
      }
      actor->flags = 0;
      if (st->triggerSlot != -1 && g_anLevelTriggerStateSlots[st->triggerSlot] != 0 &&
          (unsigned int)(actor->mode - 3) >= 2 &&
          IsPointInRotatedBox(&actor->posX, &st->box.x) != 0) {
        ENTER_POSE_B(actor, RAM_TRAPPED);
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
            ENTER_POSE(actor, RAM_WANDER);
          }
        }
        break;

      case RAM_WANDER: {
        int ang =
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, 0);

        if (TickCountdownByFrameDelta(&st->call, 4) != 0) {
          st->call = (GetRandomU32() & 0x1F) + 0x50;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 3, 0);
        }
        if (TurnActorYawTowardAngle(actor, ang, 6, 0x14, 1) != 0) {
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

    case ACTOR_TYPE_BOLT_SENTRY: {
      BoltSentryState *st = actor->state;
      int i;

      if (actor->flags & (ACTOR_HIT_FLAME | ACTOR_HIT_SUPER) && ApproxDist2D(&actor->posX, spy) < 0xE10 &&
          actor->mode != BSENTRY_KNOCKED) {
        st->speed = 0xF0;
        st->heading = SPY.bodyRotZ;
        actor->flags = 0;
        SpawnActorDeathDrop(actor, DROP_SCATTER, 0, 0);
        MarkActorCommittedKill(actor);
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
        if (st->mode != 0) {
          ENTER_POSE(actor, BSENTRY_CUE);
        }
        if (st->guards == 0 &&
            ApproxDist2D(&actor->posX, &SPY.posX) < st->range << 10) {
          ENTER_POSE(actor, BSENTRY_SUMMON);
        }
        ANIM_GO(actor, 0);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, spy) < 0x1C00 &&
            ABS2(actor->posZ - SPY.posZ) < 0x44C) {
          ENTER_POSE_BRK(actor, BSENTRY_ROUSE);
        }
        break;

      case BSENTRY_PATROL:
        if (PivotActorAlongPath(actor, st->path, 0x100, 0x6E, 0, 8, 0x28, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR) ==
            0x100) {
          if (ApproxDist2D(&actor->posX, spy) < 0x1C00 &&
              ABS2(actor->posZ - SPY.posZ) < 0x44C) {
            ENTER_POSE_BRK(actor, BSENTRY_ROUSE);
          }
          ENTER_POSE_BRK(actor, BSENTRY_IDLE);
        }
        if (ApproxDist2D(&actor->posX, spy) < 0x1194 &&
            AbsAngleDelta8(actor->rotZ,
                          ArcTan2(spy->posX - actor->posX,
                                        g_nSpyroWorldPosY - actor->posY, 0)) < 0x40) {
          ENTER_POSE_BRK_B(actor, BSENTRY_THROW);
        }
        break;

      case BSENTRY_ROUSE:
        if (g_nActorAnimDone != 0) {
          actor->mode = BSENTRY_WATCH;
          ANIM_RESET(actor, 3);
        }
        break;

      case BSENTRY_WATCH: {
        int d = ApproxDist2D(&actor->posX, spy);

        if (st->guards == 0 &&
            ApproxDist2D(&actor->posX, spy) < st->range << 10) {
          ENTER_POSE(actor, BSENTRY_SUMMON);
        }
        TrackEntityYawTowardSpyro(actor, 3, 0, 0);
        if (TickCountdownByFrameDelta(&st->timer, 4) != 0 && d < 0xFA0 &&
            ABS2(actor->posZ - SPY.posZ) < 0x44C) {
          ENTER_POSE_BRK_B(actor, BSENTRY_THROW);
        }
        if (d > 0x2000) {
          ENTER_POSE_BRK(actor, BSENTRY_LOWER);
        }
        break;
      }

      case BSENTRY_THROW:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (st->fx == 0 && actor->nextFrame - 2U < 0x1A) {
          st->fx = g_pfnLevelSpawnActor(ACTOR_TYPE_CARRIED_EGG, actor);
        }
        if (st->bolt == 0 && actor->nextFrame - 0xFU < 3 &&
            ApproxDist2D(&actor->posX, spy) < 0x1194) {
          AimedBoltState *bs;

          st->bolt = g_pfnLevelSpawnActor(ACTOR_TYPE_AIMED_BOLT, actor);
          st->bolt->rotZ = actor->rotZ;
          bs = st->bolt->state;
          bs->mode = 0;
          g_dwSpyroTriggerEventFlags = SPY.triggerEvents | 0x26;
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          GetActorAttachPoint(actor, 0, &st->bolt->posX);
          bs->parent = actor;
        }
        if (actor->nextFrame >= 0x1C && st->bolt != 0) {
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
          st->timer = 3;
        }
        if (g_nActorAnimDone != 0) {
          if (st->timer != 0) {
            actor->mode = BSENTRY_RECOVER;
            ANIM_RESET(actor, 7);
            break;
          }
          st->timer = 0x3C;
          actor->mode = BSENTRY_IDLE;
          ANIM_RESET(actor, 0);
        }
        break;

      case BSENTRY_LOWER:
        if (g_nActorAnimDone != 0) {
          actor->mode = BSENTRY_IDLE;
          ANIM_RESET(actor, 0);
        }
        break;

      case BSENTRY_KNOCKED:
        if (st->bolt != 0) {
          st->bolt->subMode = 1;
          st->bolt = 0;
        }
        if (st->speed > 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0x1F4, 0x2BC, one);
          st->speed -= 0x10;
        }
        if (g_nActorAnimDone != 0) {
          for (i = 0; i < 3; i++) {
            if (st->kids[i] != 0) {
              st->kids[i]->mode = BEAD_DESPAWN;
            }
          }
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x34);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case BSENTRY_RECOVER:
        if (g_nActorAnimDone != 0 && TickCountdownByFrameDelta(&st->timer, 4) != 0) {
          ENTER_POSE_BRK(actor, BSENTRY_IDLE);
        }
        break;

      case BSENTRY_CUE:
        switch (st->mode) {
        case 1:
          if (ApproxDist2D(&actor->posX, spy) < st->range << 10 &&
              ABS2(SPY_DZ(actor)) < 0x3E8) {
            st->mode = 0;
            st->speed = ApproxDist2D(&actor->posX, &st->path[6]) / 12;
            st->u24 = 0x6E;
            ENTER_POSE_BRK(actor, BSENTRY_LEAP);
          }
          break;
        case 2:
          if (ApproxDist2D(&actor->posX, spy) < 0x3000 &&
              AbsAngleDelta8(actor->rotZ,
                            ArcTan2(spy->posX - actor->posX,
                                          g_nSpyroWorldPosY - actor->posY, 0)) < 0x40) {
            st->mode = 0;
            ENTER_POSE_BRK(actor, BSENTRY_PATROL);
          }
          break;
        }
        break;

      case BSENTRY_LEAP: {
        int h = ArcTan2(st->path[6] - actor->posX,
                              st->path[7] - actor->posY, 0);
        int d = ApproxDist2D(&actor->posX, &st->path[6]);

        ProbeActorGroundHeight(actor);
        TrackEntityYawTowardSpyro(actor, 3, 0, 0);
        if (g_nActorAnimDone != 0) {
          if (st->guards == 0) {
            ANIM_QUEUE(actor, BSENTRY_SUMMON);
          }
          ANIM_QUEUE(actor, BSENTRY_IDLE);
        }
        if (actor->nextFrame - 8U < 7 && st->speed < d) {
          MoveActorAlongAngle(actor, h, st->speed, 0, 0, 0);
        }
        break;
      }

      case BSENTRY_SUMMON:
        if (actor->nextFrame >= 4) {
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
      if (st->mode == 0) {
        int *v = o528;
        int k;

        RefreshActorDerivedState(st->parent, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(st->parent, 0, &actor->posX);
        actor->rotZ = ArcTan2(spy->posX - actor->posX,
                                     spy->posY - actor->posY, 0);
        actor->rotY = ArcTan2(ApproxDist2D(&actor->posX, spy),
                                     SPY.posZ - actor->posZ, 0);
        SubtractVector(v, &spy->posX, &actor->posX);
        k = 0x14 - VectorLength(v, VEC_LEN_XYZ) * 0x1A / 1400;
        if (k < -0x14) {
          k = -0x14;
        }
        actor->scale = k + 0x20;
        continue;
      }
      if (st->mode == 2) {
        int t = (ArcTan2(st->target[0] - actor->posX,
                               st->target[1] - actor->posY, 0) -
                 actor->rotZ) &
                0xFF;

        if (t > 0x80) {
          t -= 0x100;
        }
        if (t > 2) {
          t = 2;
        }
        if (t < -2) {
          t = -2;
        }
        actor->rotZ += t;
        t = (ArcTan2(ApproxDist2D(&actor->posX, st->target),
                           st->target[2] - actor->posZ, 0) -
             actor->rotY) &
            0xFF;
        if (t > 0x80) {
          t -= 0x100;
        }
        if (t > 2) {
          t = 2;
        }
        if (t < -2) {
          t = -2;
        }
        actor->rotY += t;
      }
      CopyVector(o480, &actor->posX);
      if (st->mode == one && RaiseSpyroTriggerInRadius(&actor->posX, 0xFA, SPYRO_EV_HIT_TINT | SPYRO_EV_CHARGE_HIT) != 0) {
        g_anSpyroRequestVelocity.x = (short)((unsigned short *)g_anCosineLut)[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityY = (short)((unsigned short *)g_anSineLut)[actor->rotZ] >> 5;
        g_nSpyroRequestVelocityZ = 0;
        DespawnActorRecord(actor);
        continue;
      }
      if (st->mode == 2 &&
          DispatchActorContactAtSphere(&actor->posX, 0x15E, 0, ACTOR_HIT_BLAST, actor, 0) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TickCountdownByFrameDelta(&st->timer, 2) != 0) {
        DespawnActorRecord(actor);
        continue;
      }
      if (TryMoveActorForward(actor, 0x15E, 0x96, 0, 0) != 0) {
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
