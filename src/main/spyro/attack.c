#include "camera.h"
#include "cd_stream.h"
#include "emit_record.h"
#include "funcs.h"
#include "globals.h"
#include "level_sfx.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad_buttons.h"
#include "sample_bank.h"
#include "sdata.h"
#include "spyro_block.h"
#include "spyro_state.h"
#include "vecmath.h"

SDATA(g_pSpyroContactActor, 4);

/* Charge-state trigger dispatcher (0x80041270, 744 bytes), called from the
   charge states (SPYRO_ST_CHARGE, SPYRO_ST_CHARGE_AIR). Masks the
   trigger-event flags (fewer while the post-hit invulnerability runs) and
   routes them: a bump (bits 0x6 == 4, or
   == 2 with the contact actor within +-0x200 of Spyro's heading) kicks
   SPYRO_ST_CHARGE_BONK; a hit (== 6) outside the invulnerability costs one
   g_nSparxHealth, re-arms the invulnerability and picks the hurt (or, past 0,
   death) state from the flag bits; 0x8000 rebounds off the first contact
   normal; 0x800 forces SPYRO_ST_LIFT; 0x400 spends Sparx health for
   SPYRO_ST_HAZARD. Bit 0x2000 latches the charge hold. Returns nonzero once any
   state changed. */
int DispatchSpyroChargeStateChanges(void) {
  int flags = 0xFFFF;
  int changed = 0;
  int *lock;
  int *ready;
  int hi;
  int d[3];
  int angle;
  int state;

  if (g_nNextLevelId != g_nActiveLevelId) {
    return 0;
  }
  OPAQUE_COPY(lock, &g_nSpyroInvulnFrames);
  if (lock[0] != 0) {
    flags = 0xFFFF & ~SPYRO_EV_HITS;
  }
  flags &= g_dwSpyroTriggerEventFlags;
  switch (flags & SPYRO_EV_CHARGE_HIT) {
  case SPYRO_EV_CONTACT_FRONT:
    SubtractVector(d, &g_pSpyroContactActor->posX,
                   lock - 0x58); /* g_anSpyroWorldPos, 0x58 words below the
                                    invulnerability counter */
    angle =
        (ArcTan2(d[0], d[1], ATAN_ANGLE12) - g_nSpyroBodyYaw) & ANGLE12_MASK;
    if (angle > ANGLE12_HALF) {
      angle -= ANGLE12_TURN;
    }
    if (abs(angle) >= 0x200) {
      break;
    }
    /* FALLTHROUGH */
  case SPYRO_EV_CONTACT:
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    changed = 1;
    break;
  case SPYRO_EV_CHARGE_HIT:
    if (lock[0] == 0 && lock[1] >= 0) {
      if (g_bDebugNoDepletion == 0) {
        lock[1]--;
      }
      lock[0] = SPYRO_HIT_INVULN_FRAMES;
      if (flags & SPYRO_EV_HIT_PINNED) {
        state = SPYRO_ST_HURT_PINNED;
        if (lock[1] < 0) {
          state = SPYRO_ST_DEATH_PINNED;
        }
      } else if (flags & SPYRO_EV_HIT_TINT) {
        state = SPYRO_ST_HURT_TINT;
      } else if (flags & SPYRO_EV_HIT_LONG) {
        state = SPYRO_ST_HURT_LONG;
      } else if (flags & SPYRO_EV_KNOCKBACK_2) {
        state = SPYRO_ST_KNOCKBACK_2;
      } else if (flags & SPYRO_EV_KNOCKBACK) {
        state = SPYRO_ST_KNOCKBACK;
      } else {
        state = SPYRO_ST_HURT;
        if (flags & SPYRO_EV_HAZARD) {
          state = SPYRO_ST_HAZARD;
        }
      }
      ChangeSpyroState(state);
      return 1;
    }
    break;
  }
  hi = flags & 0xFC00;
  if (flags & SPYRO_EV_REBOUND) {
    ChangeSpyroState(SPYRO_ST_CHARGE_BONK);
    SetVector3Magnitude(g_anSpyroFirstContactNormal, 0x800);
    CopyVector(
        SPY_VIA(g_anSpyroFirstContactNormal, SPY_FIRST_NORMAL, SPY_VELOCITY),
        g_anSpyroFirstContactNormal);
    changed = 1;
  }
  if ((flags & SPYRO_EV_LIFT) && g_nSpyroState != SPYRO_ST_LIFT) {
    ChangeSpyroState(SPYRO_ST_LIFT);
    changed = 1;
  }
  if (hi & SPYRO_EV_HAZARD) {
    OPAQUE_COPY(ready, &g_nSparxHealth);
    if (*ready < 0) {
      goto hold;
    }
    ResetSpyroLinearMotion();
    if (g_bDebugNoDepletion == 0) {
      (*ready)--;
    }
    if (g_nSpyroInvulnFrames < SPYRO_HIT_INVULN_FRAMES) {
      g_nSpyroInvulnFrames = SPYRO_HIT_INVULN_FRAMES;
    }
    ChangeSpyroState(SPYRO_ST_HAZARD);
    changed = 1;
  }
hold:
  if (hi & SPYRO_EV_CHARGE_HOLD) {
    g_nSpyroChargeHoldFlag = 1;
  } else {
    g_nSpyroChargeHoldFlag = 0;
  }
  return changed;
}

