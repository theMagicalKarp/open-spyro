/* Level12_UpdateActors -- per-frame actor update for level 12 (Magic
 * Crafters home).
 *
 * Walks the list of actors due for an update this frame and runs each one's
 * behaviour. Each `case` of the dispatch switch is one actor class: a small
 * state machine over the actor's state byte (->mode), often with a sub-state
 * (->subMode), and usually a class-specific state block behind ->state.
 *
 * Most classes are shared with the other levels' actor updates (levels 0, 1,
 * 8, 9, 15, 25, 26 and 31); the level-12 classes are the floor-snapped marker
 * (328), the talking dragon variant (189), the villager (270) and the
 * shepherd (271).
 *
 * Load-bearing source forms (matched 2026-10-01):
 *   - Level 8's loop top with spy carried from g_anSpyroWorldPos (level 10's form);
 *     case 1 stores `oneq` so the byte `= 1` sites stay literal.
 *   - Twenty-four scratch arrays in the original frame order; case 271's
 *     index slot is `int[2]` so it is laid out at its declaration.
 *   - Case 270 declares its spilled pairGone before its voice locals, and
 *     reads the kill bitmap through idx/word/bit temps (word before bit).
 *   - Case 270 state 0x5B makes the distance call before computing the
 *     flee range: the range held across the call would take s0 from st.
 *   - Case 270's camera grab stores g_pSpyroRequestCameraAnchor/C78 first and uses the
 *     absolute symbols (g_dwSpyroRequestMask, g_bSpyroPersistentEulerZ) so the bodyRotZ load can be
 *     scheduled to the top of the block.
 *   - Case 271 keeps 0x4B0 in a local (`near`), steers with unsigned table
 *     reads (`<< 16 >> 19`), and spells the camera look x as `<< 7 >> 12`.
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
#include "level_id.h"
#include "emit_record.h"
#include "cd_stream.h"
#include "actor_cutscene.h"


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


/* Type 270: villager. Chats when Spyro faces it up close (mode 0), turns to
 * face a friend (mode 1), waits on a partner (mode 2) or watches a leader
 * (mode 3); knocked back when hit, and runs off when Spyro gets close. */


