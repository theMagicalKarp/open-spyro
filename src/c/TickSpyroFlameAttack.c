#include "camera.h"
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "pad_buttons.h"
#include "vecmath.h"

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
   no-flame anim (g_abSpyroAnimDescTable +0xB8), the breath is cancelled: the
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

/* NOTE: the function body below is decomp-permuter output, spliced in
   by the 2026-08-14-1 unattended permuter session for its PARTIAL-BYTE
   gain only. It reads worse than the hand-written form it replaced and
   its inline comments are lost. The hand-written original is recoverable
   from history as src/c/TickSpyroHornStrikeAttack.c.wip (the function's
   name before it was recognised as the flame breath); this body came from
   the permuter's TickSpyroHornStrikeAttack output-285-1.
 */
void TickSpyroFlameAttack(void) {
  int pos[4];
  int vel[4];
  int *timer;
  int *cancel;
  unsigned char *anchor;
  unsigned char *live;
  if (g_abSpyroAnimDescTable[g_abSpyroStateAnimIndexMap[g_nSpyroState] +
                             0xB8] != 0) {
    cancel = g_anSpyroFlameStateBlock;
    cancel[0] = 0;
    ZeroVector(&cancel[3]);
  } else {
    register int *strike asm("$17") = g_anSpyroFlameStateBlock;

    switch (strike[0]) {
    case 0:
      anchor = g_abSpyroFlameAnchorModeBlock;
      anchor[0] = 0;
      if ((g_dwPadPressed & PAD_CIRCLE) && (g_bSpyroFlameActive == 0)) {
        strike[0] = 1;
        g_nSpyroFlameTimer = -1;
        g_nSpyroFlamePhase = 0;
        g_nSpyroFlameAnimRate = g_abSpyroAnimDescTable
            [(g_abSpyroFlameAnimTable[g_nSpyroFlameState] * 4) + 3];
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
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
      }
      break;

    case 1:
      if (g_nSpyroFlameTimer == 0x10) {
        anchor = g_abSpyroFlameAnchorModeBlock;
        anchor[0] = 0;
      }
      if ((((g_nSpyroFlameTimer & 3) == 0) &&
           (((unsigned int)(g_nSpyroFlameTimer - 0xC)) < 0x11)) &&
          (g_nCdStreamState < 0)) {
        {
          int *tip = g_anSpyroFlameTipOffsets;
          RotateVectorByMatrix(&strike[0xC], tip, pos);
          AddVector(pos, pos, (int *)(((char *)strike) - 0x198));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroFlameVariant != 0) {
            ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                1, 1, pos, vel);
          } else {
            ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                1, 0, pos, vel);
          }
        }
        {
          int *mtx = g_anSpyroComposedBodyMtxBlock;
          int *tip = g_anSpyroFlameTipOffsets2;
          RotateVectorByMatrix(mtx, tip, pos);
          AddVector(pos, pos, (int *)(((char *)mtx) - 0x1C8));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroFlameVariant != 0) {
            ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                1, 1, pos, vel);
          } else {
            ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                1, 0, pos, vel);
          }
        }
      } else if ((g_dwPadPressed & PAD_CIRCLE) &&
                 ((timer = g_anSpyroFlameTimerBlock)[0] >= 0x2C)) {
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
        ;
        if (g_anSpyroFlameTimerBlock[0] >= 0x30) {
          live = g_abSpyroFlameActiveBlock;
          live[0] = 0;
          FillWord(((char *)live) - 0x78, 0, 8);
          g_nSpyroFlameState = 0;
          g_anSpyroFlameTimerBlock[0] = -1;
          g_nSpyroFlameAnimRate = g_abSpyroAnimDescTable
              [(g_abSpyroFlameAnimTable[g_nSpyroFlameState] * 4) + 3];
          ZeroVector(&g_anSpyroFlameTimerBlock[1]);
        }
      }
      break;

    default:
      if (g_nCameraCurrentMode != (CAM_HELD | CAM_MODE_FREE_LOOK)) {
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
      }
      break;
    }

    timer = g_anSpyroFlameTimerBlock;
    timer[0] += 1;
  }
  SmoothSpyroByteEulerSpring();
}
