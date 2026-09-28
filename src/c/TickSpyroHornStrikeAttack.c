#include "globals.h"

/* Horn-strike swing driver (0x800499c0, 1084 bytes).

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
     anchor-mode store in the swing arm through its alias pointer, which forces
     the original's strike-timer reload after it. */

/* Horn-strike (charge attack) driver, one frame (0x800499C0, 0x43C).

   If the animation currently selected for g_nSpyroState is flagged as a
   no-strike anim (g_abSpyroAnimDescTable +0xB8), the strike is cancelled: the
   state word is zeroed and the swing euler target reset.

   Otherwise the strike runs as a two-state machine on g_nSpyroHornStrikeState:

   0 (idle) — a fresh press of the attack button (pad bit 0x20) while the horn
   is not already live arms the swing: state 1, timer -1, swing phase 0, anim
   rate from the strike anim descriptor, collision on, segment 0, anchor mode 1,
   a random ring base, the variant taken from the flame-breath timer, the eight
   slot ages cleared and the swing euler target zeroed.

   1 (swinging) — the anchor mode drops at timer 0x10. Every 4th frame of the
   0xC..0x1C window (and only while the CD stream is idle) two spark particles
   are spawned through the level overlay hook, one off the composed body matrix
   reached from the state block, one off the matrix itself, each positioned at
   Spyro's world position plus a rotated tip offset. Outside that window a held
   button re-arms a second swing from frame 0x2C, and at frame 0x30 the swing
   ends: collision off, slot ages cleared, back to state 0.

   Every non-cancelled frame ends by ticking the strike timer, and the frame
   always finishes in SmoothSpyroByteEulerSpring. */
extern void ZeroVector(int *vec);
extern void AddVector(int *dst, int *a, int *b);
extern void RotateVectorByMatrix(int *mtx, int *vec, int *dst);
extern void ApplyActiveGteRotation(int *vec, int *dst);
extern void FillWord(void *dst, unsigned int value, int byteCount);
extern unsigned int GetRandomU32(void);
extern void SmoothSpyroByteEulerSpring(void);

extern unsigned char D_80075268[]; /* state -> strike anim descriptor index */
extern unsigned char D_80078A79;   /* anim layer 1 blend flag */
extern unsigned char g_abSpyroAnimLayer1SubstepBlock[];

extern int g_anSpyroHornStrikeStateBlock[]; /* g_nSpyroHornStrikeState */
extern int g_anSpyroHornStrikeTimerBlock[]; /* g_nSpyroHornStrikeTimer */
extern int g_anSpyroComposedBodyMtxBlock[]; /* g_anSpyroComposedBodyMtx */
extern int
    g_anSpyroHornStrikeTipOffsets2[]; /* g_anSpyroHornStrikeTipOffsets+0xC */
extern unsigned char g_abSpyroHornStrikeAnchorModeBlock[];
extern unsigned char g_abSpyroHornCollisionActiveBlock[];

/* NOTE: the function body below is decomp-permuter output, spliced in
   by the 2026-08-14-1 unattended permuter session for its PARTIAL-BYTE
   gain only. It reads worse than the hand-written form it replaced and
   its inline comments are lost. The hand-written original is recoverable
   with `git show HEAD:src/c/TickSpyroHornStrikeAttack.c.wip`; this body came
   from
   build/permuter/nonmatchings/TickSpyroHornStrikeAttack/output-285-1/source.c.
 */