/* Matching notes (TickSpyroAttackEffects).
   - Frame: the untouched 8-byte reload slot after empty/sfxDone is `hole`,
     a local that gets no hard register (every callee-saved reg is live
     across the flame loop), so reload slots it in pseudo order and the asms
     that name it with "g" constraints emit nothing.
   - Flame-loop preheader [&tip][&dir][la mtx][burst giv]: the non-volatile
     `hole = asm(mtx)` sits in one block after the first `dir` movable, so
     loop.c hoists it (block-local user var, lifetime 18 clears the move
     threshold) right after &tip/&dir; sched1's birthing boost then lands
     mtx's `la` immediately before that zero-code consumer, ahead of the
     burst giv init.
   - Slot age/state bytes are givs off the anchor block, the arc-path select
     pins the table base to $2 and the path to $3 with an empty barrier in
     the long arm, and the product `k` is its own local. */

/* The flame-slot arrays and anchor vectors reached from the held anchor-matrix
   and anchor-mode bases (byte offsets resolve to the named globals). */
/* g_abSpyroFlameSlotAge / g_abSpyroFlameSlotState */
#define FLAME_SLOT_AGE ((unsigned char *)&g_anSpyroFlameAnchorMtxBlock[-0x21])
#define FLAME_SLOT_STATE ((unsigned char *)&g_anSpyroFlameAnchorMtxBlock[-0x1d])
/* g_anSpyroFlameAnchorPos */
#define FLAME_ANCHOR_POS ((int *)(&g_abSpyroFlameAnchorModeBlock[-0x9a]))
/* g_anSpyroWorldPos, from the euler and body-matrix bases */
#define SPYRO_POS_FROM_EULER ((int *)(&g_abSpyroPersistentEulerBlock[-0xc]))
#define SPYRO_POS_FROM_MTX (&g_anSpyroBodyMtxBlock[-13])
/* g_anSpyroFlameAnchorMtx / g_anSpyroFlameAnchorOffset */
#define FLAME_ANCHOR_MTX (&g_abSpyroFlameAnchorModeBlock[0xa])
#define FLAME_ANCHOR_OFFSET ((int *)(&g_abSpyroFlameAnchorModeBlock[0x32]))

