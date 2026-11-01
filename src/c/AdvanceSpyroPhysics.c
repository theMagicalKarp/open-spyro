/* Matching notes (AdvanceSpyroPhysics).
   - Frame: the original's 0x98 frame has an untouched 8-byte reload slot at
     sp+0x68 after the i (0x58) / steps (0x60) spills.  `hole` reproduces it:
     an uninitialised local whose only reference is an empty asm with a "g"
     constraint is live through the whole function, gets no hard register,
     and reload gives it a stack slot (alter_reg, in pseudo order) that the
     asm then names as a memory operand -- no instruction is emitted.
   - loop.c movable threshold (&n lifetime 5 -> 4 via a pre-computed arg),
     a user-var retry preheader (base/b/p are not hoistable out of the outer
     loop), one variable `p` for contacted+nudge (set twice, never a
     movable), a non-sdata WS block so cse keeps `b + 4`, and an array-free
     `*q` suspend RMW (MEM_IN_STRUCT_P decides sched2 aliasing). */
#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "level_id.h"
#include "main_shared.h"
#include "match_asm.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

SDATA(g_nSpyroScriptedAlignFrames, 4);
SDATA(g_pSpyroContactActor, 4);

#define REQ g_adwSpyroRequestMaskBlock
#define WS g_anSpyroWallSlideBlock

#define ACTOR_POS(a) (&((Actor *)a)->posX)

/* Pull `pos` back to at most 0x140 from `from`. */
#define LIMIT_STEP(pos, from)                                                  \
  {                                                                            \
                                                                               \
    SubtractVector(d, pos, from);                                              \
    len = VectorLength(d, VEC_LEN_XYZ);                                        \
    if (len > 0x140) {                                                         \
      ScaleVector3ByRatio(d, len, 0x140);                                      \
      AddVector(pos, from, d);                                                 \
    }                                                                          \
  }

/* World-collision response at `pos`: keep the ground probe only for a ledge
   or glide contact facing the motion, slide off steep walls (unless the
   surface is a trigger of kind 6), take the resolved position, latch the
   first contact and limit the correction to 0x140 from `from`. */