void TickSpyroHornStrikeAttack(void) {
  int pos[4];
  int vel[4];
  int *timer;
  int *cancel;
  unsigned char *anchor;
  unsigned char *live;
  if (g_abSpyroAnimDescTable[g_abSpyroStateAnimIndexMap[g_nSpyroState] +
                             0xB8] != 0) {
    cancel = g_anSpyroHornStrikeStateBlock;
    cancel[0] = 0;
    ZeroVector(&cancel[3]);
  } else {
    register int *strike asm("$17") = g_anSpyroHornStrikeStateBlock;

    switch (strike[0]) {
    case 0:
      anchor = g_abSpyroHornStrikeAnchorModeBlock;
      anchor[0] = 0;
      if ((g_dwPadPressed & 0x20) && (g_bSpyroHornCollisionActive == 0)) {
        strike[0] = 1;
        g_nSpyroHornStrikeTimer = -1;
        g_nSpyroHornStrikeSwingPhase = 0;
        g_nSpyroHornStrikeAnimRate =
            g_abSpyroAnimDescTable[(D_80075268[g_nSpyroHornStrikeState] * 4) +
                                   3];
        g_bSpyroHornCollisionActive = 1;
        g_bSpyroHornStrikeSegmentIdx = 0;
        anchor[0] = 1;
        g_bSpyroHornStrikeAnimRingBase = GetRandomU32() & 1;
        if (g_nSpyroFlameBreathTimer != 0) {
          g_nSpyroHornStrikeVariant = 1;
        } else {
          g_nSpyroHornStrikeVariant = 0;
        }
        FillWord(g_abSpyroHornStrikeSlotAge, 0, 8);
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
      }
      break;

    case 1:
      if (g_nSpyroHornStrikeTimer == 0x10) {
        anchor = g_abSpyroHornStrikeAnchorModeBlock;
        anchor[0] = 0;
      }
      if ((((g_nSpyroHornStrikeTimer & 3) == 0) &&
           (((unsigned int)(g_nSpyroHornStrikeTimer - 0xC)) < 0x11)) &&
          (g_nCdStreamState < 0)) {
        {
          int *tip = g_anSpyroHornStrikeTipOffsets;
          RotateVectorByMatrix(&strike[0xC], tip, pos);
          AddVector(pos, pos, (int *)(((char *)strike) - 0x198));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroHornStrikeVariant != 0) {
            ((void (*)(int, int, int *, int *))g_pfnLevelOverlayParticleSpawn)(
                1, 1, pos, vel);
          } else {
            ((void (*)(int, int, int *, int *))g_pfnLevelOverlayParticleSpawn)(
                1, 0, pos, vel);
          }
        }
        {
          int *mtx = g_anSpyroComposedBodyMtxBlock;
          int *tip = g_anSpyroHornStrikeTipOffsets2;
          RotateVectorByMatrix(mtx, tip, pos);
          AddVector(pos, pos, (int *)(((char *)mtx) - 0x1C8));
          ApplyActiveGteRotation(tip + 6, vel);
          if (g_nSpyroHornStrikeVariant != 0) {
            ((void (*)(int, int, int *, int *))g_pfnLevelOverlayParticleSpawn)(
                1, 1, pos, vel);
          } else {
            ((void (*)(int, int, int *, int *))g_pfnLevelOverlayParticleSpawn)(
                1, 0, pos, vel);
          }
        }
      } else if ((g_dwPadPressed & 0x20) &&
                 ((timer = g_anSpyroHornStrikeTimerBlock)[0] >= 0x2C)) {
        timer[0] = -1;
        g_nSpyroHornStrikeSwingPhase = 2;
        g_abSpyroAnimLayer1SubstepBlock[0] = 4;
        D_80078A79 = 0;
        g_bSpyroHornCollisionActive = 1;
        g_bSpyroHornStrikeSegmentIdx = 0;
        g_bSpyroHornStrikeAnchorMode = 1;
        g_bSpyroHornStrikeAnimRingBase = GetRandomU32() & 1;
        if (g_nSpyroFlameBreathTimer != 0) {
          g_nSpyroHornStrikeVariant = 1;
        } else {
          g_nSpyroHornStrikeVariant = 0;
        }
        FillWord(g_abSpyroHornStrikeSlotAge, 0, 8);
      } else {
        ;
        if (g_anSpyroHornStrikeTimerBlock[0] >= 0x30) {
          live = g_abSpyroHornCollisionActiveBlock;
          live[0] = 0;
          FillWord(((char *)live) - 0x78, 0, 8);
          g_nSpyroHornStrikeState = 0;
          g_anSpyroHornStrikeTimerBlock[0] = -1;
          g_nSpyroHornStrikeAnimRate =
              g_abSpyroAnimDescTable[(D_80075268[g_nSpyroHornStrikeState] * 4) +
                                     3];
          ZeroVector(&g_anSpyroHornStrikeTimerBlock[1]);
        }
      }
      break;

    default:
      if (g_nCameraCurrentMode != 0x80000009) {
        ZeroVector(&g_nSpyroSwingEulerPitchTarget);
      }
      break;
    }

    timer = g_anSpyroHornStrikeTimerBlock;
    timer[0] += 1;
  }
  SmoothSpyroByteEulerSpring();
}