/* 0x80048d10 (2248 bytes) — per-tick attack-effects driver, two independent
   subsystems sharing one function.  (1) Superflame glow: while
   g_nSpyroSuperflameTimer runs, spawn one particle per frame at a random offset
   in Spyro's body frame and pulse his red tint (g_abSpyroTintRgba; the
   alpha follows a sine, and blinks off for part of every 24-frame cycle once
   the timer is under SUPERFLAME_WARN_FRAMES, warning that the power-up is
   running out). (2) Flame breath: arm this frame's flame slots through the ring
   permutation, rebuild the anchor frame (either the body-frame matrix of the
   current anim frame's packed offset, or a static offset from the world
   position), then walk each of the 8 slots along its arc path — probing actors
   and world geometry between the previous and current point, freezing the slot
   and spawning the hit burst (plus one SFX_ATTACK_HIT per frame) on contact, or
   spawning the end-of-arc burst when the slot runs off the end of its path.
   param steps = frame delta.  Clears g_bSpyroFlameActive once all 8 slots are
   idle.
 */
void TickSpyroAttackEffects(int steps) {
  int flame[3];
  int anchor[3];
  int tip[3];
  int dir[3];
  int empty;
  int sfxDone;
  int hole;

  int k;
  int *wp;
  unsigned char *seg;
  unsigned char *map;
  unsigned char *age;
  unsigned char *state;
  int *mtx;
  int *pt;
  int *cursor;
  int *amtx;
  int *rec;
  int count;
  int limit;
  int mask;
  int n;
  int i;
  empty = 0;
  sfxDone = 0;
  if (g_anSpyroSuperflameTimerBlock[0] != 0) {
    flame[0] = (GetRandomU32() & 0x1ff) - 0x160;
    flame[1] = (GetRandomU32() & 0x7f) - 0x40;
    flame[2] = ((flame[0] >> 1) + (GetRandomU32() & 0x3f)) - 0x60;
    RotateVectorByMatrix(g_anSpyroBodyMtxBlock, flame, flame);
    AddVector(flame, flame, SPYRO_POS_FROM_MTX);
    g_pfnLevelEmitParticles(1, EMIT_RISING_PUFF, flame, 0);
    g_abSpyroTintRgba[0] = SUPERFLAME_TINT_R;
    g_abSpyroTintRgba[1] = SUPERFLAME_TINT_G;
    g_abSpyroTintRgba[2] = SUPERFLAME_TINT_B;
    if ((g_anSpyroSuperflameTimerBlock[0] < SUPERFLAME_WARN_FRAMES) &&
        ((g_anSpyroSuperflameTimerBlock[0] % SUPERFLAME_BLINK_PERIOD) >=
         SUPERFLAME_BLINK_OFF)) {
      g_abSpyroTintRgba[3] = 0;
    } else {
      g_abSpyroTintRgba[3] = (LookupSine(g_nSpyroSuperflameTimer << 7) / 256) -
                             SUPERFLAME_TINT_ALPHA_BIAS;
    }
    g_anSpyroSuperflameTimerBlock[0] -= g_nFrameStep;
    if (g_anSpyroSuperflameTimerBlock[0] <= 0) {
      g_anSpyroSuperflameTimerBlock[0] = 0;
      g_abSpyroTintRgba[3] = 0;
    }
  }
  if (g_bSpyroFlameActive != 0) {
    wp = g_anSpyroWorldPos;
    if ((g_nSpyroAirborneFrames == 0) &&
        (g_abSpyroPersistentEulerBlock[1] >= 0x81)) {
      g_nSpyroSwingEulerTargetY =
          (0x100 - g_abSpyroPersistentEulerBlock[1]) * 8;
    } else {
      g_nSpyroSwingEulerTargetY = 0;
    }
    n = 0;
    if (steps > 0) {
      seg = g_abSpyroFlameSegmentBlock;
      map = g_abSpyroFlameSlotMap;
      do {
        if (seg[0] < 8) {
          g_abSpyroFlameSlotAge[(map + (seg[2] * 8))[seg[0]]] = steps - n;
          g_abSpyroFlameSlotEndAge[(map + (seg[2] * 8))[seg[0]]] =
              FLAME_SLOT_END_AGE;
          g_abSpyroFlameSlotState[(map + (seg[2] * 8))[seg[0]]] = 0;
        }
        seg[0]++;
      } while ((++n) < steps);
    }
    /* the word at g_bSpyroAnimPrev packs the primary prev/current anims and
       the layer-1 prev/current anims (bytes 2/3): layer 1 holding anim 7 */
    if (((g_bSpyroFlameSegmentIdx < 0x1e) &&
         (((*((int *)(&g_bSpyroAnimPrev))) & 0xffff0000) == 0x07070000)) &&
        (g_bSpyroAnimLayer1FramePrev == 4)) {
      g_abSpyroAnimLayer1SubstepBlock[0] = 0;
    }
    if (g_abSpyroFlameAnchorModeBlock[0] != 0) {
      g_abSpyroFlameAnchorEuler[0] = g_abSpyroPersistentEulerBlock[0];
      g_abSpyroFlameAnchorEuler[1] = g_abSpyroPersistentEulerBlock[1];
      g_abSpyroFlameAnchorEuler[2] = g_abSpyroPersistentEulerBlock[2];
      CopyWords(FLAME_ANCHOR_MTX,
                &g_abSpyroPersistentEulerBlock[EULER_TO_COMPOSED_MTX], 0x14);
      rec = (int *)((g_bSpyroFramePrev * 4) +
                    (*((int *)(((g_bSpyroAnimPrev * 4) +
                                ((int)g_apActorMeshTable)) +
                               0x38))));
      n = ((int *)((rec[9] & 0x1fffff) * 2))[4];
      anchor[0] = n >> 21;
      anchor[1] = (n << 11) >> 21;
      anchor[2] = (n << 22) >> 21;
      RotateVectorByMatrix(
          (int *)(&g_abSpyroPersistentEulerBlock[EULER_TO_BODY_MTX]), anchor,
          anchor);
      AddVector(FLAME_ANCHOR_POS, anchor, SPYRO_POS_FROM_EULER);
      if (g_bSpyroFlameSegmentIdx < 8) {
        CopyVector(
            anchor,
            &g_anSpyroFlameWindupAnchorOffsets[(g_bSpyroFlameSegmentIdx >> 1) *
                                               3]);
      } else {
        CopyVector(anchor, g_anSpyroFlameFinalAnchorOffset);
      }
      amtx = g_anSpyroFlameAnchorMtxBlock;
      RotateVectorByMatrix(amtx, anchor, anchor);
      AddVector(&amtx[-0x29], &amtx[-0x29], anchor);
      SubtractVector(&amtx[0xa], &amtx[-0x29], wp);
    } else {
      AddVector(FLAME_ANCHOR_POS, wp, FLAME_ANCHOR_OFFSET);
    }
    i = 0;
    mtx = g_anSpyroFlameAnchorMtxBlock;
    pt = &mtx[0xd];
    do {
      if (FLAME_SLOT_AGE[i] != 0) {
        if (i < 4) {
          count = FLAME_SHORT_ARC_POINTS;
          limit = FLAME_SHORT_ARC_LIFE;
        } else {
          count = FLAME_LONG_ARC_POINTS;
          limit = FLAME_LONG_ARC_LIFE;
        }
        if (FLAME_SLOT_STATE[i] != 0) {
          FLAME_SLOT_AGE[i] += steps;
        }
        if (FLAME_SLOT_AGE[i] >= limit) {
          FLAME_SLOT_AGE[i] = 0;
        } else if (FLAME_SLOT_STATE[i] != 2) {
          register short *tbl asm("$2");
          register short *path asm("$3");

          if (i < 4) {
            k = i * count;
            tbl = g_anSpyroFlameArcPathsShort;
          } else {
            k = (i - 4) * count;
            SCHED_FENCE();
            tbl = g_anSpyroFlameArcPathsLong;
          }
          path = (short *)(k * 8 + (int)tbl);
          if (FLAME_SLOT_AGE[i] < (count - 1)) {
            SVectorToVector(tip, &path[(FLAME_SLOT_AGE[i] + 1) * 4]);
            RotateVectorByMatrix(mtx, tip, tip);
            AddVector(tip, tip, &mtx[-0x29]);
            if (FLAME_SLOT_STATE[i] == 0) {
              CopyVector(dir, wp);
              FLAME_SLOT_STATE[i] = 1;
            } else {
              CopyVector(dir, &pt[i * 3]);
            }
            CopyVector(&pt[i * 3], tip);
            mask = ACTOR_HIT_FLAME;
            if ((*((volatile int *)(&pt[-0xf]))) != 0) {
              mask = ACTOR_HIT_FLAME | ACTOR_HIT_SUPER;
            }
            if ((DispatchActorContactAtSphere(tip, FLAME_HIT_RADIUS, 0, mask, 0,
                                              CONTACT_STOP_AT_SOLID) != 0) ||
                (CastRayWorldAndActors(dir, tip) != 0)) {
              FLAME_SLOT_STATE[i] = 2;
              g_abSpyroFlameSlotEndAge[i] = FLAME_SLOT_AGE[i];
              dir[0] = -0x10;
              dir[1] = (GetRandomU32() & 0x1f) - 0x10;
              dir[2] = 0;
              RotateVectorByMatrix(&pt[-0xd], dir, dir);
              dir[2] = 0;
              g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, tip, (int)dir);
              if ((*((volatile int *)(&pt[-0xf]))) == 0) {
                g_pfnLevelEmitParticles(4, EMIT_EMBER, tip, 0);
              }
              __asm__("" : "=g"(hole) : "r"(mtx));
              dir[0] = 0x30;
              dir[1] = 0;
              dir[2] = 0;
              ApplyActiveGteRotation(dir, dir);
              g_pfnLevelEmitParticles(((*((volatile int *)(&pt[-0xf]))) * 5) +
                                          5,
                                      EMIT_FLAME_STREAK, tip, (int)dir);
              __asm__ volatile("" : : "g"(hole));
              if (sfxDone == 0) {
                PlaySoundEffect(LEVEL_SFX(SFX_ATTACK_HIT), (Actor *)wp,
                                SFX_MODE_FLAT, 0);
                sfxDone = 1;
              }
            }
          } else if ((count < FLAME_SLOT_AGE[i]) &&
                     ((count + steps) >= FLAME_SLOT_AGE[i])) {
            SVectorToVector(tip, &path[(count - 1) * 4]);
            RotateVectorByMatrix(mtx, tip, tip);
            AddVector(tip, tip, &mtx[-0x29]);
            ZeroVector(dir);
            if (g_nSpyroFlameVariant != 0) {
              g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, tip, (int)dir);
            } else {
              g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, tip, (int)dir);
            }
            if (g_nSpyroFlameVariant == 0) {
              g_pfnLevelEmitParticles(4, EMIT_EMBER, tip, 0);
            }
            ApplyActiveGteRotation(&g_anSpyroFlameEndBurstDirs[i * 3], dir);
            g_pfnLevelEmitParticles((g_nSpyroFlameVariant * 5) + 5,
                                    EMIT_FLAME_STREAK, tip, (int)dir);
          }
        }
      } else {
        empty++;
      }
    } while ((++i) < 8);
    if (empty == 8) {
      g_bSpyroFlameActive = 0;
    }
  }
}