#define SLIDE(from, SET_WALL)                                                  \
  {                                                                            \
    int s = g_nSpyroState;                                                     \
                                                                               \
    if (s == 0xF || (unsigned)(s - 0x20) < 2 || s == 0x22) {                   \
      SubtractVector(d, g_anCollisionContactPoint, pos);                       \
      SetVector3Magnitude(d, 0x100);                                           \
      if (d[2] >= -0xC8) {                                                     \
        int *v = pos + SPY_VELOCITY_FP6;                                       \
        CopyVector(n, v);                                                      \
        SetVector3Magnitude(n, 0x100);                                         \
        if ((len = (d[0] * n[0] + d[1] * n[1] + d[2] * n[2]) >> 8) < 0xE1) {   \
          goto keep##from;                                                     \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    g_nSpyroGroundProbeSuspendFlag = 0;                                        \
    keep##from : if ((signed char)ArcTan2(                                     \
                         g_anCollisionProbeVec[2],                             \
                         VectorLength(g_anCollisionProbeVec, VEC_LEN_XY),      \
                         ATAN_ANGLE8) >= 0x17) {                               \
      int t = g_nGroundSurfaceType & 0x3F;                                     \
      unsigned char kind;                                                      \
                                                                               \
      if (t != 0x3F) {                                                         \
        kind = **(unsigned char **)((char *)g_pTriggerRegionTable + t * 4);    \
      } else {                                                                 \
        kind = 0;                                                              \
      }                                                                        \
      if (kind != 6) {                                                         \
        SET_WALL;                                                              \
      }                                                                        \
    }                                                                          \
    CopyVector(pos, g_anCollisionResolvedPos);                                 \
    if (g_nSpyroFirstContactChunkId == 0xFF) {                                 \
      g_nSpyroFirstContactChunkId = g_nGroundSurfaceType;                      \
      CopyVector(pos + SPY_FIRST_POINT, g_anCollisionContactPoint);            \
      CopyVector(pos + SPY_FIRST_NORMAL, g_anCollisionProbeVec);               \
    }                                                                          \
    LIMIT_STEP(pos, from);                                                     \
  }

/* Wall-slide contributions: the first pass takes a fresh flag pointer, the
   retry pass a hoisted nudge pointer. */
#define WALL_FIRST                                                             \
  {                                                                            \
    int *w = WS;                                                               \
    *w = one;                                                                  \
    SetVector3Magnitude(g_anCollisionProbeVec, 0x400);                         \
    w++;                                                                       \
    AddVector(w, w, g_anCollisionProbeVec);                                    \
  }
#define WALL_RETRY                                                             \
  {                                                                            \
    *b = one;                                                                  \
    SetVector3Magnitude(g_anCollisionProbeVec, 0x400);                         \
    AddVector(p, p, g_anCollisionProbeVec);                                    \
  }

/* Spyro's world collision step (0x8003fe40, 4392 bytes). Applies the pending
   impulse, then the scripted requests (state, velocity toward a target,
   euler, camera modes, the enemy-lock/ledge approach with its alignment test).
   Moves Spyro along the frame velocity in substeps of at most 0x154,
   resolving actor contacts and world collisions (wall-slide nudges, the first
   contact, at most 0x140 of correction per try, 6 tries then 6 more from the
   substep start) and finally derives the frame's motion vector. */
int AdvanceSpyroPhysics(void) {
  int d[3];
  int n[3];
  int start[3];
  int e[3];
  int i;
  int steps;
  int len;
  int hole;

  g_nSpyroContactedActorFlag = 0;
  g_nSpyroFirstContactChunkId = 0xFF;
  g_nSpyroWallSlideContributedFlag = 0;
  {
    int *q = &SPY[SPY_PROBE_SUSPEND];

    (*q)++;
  }
  ZeroVector(&SPY[SPY_NUDGE]);
  AddVector(SPY, SPY, &SPY[SPY_IMPULSE]);
  ZeroVector(&SPY[SPY_IMPULSE]);
  CopyVector(&SPY[SPY_PREV_POS], SPY);
  if ((int)g_dwSpyroRequestMask < 0) {
    unsigned int m;
    register unsigned int t asm("$4");
    unsigned int *rq;
    unsigned int *rq2;

    if ((g_dwSpyroRequestMask & SPYRO_RQ_FORCE_STATE) &&
        SPY[SPY_SPARX_HEALTH] >= 0 &&
        SPY[SPY_STATE] != SPY[SPY_REQUEST_STATE]) {
      ChangeSpyroState(SPY[SPY_REQUEST_STATE]);
      SPY[SPY_AIRBORNE] = 1;
    }
    rq = REQ;
    m = *rq;
    if (m & (SPYRO_RQ_VEL_X | SPYRO_RQ_VEL_Y | SPYRO_RQ_VEL_Z)) {
      if (m & SPYRO_RQ_RAW_VELOCITY) {
        if (m & SPYRO_RQ_VEL_X) {
          g_anSpyroVelocityFp6[0] = g_anSpyroRequestVelocity[0] << 6;
          g_anSpyroVelocity[0] = g_anSpyroVelocityFp6[0] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Y) {
          g_anSpyroVelocityFp6[1] = g_anSpyroRequestVelocity[1] << 6;
          g_anSpyroVelocity[1] = g_anSpyroVelocityFp6[1] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Z) {
          g_anSpyroVelocityFp6[2] = g_anSpyroRequestVelocity[2] << 6;
          g_anSpyroVelocity[2] = g_anSpyroVelocityFp6[2] / g_nFrameStep;
        }
      } else {
        if (m & SPYRO_RQ_VEL_X) {
          g_anSpyroVelocityFp6[0] =
              (g_anSpyroRequestTargetPos[0] - g_anSpyroWorldPos[0]) << 6;
          g_anSpyroVelocity[0] = g_anSpyroVelocityFp6[0] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Y) {
          g_anSpyroVelocityFp6[1] =
              (g_anSpyroRequestTargetPos[1] - g_nSpyroWorldPosY) << 6;
          g_anSpyroVelocity[1] = g_anSpyroVelocityFp6[1] / g_nFrameStep;
        }
        if (m & SPYRO_RQ_VEL_Z) {
          g_anSpyroVelocityFp6[2] =
              (g_anSpyroRequestTargetPos[2] - g_nSpyroWorldPosZ) << 6;
          g_anSpyroVelocity[2] = g_anSpyroVelocityFp6[2] / g_nFrameStep;
        }
      }
      g_nSpyroMotionSpeed = VectorLength((int *)(rq - 0x40), VEC_LEN_XY);
    }
    rq2 = REQ;
    t = *rq2;
    if (t & SPYRO_RQ_PITCH) {
      g_abSpyroPersistentEuler[0] = g_abSpyroRequestEuler[0];
      g_nSpyroBodyPitch = g_abSpyroRequestEuler[0] << 4;
    }
    m = t;
    if (m & SPYRO_RQ_ROLL) {
      g_abSpyroPersistentEuler[1] = g_abSpyroRequestEuler[1];
      g_nSpyroBodyRoll = g_abSpyroRequestEuler[1] << 4;
    }
    if (m & SPYRO_RQ_YAW) {
      g_abSpyroPersistentEuler[2] = g_abSpyroRequestEuler[2];
      g_nSpyroBodyYaw = g_abSpyroRequestEuler[2] << 4;
    }
    if (g_nCameraNextMode_blk[0] != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
      if (m & SPYRO_RQ_CAMERA) {
        g_nCameraTargetYawBiasFromMode = 0;
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_REQUEST);
        g_pCameraAnchorPos = g_pSpyroRequestCameraAnchor;
        g_pCameraTargetParams = g_pSpyroRequestCameraTargetParams;
      } else if (m & SPYRO_RQ_PORTAL_PATH_CAMERA) {
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_PORTAL_PATH);
      } else if (m & SPYRO_RQ_LOCK_ON) {
        int keepYaw = g_nSpyroMoveTargetYaw;
        int a;
        unsigned int lock;

        a = (g_pScriptedCameraActor->rotZ - (g_nCameraEulerYaw >> 4)) & 0xFF;
        if (a > 0x80) {
          a -= 0x100;
        }
        lock = m | SPYRO_RQ_FREEZE_INPUT;
        if (abs(a) < 0x10) {
          if (g_nSpyroScriptedAlignFrames >= 0x1E) {
            ComputeSpyroMoveTargetFromPad(g_nSpyroMoveTargetMag);
            if (g_nSpyroSpeedTarget != 0) {
              int side;

              SubtractVector(e, (int *)&rq2[-SPY_REQUEST],
                             ACTOR_POS(g_pScriptedCameraActor));
              a = ((ArcTan2(e[0], e[1], ATAN_ANGLE8) -
                    g_pScriptedCameraActor->rotZ) +
                   0x80) &
                  0xFF;
              if (a > 0x80) {
                a -= 0x100;
              }
              if (g_nActiveLevelId == LEVEL_PEACE_KEEPERS) {
                side = 0x28;
                if ((int)rq2[SPY_STATE - SPY_REQUEST] ==
                    SPYRO_ST_SCRIPTED_WALK) {
                  side = 0x32;
                }
              } else {
                side = 0x30;
                if ((int)rq2[SPY_STATE - SPY_REQUEST] ==
                    SPYRO_ST_SCRIPTED_WALK) {
                  side = 0x38;
                }
              }
              if (abs(a) < side) {
                int h;
                int t;

                SubtractVector(e, ACTOR_POS(SPY[SPY_SCRIPTED_ACTOR]), SPY);
                a = ArcTan2(e[0], e[1], ATAN_ANGLE12);
                h = g_pScriptedCameraActor->rotZ;
                a = (a - (h << 4)) & 0xFFF;
                if (a < 0x800) {
                  side = 1;
                  t = h + 0x40;
                } else {
                  side = 0;
                  t = h - 0x40;
                }
                a = t << 4;
                SCHED_FENCE();
                a = (a - g_nSpyroMoveTargetYaw) & 0xFFF;
                if (a > 0x800) {
                  a -= 0x1000;
                }
                if (abs(a) < 0x200) {
                  if (g_nSpyroState != SPYRO_ST_SCRIPTED_WALK) {
                    ChangeSpyroState(SPYRO_ST_SCRIPTED_WALK);
                  }
                  g_dwSpyroRequestMask |= SPYRO_RQ_FORCE_STATE;
                  if (side) {
                    g_nSpyroSubstate = 1;
                  } else {
                    g_nSpyroSubstate = 2;
                  }
                }
              } else {
                g_nSpyroTurnRateAccum = 0;
              }
            } else {
              g_nSpyroTurnRateAccum = 0;
            }
          } else {
            g_nSpyroTurnRateAccum = 0;
            *rq2 = lock;
            if (g_nSpyroInputBlankCountdown < 2) {
              g_nSpyroInputBlankCountdown = 2;
            }
          }
          g_nSpyroScriptedAlignFrames += g_nFrameStep;
        } else {
          g_nSpyroScriptedAlignFrames = 0;
          *rq2 = lock;
          if (g_nSpyroInputBlankCountdown < 2) {
            g_nSpyroInputBlankCountdown = 2;
          }
        }
        g_pCameraAnchorPos = g_anCameraLatchedAnchorPos;
        CopyVector(g_anCameraLatchedAnchorPos,
                   ACTOR_POS(g_pScriptedCameraActor));
        g_anCameraLatchedAnchorPos[2] += 0x200;
        g_nCameraTargetYawBiasFromMode = g_pScriptedCameraActor->rotZ << 4;
        g_pCameraTargetParams = g_anCameraParamEnemyLock;
        g_nCameraNextMode = (CAM_HELD | CAM_MODE_REQUEST);
        g_nSpyroMoveTargetYaw = keepYaw;
        g_dwSpyroRequestMask |= SPYRO_RQ_CAMERA;
      } else if (m & SPYRO_RQ_TARGET_CAMERA) {
        g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_SCRIPTED_TARGET);
      } else {
        register unsigned int r asm("$2") = t & SPYRO_RQ_FOLLOW_CAMERA;

        if (r) {
          g_nCameraNextMode_blk[0] = (CAM_HELD | CAM_MODE_SCRIPTED_FOLLOW);
        }
      }
    }
    if (g_dwSpyroRequestMask & SPYRO_RQ_SCRIPTED_WALK) {
      g_nSpyroScriptedMoveMode = 1;
      ChangeSpyroState(SPYRO_ST_WALK);
    }
  } else {
    SPY[SPY_REQUEST] = 0;
  }
  RShiftVector3(g_anSpyroVelocityFp6, 6);
  if (g_dwSpyroRequestMask & SPYRO_RQ_NO_CLIP) {
    AddVector(g_anSpyroVelocityFp6 - SPY_VELOCITY_FP6,
              g_anSpyroVelocityFp6 - SPY_VELOCITY_FP6, g_anSpyroVelocityFp6);
  } else {
    len = VectorLength(g_anSpyroVelocityFp6, VEC_LEN_XYZ);
    steps = len / 340 + 1;
    if (steps >= 2) {
      g_anSpyroVelocityFp6[0] /= steps;
      g_anSpyroVelocityFp6[1] /= steps;
      g_anSpyroVelocityFp6[2] /= steps;
    }
    i = 0;
    if (steps > 0) {
      register int *fp6 asm("$16") = g_anSpyroVelocityFp6;
      int *from = fp6 + 3;
      int *pos = fp6 - SPY_VELOCITY_FP6;
      register int *st asm("$21") = fp6 - (SPY_VELOCITY_FP6 - SPY_STATE);
      register int one asm("$20") = 1;

      do {
        int tries;
        int *p;

        CopyVector(from, pos);
        CopyVector(start, pos);
        AddVector(pos, pos, from - 3);
        tries = 0;
        p = pos + SPY_CONTACTED;
        do {
          int mask = 0;
          void *hit;

          switch (*st) {
          case SPYRO_ST_CHARGE_AIR:
            if (st[1] & SPYRO_SUB_SUPERCHARGE) {
              hit = DispatchActorContactAtSphere(
                  pos, 0x164, &g_dwSpyroTriggerEventFlags, ACTOR_HIT_SUPER, 0,
                  CONTACT_STOP_AT_SOLID);
              if (hit == 0) {
                goto collide;
              }
              if (st[SPY_TRIGGER_FLAGS - SPY_STATE] & SPYRO_EV_BLOCKS_SUPER) {
                hit = DispatchActorContactAtSphere(pos, 0x164,
                                                   &g_dwSpyroTriggerEventFlags,
                                                   ACTOR_HIT_SUPER, 0, one);
              }
              break;
            }
            /* FALLTHROUGH */
          case SPYRO_ST_CHARGE:
            hit = DispatchActorContactAtSphere(
                st - SPY_STATE, 0x164,
                (unsigned int *)(st - (SPY_STATE - SPY_TRIGGER_FLAGS)),
                ACTOR_HIT_CHARGE, 0, one);
            if (hit == 0) {
              goto collide;
            }
            if (g_nPulseRumbleAmount < 0x64) {
              g_nPulseRumbleAmount = 0x64;
            }
            if (g_nPulseRumbleTimer < 0xC) {
              g_nPulseRumbleTimer = 0xC;
            }
            break;
          case SPYRO_ST_SUPERCHARGE_AIR:
          case SPYRO_ST_SUPERCHARGE:
            mask |= ACTOR_HIT_SUPER;
            hit = DispatchActorContactAtSphere(pos, 0x164,
                                               &g_dwSpyroTriggerEventFlags,
                                               mask, 0, CONTACT_STOP_AT_SOLID);
            if (hit == 0) {
              goto collide;
            }
            if (!(g_dwSpyroTriggerEventFlags & SPYRO_EV_BLOCKS_SUPER)) {
              break;
            }
            /* FALLTHROUGH */
          default:
            hit = DispatchActorContactAtSphere(
                pos, 0x164, &g_dwSpyroTriggerEventFlags, mask, 0, one);
            break;
          }
        contact:
          if (hit != 0) {
            g_pSpyroContactActor = hit;
            *p = one;
            LIMIT_STEP(pos, from);
          }
        collide:
          if (!CollideSphereWithWorldAndActors(pos, 0x164, 0x164)) {
            if (hit == 0) {
              break;
            }
          } else {
            SLIDE(from, WALL_FIRST);
          }
          tries++;
        } while (tries < 6);
        if (tries == 6) {
          int *b;
          int *base;

          tries = 0;
          b = WS;
          p = b + 1;
          base = start;
          do {
            if (!CollideSphereWithWorldAndActors(pos, 0x164, 0x164)) {
              break;
            }
            SLIDE(base, WALL_RETRY);
            tries++;
          } while (tries < 6);
        }
      } while (++i < steps);
    }
  }
  SubtractVector(g_anSpyroMotionVec, g_anSpyroMotionVec - SPY_MOTION,
                 g_anSpyroMotionVec - (SPY_MOTION - SPY_PREV_POS));
  LShiftVector3(g_anSpyroMotionVec, 6);
  g_anSpyroMotionVec[0] /= g_nFrameStep;
  g_anSpyroMotionVec[1] /= g_nFrameStep;
  g_anSpyroMotionVec[2] /= g_nFrameStep;
  g_nSpyroFrameMotionLength = VectorLength(g_anSpyroMotionVec, VEC_LEN_XYZ);
  __asm__ volatile("" : : "g"(hole));
  return 1;
}