/* ==== The function ==== */
void Level12_UpdateActors(void) {
  Actor **actorList;
  Actor *actor;
  SpyroObj *spy;
  int one;
  short oneq;
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
  int svR[3];
  int svS[3];
  int tgt271[3];
  int idx271[2];
  int svT[3];
  int mat2[3][3];
  int x352[3];
  int x368[3];
  int col2[3];
  int svN[3];
  int svO[3];
  int svP[3];
  int svQ[3];

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
    oneq = 1;
    one = 1;
    spy = (SpyroObj *)&g_anSpyroWorldPos;

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
      int shouldSpawnText, cameraAngleDiff;
      int distance = ApproxDist2D(&actor->posX, &CAM.posX);

      if (distance < (actor->mode ? 12 : 11) * 1024) {
        shouldSpawnText = actor->mode == PORTALNAME_FAR;

        actor->mode = oneq;

        cameraAngleDiff = AbsAngleDelta8(
            actor->rotZ, ArcTan2(CAM.posX - actor->posX,
                                        CAM.posY - actor->posY, ATAN_ANGLE8));

        if (cameraAngleDiff <= 56) {
          if (actor->subMode == NAMESIDE_BACK) {
            shouldSpawnText = 1;
          }
          actor->subMode = NAMESIDE_FRONT;
        } else if (cameraAngleDiff > 71) {
          if (actor->subMode == NAMESIDE_FRONT) {
            shouldSpawnText = 1;
          }
          actor->subMode = one;
        } else {
          actor->mode = PORTALNAME_FAR;
        }

        if (actor->mode && shouldSpawnText) {
          SpawnNameLetterActors(actor, 1);
        }
      } else {
        actor->mode = PORTALNAME_FAR;
      }

      break;
    }

    /* Particle emitter on a random timer. */
    case ACTOR_TYPE_TIMED_EMITTER: {
      int *st;

      st = (int *)actor->state;
      if (TickCountdownByFrameDelta(st, 4) == 0) {
        break;
      }
      g_pfnLevelEmitParticles(1, EMIT_DRIFTING_MOTE, &actor->posX, 0);
      *st = GetRandomU32() & 0xE;
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

          rotated = TurnActorYawTowardAngle(actor, st->heading, 4, 0xA, 1);
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

    /* Egg thief (ThiefState). */
    case ACTOR_TYPE_EGG_THIEF: {
      ThiefState *st;

      EBB_BREAK();
      st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != THIEF_CAUGHT) {
        st->knock = 0x96;
        if ((actor->flags & ACTOR_HIT_CHARGE) != 0) {
          st->knock = 0xE1;
        }
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, 0x20, 0x40);
        if (actor->dropKind == 0xFF) {
          actor->dropKind = 0xF;
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorCommittedKill(actor);
        }
        actor->mode = THIEF_CAUGHT;
        continue;
      }

      if (actor->dropKind != 0xFF && st->egg == 0 && actor->mode != THIEF_CAUGHT) {
        st->egg = g_pfnLevelSpawnActor(ACTOR_TYPE_DRAGON_EGG, actor);
        st->egg->otBias = 0;
        ((EggRiseState *)st->egg->state)->owner = actor;
      }

      if (TickCountdownByFrameDelta(&st->voice, 4) != 0 &&
          (actor->mode == THIEF_RUN || actor->mode == THIEF_CLIMB_RUN)) {
        int k;

        st->voice = (int)GetRandomU32() % 3 + 4;
        k = (int)GetRandomU32() % 5;
        while (k == st->lastVoice) {
          k = (int)GetRandomU32() % 5;
        }
        PlayActorMeshVoice(actor, k, 0);
        st->lastVoice = k;
      }

      if (actor->nextAnim == 4) {
        if (actor->nextFrame == 2 && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[6]) == 0) {
          PlayActorMeshVoice(actor, 6, 0);
        }
        if (actor->nextFrame == 0xC && (GetRandomU32() & 3) == 0 &&
            GetSoundVoiceStatusByOwner(actor, g_apActorMeshTable[actor->type]->m_Sounds[5]) == 0) {
          PlayActorMeshVoice(actor, 5, 0);
        }
      }

      switch (actor->mode) {
      case THIEF_IDLE:
        actor->otBias = 2;
        if (st->mode == THIEFKIND_HOP) {
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
            st->landed = 0;
            ANIM_GO(actor, 1);
            actor->mode = THIEF_RUN;
            continue;
          }
        } else if (st->mode == THIEFKIND_GUARD) {
          int spd = 0x154;

          spd -= ApproxDist2D(&actor->posX, &spy->posX) >> 5;
          if (spd < 0x8C) {
            spd = 0x8C;
          }
          if (spd > 0xF0) {
            spd = 0xF0;
          }
          if (actor->subMode == THIEFSUB_ORBIT) {
            int home = ArcTan2(st->path->nodes[0].x - actor->posX,
                                     st->path->nodes[0].y - actor->posY, ATAN_ANGLE8);

            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) < 0x1000) {
              TurnActorYawTowardAngle(actor,
                            ArcTan2(actor->posX - spy->posX,
                                          actor->posY - SPY.posY, ATAN_ANGLE8),
                            8, 0, 0);
              MoveActorAlongHeading(actor, spd, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            } else {
              int lim = 10;
              int r;
              int a = ArcTan2(
                  st->path->nodes[st->path->cur].x - spy->posX,
                  st->path->nodes[st->path->cur].y - SPY.posY, ATAN_ANGLE8);
              int b = ArcTan2(actor->posX - st->path->nodes[0].x,
                                    actor->posY - st->path->nodes[0].y, ATAN_ANGLE8);

              if (actor->nextAnim == 4) {
                lim = 0x28;
              }
              if (AbsAngleDelta8(a, b) > lim &&
                  ApproxDist2D(&actor->posX, &spy->posX) < 0x3000) {
                r = SteerActorAroundPoint(actor, &st->path->nodes[0].x, 0x1800, a, 5,
                                  spd, 0xE, 0x14, 0xFF, 0xFF, 0, 0, MOVE_FOLLOW_FLOOR);
              }
              if (r > 0x5A) {
                st->stuck++;
              } else {
                st->stuck = 0;
              }
              if (r < 0x1E) {
                TrackEntityYawTowardSpyro(actor, 6, 0, 0);
                ANIM_GO(actor, 4);
              } else {
                ANIM_GO(actor, 1);
              }
              if (st->stuck >= 0x1E &&
                  AbsAngleDelta8(home, ArcTan2(spy->posX - actor->posX,
                                                    spy->posY - actor->posY,
                                                    ATAN_ANGLE8)) > 0x20) {
                actor->subMode = THIEFSUB_BREAK;
              }
            }
          } else {
            int a = ArcTan2(st->path->nodes[0].x - spy->posX,
                                  st->path->nodes[0].y - SPY.posY, ATAN_ANGLE8);
            int b = ArcTan2(st->path->nodes[0].x - actor->posX,
                                  st->path->nodes[0].y - actor->posY, ATAN_ANGLE8);

            TurnActorYawTowardAngle(actor, a, 8, 0, 0);
            MoveActorAlongHeading(actor, spd, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (AbsAngleDelta8(a, b) > 0x50) {
              actor->subMode = THIEFSUB_ORBIT;
            }
            if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x1800) {
              actor->subMode = THIEFSUB_ORBIT;
            }
          }
        } else if (st->mode == THIEFKIND_LEDGE) {
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
            actor->mode = THIEF_LEDGE_RESET;
            continue;
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1C00) {
            actor->mode = THIEF_LEDGE_RUN;
            continue;
          }
        } else if (st->mode == THIEFKIND_FLEE) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
              ABS2(actor->posZ - actor->floorOffset - SPY.posZ) < 0x708 &&
              (st->needLos == 0 || CheckSpyroLineOfSightTo(&actor->posX) != 0)) {
            st->node = FindFarthestPathPointFromSpyro(st->path);
            st->speed = 0;
            if (st->node - st->path->reversed != st->path->cur) {
              st->turned = 0;
              if (ApproxDist2D(&actor->posX,
                                &st->path->nodes[st->path->cur].x) < 0x100) {
                st->path->cur = (st->path->cur + st->path->reversed +
                                 st->path->count) %
                                st->path->count;
              }
              actor->mode = THIEF_FLEE;
              continue;
            }
            ANIM_GO(actor, 4);
            TrackEntityYawTowardSpyro(actor, 6, 0, 0);
            break;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == THIEFKIND_CLIMB) {
          if (IsPointInRotatedBox(&SPY.posX, st->area) != 0) {
            actor->mode = THIEF_CLIMB_RUN;
            continue;
          }
          ANIM_GO(actor, 4);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        } else if (st->mode == THIEFKIND_CIRCLE) {
          if (st->dist == 0) {
            st->dist = ApproxDist2D(&actor->posX,
                                     &st->path->nodes[st->path->cur].x);
          }
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
            int d = ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                  SPY.posY - st->path->nodes[st->path->cur].y, ATAN_ANGLE8);
            int b = ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                  actor->posY - st->path->nodes[st->path->cur].y,
                                  ATAN_ANGLE8) -
                    0x80;

            d = (d - b) & 0xFF;

            if (d > 0x80) {
              d -= 0x100;
            }
            if (ABS(d) > 0x30) {
              actor->mode = THIEF_CIRCLE;
              continue;
            }
          }
        } else if (st->mode == THIEFKIND_STAND) {
          actor->mode = THIEF_STAND;
          continue;
        }
        break;

      case THIEF_RUN: {
        int spd = 0x154;

        spd -= ApproxDist2D(&actor->posX, &SPY.posX) >> 5;
        if (spd < 0x46) {
          spd = 0x46;
        }
        if (spd > 0xDC) {
          spd = 0xDC;
        }
        if (PivotActorAlongPath(actor, st->path, 0x73A, spd, 0xC8, 0xA, 0x80,
                          actor->group, 0) == 0x104) {
          st->vz = 0x12C;
          actor->mode = THIEF_LEAP;
          continue;
        }
        break;
      }

      case THIEF_LEAP:
        actor->otBias = 0;
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, 0);
        MoveActorAlongHeading(actor, 0x96, 0, 0, MOVE_LIFT);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x) <
            0x100) {
          actor->mode = THIEF_PERCH;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        break;

      case THIEF_PERCH: {
        int g = ProbeActorGroundHeight(actor);

        ANIM_GO(actor, 4);
        if (st->landed == 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->landed = 1;
            break;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        } else {
          int k;
          int h;

          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (ApproxDist2D(&spy->posX, &st->path->nodes[4].x) <
              ApproxDist2D(&spy->posX, &st->path->nodes[5].x)) {
            k = 4;
          } else {
            k = 5;
          }
          h = ArcTan2(st->path->nodes[k].x - actor->posX,
                            st->path->nodes[k].y - actor->posY, ATAN_ANGLE8);
          if (ApproxDist2D(&actor->posX, &st->path->nodes[k].x) > 0x100) {
            if (TurnActorYawTowardAngle(actor, h, 8, 0x14, 1) != 0) {
              MoveActorAlongHeading(actor, 0x96, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            }
          } else if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x800 ||
                     ApproxDist2D(&spy->posX, &st->path->nodes[7].x) < 0x800) {
            st->vz = 0x96;
            actor->mode = THIEF_LEAP_HOME;
            continue;
          }
        }
        break;
      }

      case THIEF_LEAP_HOME: {
        int g;

        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[0].x - actor->posX,
                                    st->path->nodes[0].y - actor->posY, ATAN_ANGLE8),
                      6, 0, 0);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[0].x) > 0x100) {
          MoveActorAlongHeading(actor, 0x82, 0, 0, MOVE_LIFT);
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        g = ProbeActorGroundHeight(actor);
        if (st->vz < 0) {
          int z = actor->posZ + st->vz;

          if (g >= z) {
            actor->posZ = g;
            st->path->cur = 0;
            ANIM_GO(actor, 0);
            actor->mode = THIEF_IDLE;
            continue;
          }
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        }
        break;
      }

      case THIEF_CAUGHT:
        if (st->egg != 0) {
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
          if (actor->nextAnim == 3 && actor->nextFrame >= 5) {
            st->egg->mode = EGG_RELEASED;
            st->egg->otBias = 4;
            st->egg = 0;
          }
        }
        if (st->knock != 0) {
          MoveActorAlongAngle(actor, st->heading, st->knock, 0, 0x12C, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          st->knock -= 0x10;
          if (st->knock < 0) {
            st->knock = 0;
          }
        }
        ANIM_GO(actor, 3);
        if (g_nActorAnimDone == 0) {
          break;
        }
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        BurstActorWithSound(actor, 0x18);
        DespawnActorRecord(actor);
        continue;

      case THIEF_LEDGE_RUN:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xF0, 0, 8, sixteen, 0xFF,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == 0x103) {
          actor->mode = THIEF_LEDGE_WAIT;
          continue;
        }
        break;

      case THIEF_LEDGE_WAIT:
        if (ApproxDist2D(&spy->posX, &st->path->nodes[6].x) < 0x1800) {
          actor->mode = THIEF_LEDGE_RESET;
          continue;
        }
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          actor->mode = THIEF_LEDGE_RUN_ON;
          continue;
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        ANIM_GO(actor, 4);
        break;

      case THIEF_LEDGE_RUN_ON:
        ANIM_GO(actor, 1);
        if (PivotActorAlongPath(actor, st->path, 0x100, 0xDA, 0, 0xA, 0x30, 0xFF,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == 0x101) {
          actor->mode = THIEF_LEDGE_TAUNT;
          continue;
        }
        break;

      case THIEF_LEDGE_TAUNT: {
        int d = ApproxDist2D(&actor->posX, &spy->posX);

        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (d < 0x2000 && SPY.posZ - actor->posZ >= -0xC7) {
          st->vz = 0x96;
          st->landed = 0;
          actor->mode = THIEF_LEDGE_JUMP;
          continue;
        }
        break;
      }

      case THIEF_LEDGE_JUMP: {
        int sp = 1;

        if (st->landed != 0) {
          sp = 5;
        }
        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[2].x - actor->posX,
                                        st->path->nodes[2].y - actor->posY, ATAN_ANGLE8),
                          8, 0x1E, 1) == 0) {
          break;
        }
        MoveActorAlongHeading(actor, 0xFA, 0, 0, sp);
        if (ApproxDist2D(&actor->posX, &st->path->nodes[2].x) < 0x100) {
          st->path->cur = 3;
          actor->mode = THIEF_LEDGE_WAIT;
          continue;
        }
        if (st->landed == 0) {
          int g = ProbeActorGroundHeightRaised(actor, 0x1800);

          actor->posZ += st->vz;
          st->vz -= 0x14;
          if (st->vz < 0 && actor->posZ - g < ABS(st->vz)) {
            st->landed = 1;
          }
        }
        break;
      }

      case THIEF_LEDGE_RESET:
        CopyVector(&actor->posX, &st->path->nodes[0].x);
        st->path->cur = 1;
        SnapActorToGround(actor);
        actor->mode = THIEF_LEDGE_TAUNT;
        continue;

      case THIEF_FLEE: {
        int top = 0xCD;
        int next;
        int dCur;
        int dNext;
        int r;

        ANIM_GO(actor, 1);
        if (st->path->nodes[st->path->cur].speed > 0) {
          top = st->path->nodes[st->path->cur].speed;
        }
        if (st->speed < top) {
          st->speed += 0x1E;
          if (top < st->speed) {
            st->speed = top;
          }
        } else if (top < st->speed) {
          st->speed -= 0x1E;
          if (st->speed < top) {
            st->speed = top;
          }
        }
        next = (st->path->cur + st->path->reversed + st->path->count) %
               st->path->count;
        dCur = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);
        dNext = ApproxDist2D(&actor->posX, &st->path->nodes[next].x);
        if (dCur > 0x190 && dNext > 0x190) {
          int aCur = ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                   st->path->nodes[st->path->cur].y - actor->posY,
                                   ATAN_ANGLE8);
          int aNext = ArcTan2(st->path->nodes[next].x - actor->posX,
                                    st->path->nodes[next].y - actor->posY, ATAN_ANGLE8);
          int aSpy =
              ArcTan2(spy->posX - actor->posX, spy->posY - actor->posY, ATAN_ANGLE8);
          int dA = AbsAngleDelta8(aCur, aSpy);
          int dB = AbsAngleDelta8(aNext, aSpy);

          if (st->turned == 0 && ABS2(dA - dB) < 0x28) {
            st->turned = 1;
            st->path->reversed = -st->path->reversed;
          } else if (dA < dB && ABS2(dA - dB) >= 0x1D && dB > 0x3C &&
                     AbsAngleDelta8(actor->rotZ,
                                   ArcTan2(spy->posX - actor->posX,
                                                 spy->posY - actor->posY,
                                                 ATAN_ANGLE8)) < 0x3C) {
            st->path->reversed = -st->path->reversed;
            st->path->cur = next;
          }
        }
        r = PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x1E, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
        if (r & 0x100) {
          if ((r & ~0x100) ==
              (st->node - st->path->reversed) % st->path->count) {
            actor->mode = THIEF_IDLE;
            continue;
          }
          st->node = FindFarthestPathPointFromSpyro(st->path);
        }
        break;
      }

      case THIEF_CLIMB_RUN:
        ANIM_GO(actor, 1);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        st->speed = 0xB4;
        if (st->path->nodes[st->path->cur].speed > 0) {
          st->speed = st->path->nodes[st->path->cur].speed;
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == 0x10B) {
          st->vz = 0x96;
          actor->mode = THIEF_CLIMB_HOP;
          continue;
        }
        break;

      case THIEF_CLIMB_HOP: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, MOVE_LIFT);
        }
        if (d < 0x100 && actor->posZ == g) {
          if (++st->path->cur == 0xD) {
            actor->mode = THIEF_CLIMB_TOP;
            continue;
          }
          st->vz = 0x96;
          break;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case THIEF_CLIMB_TOP:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        {
          char *base = (char *)&g_nSpyroWorldPosZ;

          if (*(int *)base - st->path->nodes[10].z < -0x5DC &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x1800) {
            st->vz = 0x1A4;
            st->path->cur = 9;
            actor->mode = THIEF_CLIMB_HOP_DOWN;
            continue;
          }
        }
        break;

      case THIEF_CLIMB_HOP_DOWN: {
        int g = ProbeActorGroundHeight(actor);
        int d = ApproxDist2D(&actor->posX, &st->path->nodes[st->path->cur].x);

        ANIM_GO(actor, 1);
        TurnActorYawTowardAngle(actor,
                      ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                                    st->path->nodes[st->path->cur].y - actor->posY,
                                    ATAN_ANGLE8),
                      6, 0, 0);
        if (d > 0x100) {
          MoveActorAlongHeading(actor, 0x96, 0, 0, MOVE_LIFT);
        }
        if (d < 0x100 && actor->posZ == g) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 8;
          actor->mode = THIEF_CLIMB_RUN_BACK;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        if (st->vz < -0xC8) {
          st->vz = -0xC8;
        }
        if (g >= actor->posZ + st->vz) {
          actor->posZ = g;
        }
        break;
      }

      case THIEF_CLIMB_RUN_BACK:
        ANIM_GO(actor, 1);
        st->speed = 0xC8;
        if (actor->posZ < 0x800) {
          CopyVector(&actor->posX, &st->path->nodes[st->path->cur].x);
          SnapActorToGround(actor);
        }
        if (PivotActorAlongPath(actor, st->path, 0x100, st->speed, 0, 8, 0x20, 0xFF,
                          MOVE_LIFT | MOVE_FOLLOW_FLOOR) == st->path->count + 0xFF) {
          st->path->reversed = -st->path->reversed;
          st->path->cur = 1;
          actor->mode = THIEF_IDLE;
          continue;
        }
        break;

      case THIEF_CIRCLE:
        ANIM_GO(actor, 1);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x2000) {
          int a = (ArcTan2(spy->posX - st->path->nodes[st->path->cur].x,
                                 SPY.posY - st->path->nodes[st->path->cur].y,
                                 ATAN_ANGLE8) +
                   0x80) &
                  0xFF;

          SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist, a, 8,
                        st->speed, 0xF, 0x40, 0xFF, 0xFF, 0, 0, 0);
          a = (a - ArcTan2(actor->posX - st->path->nodes[st->path->cur].x,
                                 actor->posY - st->path->nodes[st->path->cur].y,
                                 ATAN_ANGLE8)) &
              0xFF;
          if (a > 0x80) {
            a -= 0x100;
          }
          if (ABS(a) < 0x20) {
            ENTER_POSE(actor, THIEF_IDLE);
          }
          break;
        }
        ENTER_POSE(actor, THIEF_IDLE);

      case THIEF_STAND:
        ANIM_GO(actor, 4);
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        break;
      }

      if (st->egg != 0) {
        RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(actor, 0, &st->egg->posX);
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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

    /* Path walker (PathState): waits for Spyro, then paces its path, turning
     * back at either end and choosing the node that leads away from him. */
    case ACTOR_TYPE_PATH_WALKER: {
      PathState *st = actor->state;

      if ((actor->flags & ACTOR_HITS_SPYRO) && actor->mode != PATHWALK_DYING) {
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        actor->mode = PATHWALK_DYING;
        ANIM_RESET(actor, 2);
        continue;
      }

      switch (actor->mode) {
      case PATHWALK_WAIT:
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1800) {
          int height = actor->posZ - actor->floorOffset;
          int drop = height - SPY.posZ;

          if (drop > 0 ? drop < 0x800 : (SPY.posZ - height) < 0x800) {
            ENTER_POSE4(actor, PATHWALK_PACE);
          }
        }
        break;

      case PATHWALK_PACE:
        if (ApproxDist2D(&actor->posX,
                          &st->path->nodes[st->path->cur].x) < 0x400) {
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1801) {
            int height = actor->posZ - actor->floorOffset;
            int drop = height - SPY.posZ;

            if (drop > 0 ? drop >= 0x801 : (SPY.posZ - height) >= 0x801) {
              goto pose0;
            }
            goto walk;
          }
        pose0:
          ENTER_POSE4(actor, PATHWALK_WAIT);
        walk:
          if (st->path->cur == 0 && st->loops == 0) {
            st->path->cur = 1;
          } else if (st->path->cur == st->path->count - 1 && st->loops == 0) {
            st->path->cur--;
          } else {
            int spyroAngle;
            int nextNode;
            int previousNode;
            int nextAngle;
            int previousAngle;

            spyroAngle = ArcTan2(spy->posX - actor->posX,
                                       spy->posY - actor->posY, ATAN_ANGLE8);
            nextNode = (st->path->cur + 1) % st->path->count;
            previousNode =
                (st->path->cur - 1 + st->path->count) % st->path->count;
            nextAngle =
                ArcTan2(st->path->nodes[nextNode].x - actor->posX,
                              st->path->nodes[nextNode].y - actor->posY, ATAN_ANGLE8);
            previousAngle =
                ArcTan2(st->path->nodes[previousNode].x - actor->posX,
                              st->path->nodes[previousNode].y - actor->posY, ATAN_ANGLE8);
            if (AbsAngleDelta8(spyroAngle, nextAngle) >
                AbsAngleDelta8(spyroAngle, previousAngle)) {
              st->path->cur = nextNode;
            } else {
              st->path->cur = previousNode;
            }
          }
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(st->path->nodes[st->path->cur].x -
                                            actor->posX,
                                        st->path->nodes[st->path->cur].y -
                                            actor->posY,
                                        ATAN_ANGLE8),
                          8, 0x20, 1) != 0) {
          MoveActorAlongHeading(actor, 0x60, 0x200, 0x200, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        }
        break;

      case PATHWALK_DYING:
        if (g_nActorAnimDone != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }

      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Split shot: launched at Spyro on the thrower's release frame, it
     * splits into four more of itself when it reaches him, and fizzles out
     * on a timer or against a wall. */
    case ACTOR_TYPE_SPLIT_SHOT: {
      int *st = actor->state;

      switch (actor->mode) {
      case SPLITSHOT_GATHER:
      case SPLITSHOT_RAISE:
      case SPLITSHOT_FRAGMENT:
        if (g_nActorAnimDone != 0 && TickCountdownByFrameDelta(st, 4) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case SPLITSHOT_FLY: {
        int i;
        int a;

        CopyVector(svD, &actor->posX);
        if (actor->posX < 0x400 || actor->posY < 0x400 || actor->posZ < 0x400) {
          DespawnActorRecord(actor);
          continue;
        }
        if (RaiseSpyroTriggerInRadius(&actor->posX, 0x80, SPYRO_EV_HIT_TINT | SPYRO_EV_UNREAD_20000) != 0) {
          i = 0;
          a = GetRandomU32() & 0x3F;
          for (; i < 4; i++, a += 0x40) {
            if (g_nActorPoolCapacity - g_nActorPoolLiveCount < 0x15) {
              break;
            }
            g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor)->rotX = a;
          }
          DespawnActorRecord(actor);
          continue;
        }
        if (TickCountdownByFrameDelta(st, 4) != 0 ||
            TryMoveActorForward(actor, 0x80, 0, 0x100, 0) != 0 ||
            CastRayWorldAndActors(svD, &actor->posX) != 0) {
          DespawnActorRecord(actor);
          continue;
        }
        if ((*st & 7) == 0) {
          g_pfnLevelEmitParticles(1, EMIT_DRIFT_SPARKLE, &actor->posX, (void *)0x10);
        }
        break;
      }
      }
      break;
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
        svD[0] = GetRandomU32() & 3;
        svD[1] = GetRandomU32() & 3;
        svD[2] = 0x14;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, &actor->posX, svD);
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
            CopyVector(svF, &actor->posX);
            svF[2] += 0x400;
            RefreshActorDerivedState(actor, ACTOR_REFRESH_GRID);
            if (FindGroundHeightBelow(svF, 0x1000) > 0) {
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
        break;

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
                                             spy->posY - actor->posY, ATAN_ANGLE8) -
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
                                      spy->posY - rider->posY, ATAN_ANGLE8);
          int aim = (bearing + (GetRandomU32() & 0xF)) - 8;

          EBB_BREAK();
          CopyVector(st->wanderTo, &rider->posX);
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
            TurnActorYawTowardAngle(actor, ArcTan2(svA[0], svA[1], ATAN_ANGLE8), 0xA, 0, 0);
            actor->rotY = ArcTan2(VectorLength(svA, VEC_LEN_XY), svA[2], ATAN_ANGLE8);
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

            SubtractVector(svJ, &st->target->posX, &actor->posX);
            dist = VectorLength(svJ, VEC_LEN_XYZ);

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
                ScaleVector3ByRatio(svJ, dist, lim);
              } else {
                ScaleVector3ByRatio(svJ, dist, 0x122);
              }
              AddVector(&actor->posX, &actor->posX, svJ);
              TurnActorYawTowardAngle(actor, ArcTan2(svJ[0], svJ[1], ATAN_ANGLE8), 0xA, 0, 0);
              actor->rotY = ArcTan2(VectorLength(svJ, VEC_LEN_XY), svJ[2], ATAN_ANGLE8);
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
          svR[0] = 0x200;
          svR[1] = 0;
          svR[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, svR, svR);
          AddVector(svR, svR, &actor->posX);
          SubtractVector(svR, svR, &spy->posX);
          svR[2] -= 0x100;
          RShiftVector3(svR, 5);
          ScaleVector3Sat(svR, svR, st->shake);
          DivideVector3(svR, svR, 6);
          AddVector(svR, svR, &spy->posX);
          svR[2] += 0x100;
          svR[2] += g_anSineLut[(st->shake << 7) / 192] >> 3;
          CopyVector(&key->posX, svR);
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
          svS[0] = 0x200 - (st->shake - 0xC0) * 8;
          svS[1] = 0;
          svS[2] = 0x140;
          RotateVectorByMatrix(actor->mtx, svS, svS);
          AddVector(&key->posX, svS, &actor->posX);
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

    /* Type 189: talking dragon. When Spyro faces it up close, start its
     * dialogue; the first visit and the post-boss visit pick their own
     * lines. */
    case ACTOR_TYPE_HOME_TALKER + 2: {
      if (actor->mode == TALKER_REARM) {
        if (ApproxDist2D(&actor->posX, &SPY.posX) >= 0x1000) {
          actor->mode = TALKER_READY;
        }
      } else if ((SPY.state == SPYRO_ST_IDLE || SPY.state == SPYRO_ST_WALK || SPY.state == SPYRO_ST_JOG ||
                  SPY.state == SPYRO_ST_RUN || SPY.state == SPYRO_ST_CHARGE) &&
                 ApproxDist2D(&actor->posX, &spy->posX) < 0x780 &&
                 AbsAngleDelta8(SPY.bodyRotZ,
                               ArcTan2(actor->posX - spy->posX,
                                             actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8)) <
                     0x20 &&
                 AbsAngleDelta8(actor->rotZ,
                               ArcTan2(spy->posX - actor->posX,
                                             g_nSpyroWorldPosY - actor->posY, ATAN_ANGLE8)) <
                     0x28) {
        actor->mode = TALKER_REARM;
        SPY.requestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_FREEZE_INPUT);
        ResetSpyroLinearMotion();
        ZeroVector(&spy->headLook);
        g_pfnLevelBeginActorCutscene(actor);
        g_abWorldBalloonistState[1] = BALLOONIST_OPEN;
        g_abWorldBalloonistState[2] = BALLOONIST_OPEN;

        if (g_nTotalEggsRecovered < 5) {
          if (g_abWorldBalloonistState[3] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_LOCKED_FIRST);
            g_abWorldBalloonistState[3] = BALLOONIST_TALKED;
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_LOCKED_AGAIN);
          }
        } else if (g_abWorldBalloonistState[3] < BALLOONIST_OPEN) {
          if (g_abWorldBalloonistState[3] == BALLOONIST_UNMET) {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_UNLOCK_FIRST);
          } else {
            g_nActorCutsceneDialogPage = BALLOONIST_PAGE(2, BPAGE_UNLOCK_AGAIN);
          }
          g_abWorldBalloonistState[3] = BALLOONIST_OPEN;
        } else {
          g_nActorCutsceneDialogPage = BPAGE_ROSTER;
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
          SubtractVector(tgt271, at, &spy->posX);
          tgt271[2] = (tgt271[2] * 3) >> 2;

          if (VectorLength(tgt271, VEC_LEN_XYZ) < 0x440) {
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

    /* Type 270: villager (VillagerState). */
    case ACTOR_TYPE_VILLAGER: {
      VillagerState *st = actor->state;
      int pairGone;
      int animState;

      if (st->init == 0) {
        int gone;
        int idx;
        int word;
        int bit;

        st->init = one;
        gone = 0;
        idx = actor - g_pActorListBase;
        word = idx >> 5;
        bit = idx & 0x1F;
        if (g_anRunningKillBitmap[word] & (one << bit)) {
          gone = actor->dropKind == 0xFF;
        }
        if (st->mode == VROLE_PAIR) {
          Actor *pair = &g_pActorListBase[st->link];

          pairGone = 0;
          idx = pair - g_pActorListBase;
          word = idx >> 5;
          bit = idx & 0x1F;
          if ((g_anRunningKillBitmap[word] & (one << bit)) &&
              (pair->mode >= ACTOR_MODE_DORMANT || pair->dropKind == 0xFF)) {
            pairGone = 1;
          }
        }
        if (st->mode == VROLE_PAIR && pairGone == 0) {
          continue;
        }
        if (gone) {
          if (st->mode == VROLE_PAIR && pairGone) {
            if (g_pActorListBase[st->link].mode < ACTOR_MODE_DORMANT) {
              DespawnActorRecord(&g_pActorListBase[st->link]);
            }
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
          StopSoundVoiceByIndex(actor->voiceRow, 4);
          DespawnActorRecord(actor);
          continue;
        }
      }

      if ((actor->flags & ACTOR_HITS_SPYRO) != 0 && actor->mode != VILLAGER_KNOCKED) {
        st->speed = 0xFA;
        st->heading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, 0x20, 0x40);
        StopSoundVoiceByIndex(actor->voiceRow, 4);
        SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
        MarkActorCommittedKill(actor);
        ENTER_POSE(actor, VILLAGER_KNOCKED);
      }

      switch (actor->mode) {
      case VILLAGER_IDLE: {
        int dist;
        int animState;
        int marker;
        int range;

        dist = ApproxDist2D(&actor->posX, &SPY.posX);
        if (st->friend >= 0 && (st->mode != VROLE_PAIR || actor->subMode == 0) &&
            (GetRandomU32() & 1)) {
          g_pfnLevelEmitParticles(1, EMIT_TRACKING_SPARK, (int *)actor, &g_pActorListBase[st->friend]);
        }
        if (st->chunkAnim != -1) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          marker = CHUNK_ANIM_STEP(animState);
        }
        if (dist < st->camDist << 10 && (st->camAlways != 0 || marker >= 2) &&
            SPY.state != SPYRO_ST_CHARGE && SPY.state != SPYRO_ST_CHARGE_AIR &&
            AbsAngleDelta8(SPY.bodyRotZ,
                          ArcTan2(actor->posX - SPY.posX,
                                        actor->posY - SPY.posY, ATAN_ANGLE8)) < 0x20) {
          g_pSpyroRequestCameraAnchor = &spy->posX;
          g_pSpyroRequestCameraTargetParams = &g_anCameraActiveTargetParams;
          g_dwSpyroRequestMask = (SPYRO_RQ_PENDING | SPYRO_RQ_CAMERA);
          g_nCameraActiveTargetDistance = 0xC00;
          g_nCameraActiveTargetPitch = 0x80;
          g_nCameraActiveTargetPitchBias = -0xB0;
          g_nCameraActiveTargetRoll = 0;
          g_nCameraActiveTargetYawBias = 0;
          g_anCameraActiveTargetParams.w[0] = 0x800 - g_bSpyroPersistentEulerZ * 16;
        }
        if (st->chunkAnim == -1) {
          break;
        }
        if (st->chunkAnim == 0 && st->timer < 3 && (animState & CHUNK_ANIM_HALTED) && g_nActiveLevelId == LEVEL_ALPINE_RIDGE) {
          Actor *f = &g_pActorListBase[st->friend];

          PlaySoundEffect(g_pLevelSampleBankHeader[0x2A], f, SFX_MODE_3D, &f->voiceRow);
        }
        if (st->mode == VROLE_CHAT) {
          range = st->talkDist << 10;

          if (SPY.state != SPYRO_ST_CHARGE || st->noBoost == 0) {
            range += 0xC00;
          }
          SnapActorToGround(actor);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (marker < 2) {
            ANIM_GO(actor, 0);
            if (dist < range) {
              if (st->timer == 0) {
                st->timer = st->timerLong;
              } else if (TickCountdownByFrameDelta(&st->timer, 4) == 2 && (animState & CHUNK_ANIM_HALTED)) {
                ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              }
            } else {
              TickCountdownByFrameDelta(&st->timer, 4);
            }
          } else {
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
            if (TickCountdownByFrameDelta(&st->idleTimer, 4)) {
              st->idleTimer = (GetRandomU32() & 0x2F) + 0x3C;
              PlayActorMeshVoice(actor, (GetRandomU32() & 1) | 4, 0);
            }
            ANIM_GO(actor, 4);
            if (range + 0x800 < dist && (animState & CHUNK_ANIM_HALTED)) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              st->timer = 0x1E;
            }
          }
        } else if (st->mode == VROLE_FACE) {
          if (st->friend > 0) {
            TurnActorYawTowardAngle(actor,
                          ArcTan2(g_pActorListBase[st->friend].posX - actor->posX,
                                        g_pActorListBase[st->friend].posY - actor->posY, ATAN_ANGLE8),
                          6, 0, 0);
          }
          SnapActorToGround(actor);
          if (TickCountdownByFrameDelta(&st->faceTimer, 4) == 0) {
            break;
          }
          if ((SPY.state == SPYRO_ST_SUPERCHARGE || SPY.state == SPYRO_ST_SUPERCHARGE_AIR ||
               (SPY.state == SPYRO_ST_CHARGE_AIR && (g_nSpyroSubstate & SPYRO_SUB_SUPERCHARGE))) &&
              g_nActiveLevelId == LEVEL_MAGIC_CRAFTERS && marker < 2) {
            break;
          }
          if ((st->fleeDist << 10) < dist && TickCountdownByFrameDelta(&st->timer, 4)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            if (marker >= 2) {
              st->timer = st->timerLong;
            } else {
              st->timer = st->timerShort;
            }
          } else if (dist < st->fleeDist << 10) {
            if (actor->group == 0xFF) {
              actor->mode = VILLAGER_PANIC;
              continue;
            }
            SetActorGroupStateExcept(actor, VILLAGER_PANIC, VILLAGER_KNOCKED);
          }
        } else if (st->mode == VROLE_PAIR) {
          if (actor->subMode == 0 &&
              (ApproxDist2D(&actor->posX, &SPY.posX) < 0x3400 ||
               g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            actor->subMode = 1;
            st->timer = 0x46;
          } else if (TickCountdownByFrameDelta(&st->timer, 4) == 2) {
            g_pActorListBase[st->link].subMode = 1;
          }
          if ((g_pActorListBase[st->link].nextAnim == 3 || g_pActorListBase[st->link].mode >= ACTOR_MODE_DORMANT) &&
              ApproxDist2D(&actor->posX, &SPY.posX) <
                  (st->fleeDist << 10) + 0x7D0) {
            actor->mode = VILLAGER_PANIC;
            continue;
          }
        } else if (st->mode == VROLE_LEADER) {
          Actor *lead = &g_pActorListBase[st->link];
          LeaderState *ls = lead->state;

          SnapActorToGround(actor);
          TurnActorYawTowardAngle(actor,
                        ArcTan2(lead->posX - actor->posX,
                                      lead->posY - actor->posY, ATAN_ANGLE8),
                        6, 0, 0);
          if (marker < 3) {
            if (TickCountdownByFrameDelta(&st->timer, 4) && ls->script[1] == 7) {
              st->timer = 0x12C;
              ModifyWorldChunkAnimFlags(9, CHUNK_ANIM_RESUME, 0);
              ls->mode = 8;
              ls->cue = 2;
            }
            if ((ANIM_WORD(lead) & 0xFF0000FF) == 0x04000004 &&
                (int)(GetRandomU32() & 0xFF) >= 0xD3) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
              st->timer = 0x5A;
            }
            if ((ANIM_WORD(lead) & 0xFF0000FF) == 0x08000004 &&
                lead->frameProgress < 0x11) {
              ANIM_GO(actor, 9);
            }
            if ((ANIM_WORD(actor) & 0xFF00FF00) == 0x14000900 &&
                g_nActorAnimNewFrame != 0) {
              ANIM_GO(actor, 0);
            }
          } else if (TickCountdownByFrameDelta(&st->timer, 4)) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        break;
      }

      case VILLAGER_KNOCKED:
        if (st->speed != 0) {
          MoveActorAlongAngle(actor, st->heading, st->speed, 0, 0x2BC, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          st->speed -= 0x14;
          if (st->speed < 0) {
            st->speed = 0;
          }
        }
        if (g_nActorAnimDone != 0) {
          actor->cullRadius = 0;
        }
        if (st->chunkAnim != -1 && st->chunkAnimDone == 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if (animState & CHUNK_ANIM_HALTED) {
            if (CHUNK_ANIM_STEP(animState) >= 2) {
              ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
            } else {
              st->chunkAnimDone = one;
            }
          }
        } else {
          st->chunkAnimDone = one;
        }
        if (actor->cullRadius == 0 && st->chunkAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          StopSoundVoiceByIndex(actor->voiceRow, 4);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case VILLAGER_PANIC:
        StopSoundVoiceByIndex(actor->voiceRow, 4);
        st->idleTimer = 0;
        SnapActorToGround(actor);
        if (st->stopVoice != 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if ((animState & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(animState) >= 2) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        ANIM_GO(actor, 7);
        if (g_nActorAnimDone != 0) {
          st->speed = 0x5F;
          st->timer = RandomIntInRange(0xF0, 0x168);
          actor->mode = VILLAGER_RUN;
          continue;
        }
        break;

      case VILLAGER_RUN:
        if (TickCountdownByFrameDelta(&st->idleTimer, 4)) {
          st->idleTimer = (GetRandomU32() & 0x2F) + 0x5A;
          PlayActorMeshVoice(actor, (int)GetRandomU32() % 4, 0);
        }
        SnapActorToGround(actor);
        if (st->stopVoice != 0) {
          animState = GetWorldChunkAnimState(st->chunkAnim);
          if ((animState & CHUNK_ANIM_HALTED) && CHUNK_ANIM_STEP(animState) >= 2) {
            ModifyWorldChunkAnimFlags(st->chunkAnim, CHUNK_ANIM_RESUME, 0);
          }
        }
        ANIM_GO(actor, 8);
        if (ApproxDist2D(&actor->posX, PATH_PT(st->path, st->path[1])) < 0x96) {
          int next;

          do {
            next = RandomIntInRange(0, st->path[0] - 1);
          } while (next == st->path[1]);
          st->path[1] = next;
          st->speed = RandomIntInRange(0x3C, 0x6E);
        }
        if (TurnActorYawTowardAngle(actor,
                          ArcTan2(PATH_X(st->path, st->path[1]) - actor->posX,
                                        PATH_Y(st->path, st->path[1]) - actor->posY,
                                        ATAN_ANGLE8),
                          7, 0x50, one)) {
          MoveActorAlongHeading(actor, st->speed, 0, 0, one);
        }
        if (ApproxDist2D(&actor->posX, &SPY.posX) >
                (st->fleeDist << 10) + 0x1388 &&
            (st->mode != VROLE_PAIR || g_pActorListBase[st->link].nextAnim != 3)) {
          ENTER_POSE(actor, VILLAGER_IDLE);
        }
        if (TickCountdownByFrameDelta(&st->timer, 4)) {
          st->idleTimer = 0x1E;
          st->runFrom = ApproxDist2D(&actor->posX, &SPY.posX);
          ANIM_BLEND(actor, 4);
          actor->mode = VILLAGER_STOP;
          continue;
        }
        break;

      case VILLAGER_STOP:
        if (TickCountdownByFrameDelta(&st->idleTimer, 4)) {
          st->idleTimer = (GetRandomU32() & 0x2F) + 0x3C;
          PlayActorMeshVoice(actor, (GetRandomU32() & 1) | 4, 0);
        }
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        if (ApproxDist2D(&actor->posX, &SPY.posX) < st->runFrom - 0x80) {
          actor->mode = VILLAGER_PANIC;
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 271: shepherd (ShepherdState). */
    case ACTOR_TYPE_SHEPHERD: {
      ShepherdState *st = actor->state;
      int near = 0x4B0;

      TickActorFlameGlow(actor);
      if ((actor->flags & (ACTOR_HIT_CHARGE | ACTOR_HIT_SUPER)) != 0 && actor->mode != SHEPHERD_KNOCKED) {
        st->knockSpeed = 0x1C2;
        st->knockHeading = StepAndLerpAngle8(
            ArcTan2(actor->posX - spy->posX, actor->posY - spy->posY, ATAN_ANGLE8),
            spy->bodyRotZ, 0x20, 0x40);
        st->knockSpin = 0x8C;
        actor->flags = 0;
        ((CollectibleState *)SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0)->state)->mode = COLLREST_SNAP;
        MarkActorCommittedKill(actor);
        PlayActorMeshVoice(actor, 0, 0);
        ENTER_POSE(actor, SHEPHERD_KNOCKED);
      }
      actor->flags = 0;

      switch (actor->mode) {
      case SHEPHERD_IDLE:
        switch (st->post) {
        case SHEPHERD_POST_GUARD: {
          char *base = (char *)&g_nSpyroWorldPosZ;
          int h = actor->posZ - actor->floorOffset - *(int *)base;

          ANIM_GO(actor, 0);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          if (TickCountdownByFrameDelta(&st->timer, 4) &&
              ApproxDist2D(&actor->posX, (int *)(base - 8)) < 0x898 &&
              h < 0 && h > -0x7D0) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_RUN_PATH:
          ANIM_GO(actor, 1);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x3800) {
            st->done = 0;
            *st->doneFlag = 0;
            SetActorGroupStateExcept(actor, SHEPHERD_CHASE, SHEPHERD_KNOCKED);
          }
          break;

        case SHEPHERD_POST_LOITER:
          if (actor->nextAnim != 6) {
            g_nActorAnimDone = 0;
            ANIM_ADVANCE(actor, 6);
            actor->nextFrame = RandomIntInRange(0, 0xF);
          }
          if (st->heading == -1) {
            st->heading = actor->rotZ;
          }
          TurnActorYawTowardAngle(actor, st->heading, 4, 0, 0);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
            SetActorGroupStateExcept(actor, SHEPHERD_CHASE, SHEPHERD_KNOCKED);
          }
          break;

        case SHEPHERD_POST_GUARD_LEVEL:
          ANIM_GO(actor, 0);
          TrackEntityYawTowardSpyro(actor, 6, 0, 0);
          SnapActorToGround(actor);
          if (TickCountdownByFrameDelta(&st->timer, 4) &&
              ApproxDist2D(&actor->posX, &spy->posX) < near &&
              ABS2(SPY_DZ(actor)) < 0x320) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;

        case SHEPHERD_POST_THROWER:
          if (st->heading == -1) {
            st->heading = actor->rotZ;
          }
          TurnActorYawTowardAngle(actor, st->heading, 4, 0, 0);
          ANIM_GO(actor, 5);
          if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x3000) {
            Actor *p = &g_pActorListBase[st->partner];
            ShepherdState *ps;

            p->mode = SHEPHERD_CHASE;
            ps = p->state;
            ps->vz = 0x14;
            ps->side = 0;
            ANIM_GO(actor, 0xD);
            actor->mode = SHEPHERD_WATCH;
            continue;
          }
          if (g_nActorAnimNewFrame != 0 && actor->nextFrame == 0xE) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
            CrookState *cs = c->state;

            GetActorAttachPoint(actor, 0, &c->posX);
            c->rotZ = actor->rotZ;
            cs->owner = actor;
            cs->live = one;
            cs->heading = actor->rotZ;
            cs->life = 0x14;
            cs->speed = 0x50;
          }
          break;

        case SHEPHERD_POST_LEAPER:
          if (actor->subMode != 0) {
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
            if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
              if (actor->nextAnim == 4) {
                g_nActorAnimDone = 0;
                actor->frameProgress = sixteen;
                actor->frameRate = sixteen;
                actor->nextAnim = 1;
                actor->nextFrame = 0;
                OnActorAnimQueued(actor);
              }
              actor->mode = SHEPHERD_CHASE;
              continue;
            }
            ANIM_GO(actor, 2);
          } else {
            ANIM_GO(actor, 5);
            if (ApproxDist2D(&actor->posX, &spy->posX) < st->range << 10 &&
                g_nSpyroAirborneFrames == 0 && SPY.posZ - PATH_Z(st->path, 0) > 0) {
              Actor *p = &g_pActorListBase[st->partner];

              p->mode = SHEPHERD_ROUSE;
              st->vz = 0xD2;
              ENTER_POSE(actor, SHEPHERD_LEAP_ON);
            }
          }
          if (g_nActorAnimNewFrame != 0 && actor->nextFrame == 0xE) {
            Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
            CrookState *cs = c->state;

            GetActorAttachPoint(actor, 0, &c->posX);
            c->rotZ = actor->rotZ;
            cs->owner = actor;
            cs->live = one;
            cs->heading = actor->rotZ;
            cs->life = 0x1E;
            cs->speed = 0x50;
          }
          break;

        case SHEPHERD_POST_PAIR_PATROL: {
          Actor *p = &g_pActorListBase[st->partner];

          if (TickCountdownByFrameDelta(&st->timer2, 4) &&
              ApproxDist2D(&actor->posX, &spy->posX) < 0x1000) {
            int mine = ArcTan2(actor->posX - spy->posX,
                                     actor->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
            int theirs =
                ArcTan2(p->posX - spy->posX, p->posY - g_nSpyroWorldPosY, ATAN_ANGLE8);
            ShepherdState *ps = p->state;

            if (WrapAngleDelta8(mine, theirs) < 0) {
              st->side = 2;
              ps->side = one;
            } else {
              st->side = one;
              ps->side = 2;
            }
            SetActorGroupStateExcept(actor, SHEPHERD_ALERTED, SHEPHERD_KNOCKED);
          }
          if (st->sub == 0) {
            ANIM_GO(actor, 1);
            PivotActorAlongPath(actor, st->path, 0x100, 0x48, 0xC8, 3, 8,
                          actor->group, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
            if (TickCountdownByFrameDelta(&st->timer, 4) && p->mode < ACTOR_MODE_DORMANT &&
                ApproxDist2D(&actor->posX, &p->posX) < 0x8FC) {
              st->timer = 0x5A;
              st->sub = one;
            }
          } else if (st->sub == one) {
            if (TurnActorYawTowardAngle(actor,
                              ArcTan2(p->posX - actor->posX,
                                            p->posY - actor->posY, ATAN_ANGLE8),
                              4, 0xA, one)) {
              st->sub = 2;
            }
          } else if (st->sub == 2) {
            ANIM_GO(actor, 0xE);
            if (g_nActorAnimDone != 0) {
              st->sub = 0;
            }
          }
          break;
        }

        case SHEPHERD_POST_PATROL:
          if (TickCountdownByFrameDelta(&st->timer2, 4) &&
              ABS2(SPY_DZ(actor)) < 0x320 &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x1C00) {
            actor->mode = SHEPHERD_CHASE;
            continue;
          }
          ANIM_GO(actor, 1);
          PivotActorAlongPath(actor, st->path, 0x100, 0x78, 0xC8, 6, 0x28, 0xFF, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          break;

        case SHEPHERD_POST_WATCH:
          TrackEntityYawTowardSpyro(actor, 4, 0, 0);
          if (g_nSpyroAirborneFrames == 0 && ABS2(SPY_DZ(actor)) < 0x320 &&
              ApproxDist2D(&actor->posX, &SPY.posX) < 0x2000) {
            actor->mode = SHEPHERD_CHASE;
            continue;
          }
          break;

        case SHEPHERD_POST_COVER: {
          Actor *p = &g_pActorListBase[st->partner];
          Actor *q = &g_pActorListBase[st->partner2];

          if (p->nextAnim == 3 || q->nextAnim == 3) {
            st->post = SHEPHERD_POST_WATCH;
            ENTER_POSE(actor, SHEPHERD_RECOVER);
          }
          switch (actor->subMode) {
          case 0: {
            int ang = ArcTan2(p->posX - actor->posX,
                                    p->posY - actor->posY, ATAN_ANGLE8);

            ANIM_GO(actor, 5);
            TurnActorYawTowardAngle(actor, ang, 4, 0, 0);
            if (ApproxDist2D(&actor->posX, &SPY.posX) < 0x1800) {
              actor->subMode = 1;
            }
            if (g_nActorAnimNewFrame != 0 && actor->nextFrame == 0xE) {
              Actor *c = g_pfnLevelSpawnActor(ACTOR_TYPE_SHEPHERD_THROW, actor);
              CrookState *cs = c->state;

              GetActorAttachPoint(actor, 0, &c->posX);
              c->rotZ = actor->rotZ;
              cs->owner = actor;
              cs->live = one;
              cs->heading = actor->rotZ;
              cs->life = 0x1E;
              cs->speed = 0x50;
            }
            break;
          }
          case 1: {
            int *pos = &actor->posX;
            int d = ApproxDist2D(pos, &spy->posX);

            ANIM_GO(actor, 0);
            TrackEntityYawTowardSpyro(actor, 4, 0, 0);
            if (d > 0x2000) {
              actor->subMode = 0;
            }
            if (TickCountdownByFrameDelta(&st->timer, 4) &&
                ApproxDist2D(pos, &spy->posX) < near &&
                ABS2(SPY_DZ(actor)) < 0x320) {
              ENTER_POSE(actor, SHEPHERD_SWING);
            }
            break;
          }
          }
          break;
        }
        }
        break;

      case SHEPHERD_CHASE:
        ANIM_GO(actor, 1);
        switch (st->post) {
        case SHEPHERD_POST_RUN_PATH:
          if (st->done == 0 &&
              PivotActorAlongPath(actor, st->path, 0x100, 0x8C, 0, 0x10, 0x80, 0xFF,
                            MOVE_LIFT | MOVE_FOLLOW_FLOOR) == 0x100) {
            st->done = one;
            *st->doneFlag = one;
            st->post = SHEPHERD_POST_GUARD;
            ENTER_POSE(actor, SHEPHERD_IDLE);
          }
          break;

        case SHEPHERD_POST_LOITER:
        case SHEPHERD_POST_PAIR_PATROL:
        case SHEPHERD_POST_PATROL: {
          int side = st->side;
          int ang;

          if (side != 0 && SPY_FACING(actor) > 0x40) {
            side = 3 - side;
          }
          CopyVector(tgt271, &spy->posX);
          if (side == one) {
            tgt271[0] += (unsigned short)g_anCosineLut[WrapAddAngle8(g_bSpyroPersistentEulerZ, -0x40)] << 16 >> 19;
            tgt271[1] += (unsigned short)g_anSineLut[WrapAddAngle8(SPY.bodyRotZ, -0x40)] << 16 >> 19;
          } else if (side == 2) {
            tgt271[0] += (unsigned short)g_anCosineLut[WrapAddAngle8(g_bSpyroPersistentEulerZ, 0x40)] << 16 >> 19;
            tgt271[1] += (unsigned short)g_anSineLut[WrapAddAngle8(SPY.bodyRotZ, 0x40)] << 16 >> 19;
          }
          ang = ArcTan2(tgt271[0] - actor->posX, tgt271[1] - actor->posY,
                              ATAN_ANGLE8);
          if (TickCountdownByFrameDelta(&st->timer, 4) &&
              TurnActorYawTowardAngle(actor, ang, 6, 0x14, one) &&
              SPY_FACING(actor) < 0x32 &&
              MoveActorAlongHeading(actor, 0x32, 0x12C, 0x1F4, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_LOITER && SPY_FACING(actor) > 0x40) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_LOITER &&
              ApproxDist2D(&actor->posX, st->home) > 0x1800) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (st->post == SHEPHERD_POST_PAIR_PATROL || st->post == SHEPHERD_POST_PATROL) {
            if (FindNearestPathPointToActor(actor, st->path, idx271) > 0x1000) {
              if (st->post == SHEPHERD_POST_PATROL) {
                st->path[1] = idx271[0];
                CopyVector(st->home, PATH_PT(st->path, idx271[0]));
              }
              actor->mode = SHEPHERD_GO_HOME;
              continue;
            }
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) && SPY_FACING(actor) < 0x32 &&
              ApproxDist2D(&actor->posX, &spy->posX) < near &&
              g_nSpyroAirborneFrames == 0 && SPY_DZ(actor) < 0xC8 &&
              ABS2(SPY_DZ(actor)) < 0x3E8) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_LEAPER: {
          int *pos;

          if (TrackEntityYawTowardSpyro(actor, 6, 0x14, 1)) {
            if (TickCountdownByFrameDelta(&st->timer, 4) == 0) {
              break;
            }
            if (SPY_FACING(actor) < 0x32 &&
                MoveActorAlongHeading(actor, 0x32, 0, 0x1F4, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT | MOVE_SLIDE)) {
              actor->mode = SHEPHERD_GO_HOME;
              continue;
            }
          }
          pos = &actor->posX;
          if (ApproxDist2D(pos, st->home) > 0x2000) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) && SPY_FACING(actor) < 0x32 &&
              ApproxDist2D(pos, &spy->posX) < near && SPY_DZ(actor) < 0xC8 &&
              ABS2(SPY_DZ(actor)) < 0x3E8) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }

        case SHEPHERD_POST_WATCH: {
          int *pos;

          if (TrackEntityYawTowardSpyro(actor, 6, 0x14, 1) && SPY_FACING(actor) < 0x32 &&
              MoveActorAlongHeading(actor, 0x32, 0x12C, 0x1F4, MOVE_LIFT | MOVE_FOLLOW_FLOOR | MOVE_STEP_LIMIT)) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          pos = &actor->posX;
          if (ApproxDist2D(pos, st->home) > 0x1800) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          if (TickCountdownByFrameDelta(&st->timer, 4) && SPY_FACING(actor) < 0x32 &&
              ApproxDist2D(pos, &spy->posX) < near && SPY_DZ(actor) < 0xC8) {
            ENTER_POSE(actor, SHEPHERD_SWING);
          }
          break;
        }
        }
        break;

      case SHEPHERD_KNOCKED:
        DriveActorTrackedMove(actor, &st->knockSpeed, st->knockHeading, &st->knockSpin,
                      0x12, 0xC);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;

      case SHEPHERD_SWING:
        TrackEntityYawTowardSpyro(actor, 6, 0, 0);
        SnapActorToGround(actor);
        if (actor->nextFrame >= 0x1C) {
          if (st->post == SHEPHERD_POST_LOITER) {
            actor->mode = SHEPHERD_GO_HOME;
            continue;
          }
          st->timer = 0xA0;
          actor->mode = SHEPHERD_IDLE;
          continue;
        }
        CAM_LOOK.x = (g_anSineLut[actor->rotZ] << 7) >> 12;
        CAM_LOOK.y = -(g_anCosineLut[actor->rotZ] << 7) >> 12;
        CAM_LOOK.z = 0;
        break;

      case SHEPHERD_WATCH: {
        Actor *p = &g_pActorListBase[st->partner];

        if (g_nActorAnimDone != 0) {
          ANIM_GO(actor, 0);
        }
        TrackEntityYawTowardSpyro(actor, 4, 0, 0);
        if (ApproxDist2D(&actor->posX, &spy->posX) < 0xC00 &&
            ABS2(SPY_DZ(actor)) < 0x258) {
          st->post = SHEPHERD_POST_GUARD;
          actor->mode = SHEPHERD_IDLE;
          continue;
        }
        if (p->anim == 3) {
          ENTER_POSE(actor, SHEPHERD_RECOVER);
        }
        break;
      }

      case SHEPHERD_RECOVER:
        if (g_nActorAnimDone != 0) {
          st->post = SHEPHERD_POST_GUARD;
          ENTER_POSE(actor, SHEPHERD_IDLE);
        }
        break;

      case SHEPHERD_ROUSE:
        if (TrackEntityYawTowardSpyro(actor, 8, 0x23, 1) == 0) {
          break;
        }
        ANIM_GO(actor, 7);
        if (g_nActorAnimDone == 0) {
          break;
        }
        /* fall through */
      case SHEPHERD_ALERTED:
        st->timer2 = 0xB4;
        actor->mode = SHEPHERD_CHASE;
        continue;

      case SHEPHERD_LEAP_ON: {
        Actor *p = &g_pActorListBase[st->partner];
        int d;

        ((ShepherdState *)p->state)->timer = 0xE;
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, tgt271);
        if (st->vz < -0x12C) {
          st->vz = -0x12C;
        }
        if (p->mode == SHEPHERD_KNOCKED) {
          st->vz = 0xC8;
          st->knockSpeed = 0x190;
          st->knockHeading = ArcTan2(actor->posX - spy->posX,
                                           actor->posY - spy->posY, ATAN_ANGLE8);
          actor->mode = SHEPHERD_THROWN;
          continue;
        }
        if (st->vz < 0 &&
            actor->posZ - actor->floorOffset - tgt271[2] <= ABS(st->vz)) {
          p->mode = SHEPHERD_ALERTED;
          actor->mode = SHEPHERD_RIDE;
          continue;
        }
        actor->posZ += st->vz;
        st->vz -= 0x14;
        d = ApproxDist2D(&actor->posX, tgt271);
        if (d > 0xC8) {
          MoveActorAlongAngle(actor,
                        ArcTan2(tgt271[0] - actor->posX,
                                      tgt271[1] - actor->posY, ATAN_ANGLE8),
                        0xC8, 0, 0, 0);
        } else {
          TurnActorYawTowardAngle(actor, p->rotZ, 6, 0, 0);
          actor->posX = p->posX;
          actor->posY = p->posY;
        }
        if (d < 0x5DC) {
          TurnActorYawTowardAngle(actor, p->rotZ, 4, 0, 0);
        }
        break;
      }

      case SHEPHERD_RIDE: {
        Actor *p = &g_pActorListBase[st->partner];

        ((ShepherdState *)p->state)->timer = 0xA;
        ANIM_GO(actor, 0xC);
        if (p->nextAnim == 3) {
          st->vz = 0xC8;
          st->knockSpeed = 0x190;
          st->knockHeading = ArcTan2(actor->posX - spy->posX,
                                           actor->posY - spy->posY, ATAN_ANGLE8);
          actor->mode = SHEPHERD_THROWN;
          continue;
        }
        RefreshActorDerivedState(p, ACTOR_REFRESH_MATRIX);
        GetActorAttachPoint(p, 0, &actor->posX);
        TurnActorYawTowardAngle(actor, p->rotZ, 6, 0, 0);
        break;
      }

      case SHEPHERD_THROWN: {
        int floor = ProbeActorGroundHeight(actor);
        int z = actor->posZ + st->vz;

        if (floor + actor->floorOffset < z) {
          actor->posZ = z;
          st->vz -= 0x14;
          if (st->vz < -0xC8) {
            st->vz = -0xC8;
          }
        } else {
          actor->posZ = floor + actor->floorOffset;
        }
        if (st->knockSpeed != 0) {
          MoveActorAlongAngle(actor, st->knockHeading, st->knockSpeed, 0, 0x2BC, one);
          st->knockSpeed -= 0x14;
          if (st->knockSpeed < 0) {
            st->knockSpeed = 0;
          }
          if (st->knockSpeed != 0) {
            break;
          }
        }
        if (actor->posZ == floor + actor->floorOffset) {
          CopyVector(st->home, PATH_PT(st->path, 0));
          st->timer = 0x14;
          actor->subMode = 1;
          actor->mode = SHEPHERD_CHASE;
          continue;
        }
        break;
      }

      case SHEPHERD_GO_HOME: {
        int ang = ArcTan2(st->home[0] - actor->posX,
                                st->home[1] - actor->posY, ATAN_ANGLE8);

        ANIM_GO(actor, 1);
        if (TurnActorYawTowardAngle(actor, ang, 4, 0x14, one)) {
          MoveActorAlongHeading(actor, 0x5A, 0, 0, MOVE_LIFT | MOVE_FOLLOW_FLOOR);
          if (ApproxDist2D(&actor->posX, st->home) < 0x80) {
            actor->subMode = 1;
            st->timer = 0x3C;
            st->vz = 0;
            ENTER_POSE(actor, SHEPHERD_IDLE);
          }
        }
        break;
      }
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
      break;
    }

    /* Type 283: path guard. Walks its waypoint ring, waits on its partner,
     * rears up (and calls for help) when Spyro comes near, and is knocked
     * away when hit. */
    case ACTOR_TYPE_PATH_GUARD: {
      PathGuardState *st = actor->state;

      if (actor->mode != PATHGUARD_KNOCKED) {
        g_pfnLevelEmitParticles(1, EMIT_ACTOR_WHITE_STREAK, actor, 0);
        if (actor->flags & ACTOR_HITS_SPYRO) {
          st->speed = 0xDC;
          st->knock = StepAndLerpAngle8(ArcTan2(actor->posX - spy->posX,
                                                  actor->posY - spy->posY, ATAN_ANGLE8),
                                    spy->bodyRotZ, 0x20, 0x60);
          if (actor->flags & ACTOR_HIT_CHARGE) {
            st->speed += 0x8C;
          }
          StopSoundVoiceByIndex(actor->voiceRow, 4);
          SpawnActorDeathDrop(actor, DROP_TUMBLE_IF_NEAR, 0, 0);
          MarkActorCommittedKill(actor);
          ENTER_POSE(actor, PATHGUARD_KNOCKED);
        }
      }

      switch (actor->mode) {
      case PATHGUARD_WALK:
        if (st->flags & PATHGUARD_F_HOLD) {
          st->path->cur = st->node;
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= 0x100) {
            int dz;

            TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
            if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000) {
              dz = (actor->posZ - actor->floorOffset) - SPY.posZ;
              if (dz > 0 ? dz < 0x800
                         : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x800) {
                st->timer = 0;
                ENTER_POSE(actor, PATHGUARD_THROW);
              }
            }
            if (TickCountdownByFrameDelta(&st->timer, 4) != 0) {
              st->flags &= PATHGUARD_F_RING;
            }
            break;
          }
        } else {
          if (ApproxDist2D(&actor->posX, &spy->posX) < 0x1000) {
            int dz = (actor->posZ - actor->floorOffset) - SPY.posZ;

            if (dz > 0 ? dz < 0x800
                       : SPY.posZ - (actor->posZ - actor->floorOffset) < 0x800) {
              TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
              if (st->timer == 0 || --st->timer <= 0) {
                st->flags |= PATHGUARD_F_HOLD;
                ENTER_POSE(actor, PATHGUARD_THROW);
              }
              break;
            }
          }
          if (st->flags == PATHGUARD_F_RING) {
            PathGuardState *ps = g_pActorListBase[st->partner].state;

            st->path->cur = 0;
            if (st->dist == 0) {
              st->dist = ApproxDist2D(&actor->posX,
                                       &st->path->nodes[st->path->cur].x);
            }
            if (st->dist < ps->dist) {
              st->dist = ps->dist;
            }
            if (ps->heading != 0) {
              st->heading = (ps->heading + 0x80) & 0xFF;
              SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, 0x40, 0xE, 0x80, 0xFF, 0xFF, 0x180,
                            0x15C, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
              st->heading = 0;
              ps->heading = 0;
            } else {
              st->heading =
                  (ArcTan2(
                       actor->posX - st->path->nodes[st->path->cur].x,
                       actor->posY - st->path->nodes[st->path->cur].y, ATAN_ANGLE8) +
                   st->angle) &
                  0xFF;
              SteerActorAroundPoint(actor, &st->path->nodes[0].x, st->dist,
                            st->heading, 8, 0x40, 0xE, 0x80, 0xFF, 0xFF, 0, 0,
                            MOVE_FOLLOW_FLOOR);
            }
            break;
          }
          st->path->cur = 0;
          if (ApproxDist2D(&actor->posX,
                            &st->path->nodes[st->path->cur].x) <= 0x100) {
            break;
          }
        }
        TurnActorYawTowardAngle(
            actor,
            ArcTan2(st->path->nodes[st->path->cur].x - actor->posX,
                          st->path->nodes[st->path->cur].y - actor->posY, ATAN_ANGLE8),
            8, 0x10, 1);
        MoveActorAlongHeading(actor, 0x60, 0x180, 0x180, MOVE_LIFT | MOVE_PASS_ACTORS | MOVE_FOLLOW_FLOOR | MOVE_SLIDE);
        break;

      case PATHGUARD_THROW:
        TrackEntityYawTowardSpyro(actor, 8, 0x10, 1);
        if (g_nActorAnimDone != 0) {
          actor->mode = PATHGUARD_WALK;
          ANIM_RESET(actor, 0);
          continue;
        }
        if (st->timer <= 0) {
          st->timer = 1;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        } else if (st->timer < 2 && actor->nextFrame >= 2) {
          st->timer = 2;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        } else if (st->timer < 0x3C && actor->nextFrame >= 6) {
          st->timer = 0x3C;
          g_pfnLevelSpawnActor(ACTOR_TYPE_SPLIT_SHOT, actor);
        }
        break;

      case PATHGUARD_KNOCKED:
        DriveActorTrackedMove(actor, &st->speed, st->knock, 0, 0xC, 0);
        if (g_nActorAnimDone != 0) {
          RefreshActorDerivedState(actor, ACTOR_REFRESH_MATRIX);
          BurstActorWithSound(actor, 0x18);
          DespawnActorRecord(actor);
          continue;
        }
        break;
      }
      RefreshActorDerivedState(actor, ACTOR_REFRESH_COLLISION);
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

          if (actor->voiceRow == VOICE_ROW_NONE && st->left == (st->left / 48) * 48) {
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
        PlaySoundEffect(g_pLevelSampleBankHeader[voice], actor, SFX_MODE_3D, &actor->voiceRow);
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

        tgt271[0] = (GetRandomU32() & 0xFE) - 0x7F;
        tgt271[1] = (GetRandomU32() & 0xFE) - 0x7F;
        tgt271[2] = (GetRandomU32() & 0xFE) - 0x40;
        AddVector(tgt271, tgt271, &actor->posX);
        g_pfnLevelEmitParticles(1, EMIT_TWINKLE, tgt271, (int *)1);
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
            svT[0] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[1] = (GetRandomU32() & 0xFE) - 0x7F;
            svT[2] = (GetRandomU32() & 0xFE) - 0x40;
            AddVector(svT, svT, &actor->posX);
            g_pfnLevelEmitParticles(1, EMIT_TWINKLE, svT, (int *)1);
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

    /* Type 328: floor-snapped marker. Drops onto the floor once, then sits at
     * the centroid of the triangle it landed on. */
    case ACTOR_TYPE_FLOOR_MARKER: {
      int *tri = actor->state;

      if (*tri < 0) {
        int floor;
        int d;

        actor->posZ += 0x200;
        floor = FindGroundHeightBelow(&actor->posX, 0x400);
        d = actor->posZ - floor;
        if (ABS(d) < 0x400) {
          actor->posZ = floor;
          *tri = g_nLastCollisionHitChunkId;
        } else {
          actor->posZ -= 0x200;
        }
        if (*tri < 0) {
          break;
        }
      }
      UnpackWorldCollisionTri(*tri, mat2[0]);
      CopyVector(&actor->posX, mat2[0]);
      AddVector(&actor->posX, &actor->posX, mat2[1]);
      AddVector(&actor->posX, &actor->posX, mat2[2]);
      actor->posX /= 3;
      actor->posY /= 3;
      actor->posZ /= 3;
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
          ZeroVector(x352);
          g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &st->prize->posX, x352);
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
            CopyVector(x368, &st->prize->posX);
            x368[0] += g_anCosineLut[j * 43] >> 6;
            x368[1] += g_anSineLut[j * 43] >> 6;
            x368[2] += 0x28;
            col2[0] = g_anCosineLut[j * 43] >> 7;
            col2[1] = g_anSineLut[j * 43] >> 7;
            col2[2] = 0x10;
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, x368, col2);
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
    case ACTOR_TYPE_CHECKPOINT + 2:      switch (actor->mode) {
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

    /* One-shot effects: removed as soon as their animation ends. */
    case ACTOR_TYPE_HAZARD0_DEATH_FX:
    case ACTOR_TYPE_HAZARD0_ENTER_FX:      if (g_nActorAnimDone != 0) {
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
          SubtractVector(x352, &a->posX, &actor->posX);
          d = ABS2(x352[0]) + ABS2(x352[1]);
          if (d < 0x6000 &&
              (unsigned int)(VectorLength(x352, VEC_LEN_XY) - 0x1000) <= 0x2000 &&
              (unsigned int)(x352[2] + 0x1000) <= 0x2000) {
            d = AbsAngleDelta12(ArcTan2(actor->posX - spy->posX,
                                            actor->posY - spy->posY, ATAN_ANGLE12),
                              ArcTan2(x352[0], x352[1], ATAN_ANGLE12));
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

    /* Portal guide (PortalState). */
    case ACTOR_TYPE_PORTAL_GUIDE: {
      PortalState *st = actor->state;
      PortalCamPath *p;
      int mag;
      short progressed;
      switch (actor->mode) {
      case PORTALGUIDE_ENTER: {
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
        AddVector(x368, p->point[0].pos, p->point[1].pos);
        RShiftVector3(x368, 1);

        SubtractVector(&SPY.requestVelX, x368, &SPY.posX);
        AddVector(&SPY.requestVelX, &SPY.requestVelX, st->offset);

        mag = VectorLength(&SPY.requestVelX, VEC_LEN_XYZ);
        if (mag > 128) {
          ScaleVector3ByRatio(&SPY.requestVelX, mag, 128);
        } else {
          if (actor->subMode != PORTAL_SIDE_FORWARD) {
            progressed = (st->desc->index < (st->desc->count - 1));
          } else {
            progressed = st->desc->index;
          }

          if (progressed) {
            actor->mode = PORTALGUIDE_GLIDE;
          }
        }

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = PORTALGUIDE_IDLE;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          SPY.requestState = SPYRO_ST_GLIDE;
          g_nCdStreamState = LSTREAM_PRESERVE;
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
      case PORTALGUIDE_GLIDE: {
        SPY.requestMask = SPYRO_RQ_PENDING | 0x4000 | 0x2000 | 0x100;
        SPY.pPortalCamPath = st->desc;
        SPY.camPathMaxStep = 0x60;
        SPY.requestState = SPYRO_ST_GLIDE;
        SPY.otBinBias = 0x7F;

        if (CollideSphereWithWorldAndActors(&CAM.posX, 0x300, 0x300) != 0 &&
            (g_nGroundSurfaceType & 0x3F) < 0x3F && g_pTriggerRegionTable[g_nGroundSurfaceType][0] == TRIGGER_PORTAL) {
          actor->mode = PORTALGUIDE_IDLE;
          CAM.nextMode = (CAM_HELD | CAM_MODE_LEVEL_TRANSITION);
          g_nCdStreamState = LSTREAM_PRESERVE;
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

    /* Balloon: bobs about its rest height. */
    case ACTOR_TYPE_BALLOON: {
      int *base = actor->state;

      if (actor->mode == BALLOON_INIT) {
        *base = actor->posZ;
        actor->mode = BALLOON_BOB;
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
        svN[0] = ((short)g_anCosineLut[i * 32] >> 8) * 3;
        svN[1] = ((short)g_anSineLut[i * 32] >> 8) * 3;
        svN[2] = 0x18;
        g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, &actor->posX, svN);
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
                                  SPY.posY - st->parent->posY, ATAN_ANGLE8);
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