#undef FLAME_SLOT_AGE
#undef FLAME_SLOT_STATE
#undef FLAME_ANCHOR_POS
#undef SPYRO_POS_FROM_EULER
#undef SPYRO_POS_FROM_MTX
#undef FLAME_ANCHOR_MTX
#undef FLAME_ANCHOR_OFFSET

/* Flame-breath driver (0x800499c0, 1084 bytes).

   Load-bearing forms:
   - the strike state block pointer is a `$17` register variable scoped to
     the non-cancel branch. As a pseudo it wins s0 on local priority and every
     short-lived base shifts up one callee-saved register.
   - the two tip-spawn halves are separate blocks with their own `tip` (and
     `mtx`) locals, so each base is a fresh short-lived pseudo in s0/s1.
   - five allow_duplicated alias blocks (state, timer, anchor mode,
     collision-active, composed matrix) and a DISTINCT tip-offset alias at +0xC
     (A24) so cse cannot derive it from the first.
   - the variant select as two complete spawn calls in the two arms, which
     jump.c cross-jumps into one call (A80) instead of `sltu a1,zero,a1`.
   - a separate pointer local in the cancel arm (caller-saved), and the
     anchor-mode store in the breath arm through its alias pointer, which forces
     the original's strike-timer reload after it. */

/* Flame-breath (Circle) driver, one frame (0x800499C0, 0x43C).

   If the animation currently selected for g_nSpyroState is flagged as a
   no-overlay anim (SPYRO_ANIM_NO_OVERLAY), the breath is cancelled: the
   state word is zeroed and the swing euler target reset.

   Otherwise the breath runs as a two-state machine on g_nSpyroFlameState:

   0 (idle) — a fresh press of PAD_CIRCLE while no flame is live starts a
   breath: state 1, timer -1, phase 0, anim rate from the flame anim
   descriptor, g_bSpyroFlameActive on, segment 0, anchor mode 1, a random ring
   base, the variant (1 while g_nSpyroSuperflameTimer runs), the eight flame
   slot ages cleared and the swing euler target zeroed.

   1 (breathing) — the anchor mode drops at timer 0x10. Every 4th frame of the
   0xC..0x1C window (and only while the CD stream is idle) two flame particles
   are spawned through the level overlay hook, one off the composed body matrix
   reached from the state block, one off the matrix itself, each positioned at
   Spyro's world position plus a rotated tip offset. Outside that window a fresh
   press re-arms a second breath from frame 0x2C, and at frame 0x30 the breath
   ends: flame off, slot ages cleared, back to state 0.

   Every non-cancelled frame ends by ticking the strike timer, and the frame
   always finishes in SmoothSpyroByteEulerSpring. */

/* The body below started as decomp-permuter output (2026-08-14-1) and was
   carried to a full match from there; the earlier hand-written draft and its
   comments are in git history (7623bbd6^). */
void TickSpyroFlameAttack(void) {
  int pos[4];
  int vel[4];
  int *timer;
  int *cancel;
  unsigned char *anchor;
  unsigned char *live;
  if (SPYRO_ANIM_NO_OVERLAY(g_abSpyroStateAnimIndexMap[g_nSpyroState]) != 0) {
    cancel = g_anSpyroFlameStateBlock;
    cancel[0] = 0;
    ZeroVector(&cancel[3]);
  } else {
    register int *strike asm("$17") = g_anSpyroFlameStateBlock;

    switch (strike[0]) {
    case SPYRO_FLAME_IDLE:
      anchor = g_abSpyroFlameAnchorModeBlock;
      anchor[0] = 0;
      if ((g_dwPadPressed & PAD_CIRCLE) && (g_bSpyroFlameActive == 0)) {
        strike[0] = SPYRO_FLAME_BREATHING;
        g_nSpyroFlameTimer = -1;
        g_nSpyroFlamePhase = 0;
        g_nSpyroFlameAnimRate = g_abSpyroAnimDescTable
            [(g_abSpyroFlameAnimTable[g_nSpyroFlameState] * 4) +
             SPYRO_ANIM_DESC_RATE];
        g_bSpyroFlameActive = 1;
        g_bSpyroFlameSegmentIdx = 0;
        anchor[0] = 1;
        g_bSpyroFlameAnimRingBase = GetRandomU32() & 1;
        if (g_nSpyroSuperflameTimer != 0) {
          g_nSpyroFlameVariant = 1;
        } else {
          g_nSpyroFlameVariant = 0;
        }
        FillWord(g_abSpyroFlameSlotAge, 0, 8);
        ZeroVector(&g_nSpyroSwingEulerTargetX);
      }
      break;

    case SPYRO_FLAME_BREATHING:
      if (g_nSpyroFlameTimer == FLAME_ANCHOR_DROP_FRAME) {
        anchor = g_abSpyroFlameAnchorModeBlock;
        anchor[0] = 0;
      }
      if ((((g_nSpyroFlameTimer & (FLAME_EMIT_PERIOD - 1)) == 0) &&
           (((unsigned int)(g_nSpyroFlameTimer - FLAME_EMIT_FIRST)) <
            FLAME_EMIT_LAST - FLAME_EMIT_FIRST + 1)) &&
          (g_nCdStreamState < LSTREAM_PRESERVE)) {
        {
          int *tip = g_anSpyroFlameTipOffsets;
          RotateVectorByMatrix(&strike[0xC], tip, pos);
          AddVector(pos, pos, (int *)(((char *)strike) - 0x198));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroFlameVariant != 0) {
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, pos, (int)vel);
          } else {
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, pos, (int)vel);
          }
        }
        {
          int *mtx = g_anSpyroComposedBodyMtxBlock;
          int *tip = g_anSpyroFlameTipOffsets2;
          RotateVectorByMatrix(mtx, tip, pos);
          AddVector(pos, pos, (int *)(((char *)mtx) - 0x1C8));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroFlameVariant != 0) {
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL_DIM, pos, (int)vel);
          } else {
            g_pfnLevelEmitParticles(1, EMIT_SPARK_VEL, pos, (int)vel);
          }
        }
      } else if ((g_dwPadPressed & PAD_CIRCLE) &&
                 ((timer = g_anSpyroFlameTimerBlock)[0] >= FLAME_REARM_FRAME)) {
        timer[0] = -1;
        g_nSpyroFlamePhase = 2;
        g_abSpyroAnimLayer1SubstepBlock[0] = 4;
        g_bSpyroAnimLayer1Frame = 0;
        g_bSpyroFlameActive = 1;
        g_bSpyroFlameSegmentIdx = 0;
        g_bSpyroFlameAnchorMode = 1;
        g_bSpyroFlameAnimRingBase = GetRandomU32() & 1;
        if (g_nSpyroSuperflameTimer != 0) {
          g_nSpyroFlameVariant = 1;
        } else {
          g_nSpyroFlameVariant = 0;
        }
        FillWord(g_abSpyroFlameSlotAge, 0, 8);
      } else {
        if (g_anSpyroFlameTimerBlock[0] >= FLAME_END_FRAME) {
          live = g_abSpyroFlameActiveBlock;
          live[0] = 0;
          FillWord(((char *)live) - 0x78, 0, 8);
          g_nSpyroFlameState = SPYRO_FLAME_IDLE;
          g_anSpyroFlameTimerBlock[0] = -1;
          g_nSpyroFlameAnimRate = g_abSpyroAnimDescTable
              [(g_abSpyroFlameAnimTable[g_nSpyroFlameState] * 4) +
               SPYRO_ANIM_DESC_RATE];
          ZeroVector(
              &g_anSpyroFlameTimerBlock[SPY_SWING_TARGET - SPY_FLAME_TIMER]);
        }
      }
      break;

    default:
      if (g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
        ZeroVector(&g_nSpyroSwingEulerTargetX);
      }
      break;
    }

    timer = g_anSpyroFlameTimerBlock;
    timer[0] += 1;
  }
  SmoothSpyroByteEulerSpring();
}
