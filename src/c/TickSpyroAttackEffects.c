/* Matching notes (TickSpyroAttackEffects).
   - Frame: the untouched 8-byte reload slot after empty/sfxDone is `hole`,
     a local that gets no hard register (every callee-saved reg is live
     across the horn loop), so reload slots it in pseudo order and the asms
     that name it with "g" constraints emit nothing.
   - Horn-loop preheader [&tip][&dir][la mtx][burst giv]: the non-volatile
     `hole = asm(mtx)` sits in one block after the first `dir` movable, so
     loop.c hoists it (block-local user var, lifetime 18 clears the move
     threshold) right after &tip/&dir; sched1's birthing boost then lands
     mtx's `la` immediately before that zero-code consumer, ahead of the
     burst giv init.
   - Slot age/state bytes are givs off the anchor block, the arc-path select
     pins the table base to $2 and the path to $3 with an empty barrier in
     the long arm, and the product `k` is its own local. */
#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "sample_bank.h"

/* Held-base alias views (see config/symbol_addrs.txt). */

/* The particle spawner installed by the level overlay:
   (count, kind, world position, direction | NULL). */
#define SPAWN ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)

/* 0x80048d10 (2248 bytes) — per-tick attack-effects driver, two independent
   subsystems sharing one function.  (1) Flame breath: while the breath timer
   runs, spawn one particle per frame at a random offset in Spyro's body frame
   and pulse the flame trail's RGBA (the alpha follows a sine, and blinks off
   for part of every 24-frame cycle once the timer is under 0x78).  (2) Horn
   strike: arm this frame's trail slots through the ring permutation, rebuild
   the anchor frame (either the body-frame matrix of the current anim frame's
   packed offset, or a static offset from the world position), then walk each
   of the 8 slots along its arc path — probing actors and world geometry
   between the previous and current point, freezing the slot and spawning the
   hit burst (plus one kick sfx per frame) on contact, or spawning the
   end-of-arc burst when the slot runs off the end of its path.  param steps =
   frame delta.  Clears g_bSpyroHornCollisionActive once all 8 slots are idle.
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
  if (g_anSpyroFlameBreathTimerBlock[0] != 0) {
    flame[0] = (GetRandomU32() & 0x1ff) - 0x160;
    flame[1] = (GetRandomU32() & 0x7f) - 0x40;
    flame[2] = ((flame[0] >> 1) + (GetRandomU32() & 0x3f)) - 0x60;
    RotateVectorByMatrix(g_anSpyroBodyMtxBlock, flame, flame);
    AddVector(flame, flame, &g_anSpyroBodyMtxBlock[-13]);
    ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(1, 0x1e, flame,
                                                                0);
    g_abSpyroFxParticleRgba[0] = 0x90;
    g_abSpyroFxParticleRgba[1] = 0x20;
    g_abSpyroFxParticleRgba[2] = 0x10;
    if ((g_anSpyroFlameBreathTimerBlock[0] < 0x78) &&
        ((g_anSpyroFlameBreathTimerBlock[0] % 24) >= 0xe)) {
      g_abSpyroFxParticleRgba[3] = 0;
    } else {
      g_abSpyroFxParticleRgba[3] =
          (LookupSine(g_nSpyroFlameBreathTimer << 7) / 256) - 0x28;
    }
    g_anSpyroFlameBreathTimerBlock[0] -= g_nFrameStep;
    if (g_anSpyroFlameBreathTimerBlock[0] <= 0) {
      g_anSpyroFlameBreathTimerBlock[0] = 0;
      g_abSpyroFxParticleRgba[3] = 0;
    }
  }
  if (g_bSpyroHornCollisionActive != 0) {
    wp = g_anSpyroWorldPos;
    if ((g_nSpyroAirborneFrames == 0) &&
        (g_abSpyroPersistentEulerBlock[1] >= 0x81)) {
      g_nCameraScriptStickRequestY =
          (0x100 - g_abSpyroPersistentEulerBlock[1]) * 8;
    } else {
      g_nCameraScriptStickRequestY = 0;
    }
    n = 0;
    if (steps > 0) {
      seg = g_abSpyroHornStrikeSegmentBlock;
      map = g_abSpyroHornStrikeSlotMap;
      do {
        if (seg[0] < 8) {
          g_abSpyroHornStrikeSlotAge[(map + (seg[2] * 8))[seg[0]]] = steps - n;
          g_abSpyroHornStrikeSlotEndAge[(map + (seg[2] * 8))[seg[0]]] = 0x20;
          g_abSpyroHornStrikeSlotState[(map + (seg[2] * 8))[seg[0]]] = 0;
        }
        seg[0]++;
      } while ((++n) < steps);
    }
    if (((g_bSpyroHornStrikeSegmentIdx < 0x1e) &&
         (((*((int *)(&g_bSpyroAnimPrev))) & 0xffff0000) == 0x07070000)) &&
        (g_bSpyroAnimLayer1FramePrev == 4)) {
      g_abSpyroAnimLayer1SubstepBlock[0] = 0;
    }
    if (g_abSpyroHornStrikeAnchorModeBlock[0] != 0) {
      g_abSpyroHornStrikeAnchorEuler[0] = g_abSpyroPersistentEulerBlock[0];
      g_abSpyroHornStrikeAnchorEuler[1] = g_abSpyroPersistentEulerBlock[1];
      g_abSpyroHornStrikeAnchorEuler[2] = g_abSpyroPersistentEulerBlock[2];
      CopyWords(&g_abSpyroHornStrikeAnchorModeBlock[0xa],
                &g_abSpyroPersistentEulerBlock[0x1bc], 0x14);
      rec = (int *)((g_bSpyroFramePrev * 4) +
                    (*((int *)(((g_bSpyroAnimPrev * 4) +
                                ((int)g_apActorMeshTable)) +
                               0x38))));
      n = ((int *)((rec[9] & 0x1fffff) * 2))[4];
      anchor[0] = n >> 21;
      anchor[1] = (n << 11) >> 21;
      anchor[2] = (n << 22) >> 21;
      RotateVectorByMatrix((int *)(&g_abSpyroPersistentEulerBlock[0x28]),
                           anchor, anchor);
      AddVector((int *)(&g_abSpyroHornStrikeAnchorModeBlock[-0x9a]), anchor,
                (int *)(&g_abSpyroPersistentEulerBlock[-0xc]));
      if (g_bSpyroHornStrikeSegmentIdx < 8) {
        CopyVector(anchor, &g_anSpyroHornStrikeWindupAnchorOffsets
                               [(g_bSpyroHornStrikeSegmentIdx >> 1) * 3]);
      } else {
        CopyVector(anchor, g_anSpyroHornStrikeFinalAnchorOffset);
      }
      amtx = g_anSpyroHornStrikeAnchorMtxBlock;
      RotateVectorByMatrix(amtx, anchor, anchor);
      AddVector(&amtx[-0x29], &amtx[-0x29], anchor);
      SubtractVector(&amtx[0xa], &amtx[-0x29], wp);
    } else {
      AddVector((int *)(&g_abSpyroHornStrikeAnchorModeBlock[-0x9a]), wp,
                (int *)(&g_abSpyroHornStrikeAnchorModeBlock[0x32]));
    }
    i = 0;
    mtx = g_anSpyroHornStrikeAnchorMtxBlock;
    pt = &mtx[0xd];
    do {
      if (((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] !=
          0) {
        if (i < 4) {
          count = 0x18;
          limit = 0x20;
        } else {
          count = 0x20;
          limit = 0x28;
        }
        if (((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x1d])[i] !=
            0) {
          ((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] +=
              steps;
        }
        if (((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] >=
            limit) {
          ((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] = 0;
        } else if (((unsigned char
                         *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x1d])[i] != 2) {
          register short *tbl asm("$2");
          register short *path asm("$3");

          if (i < 4) {
            k = i * count;
            tbl = g_anSpyroHornStrikeArcPathsShort;
          } else {
            k = (i - 4) * count;
            __asm__ volatile("");
            tbl = g_anSpyroHornStrikeArcPathsLong;
          }
          path = (short *)(k * 8 + (int)tbl);
          if (((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] <
              (count - 1)) {
            SVectorToVector(
                tip,
                &path[(((unsigned char
                             *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i] +
                       1) *
                      4]);
            RotateVectorByMatrix(mtx, tip, tip);
            AddVector(tip, tip, &mtx[-0x29]);
            if (((unsigned char
                      *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x1d])[i] == 0) {
              CopyVector(dir, wp);
              ((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x1d])[i] =
                  1;
            } else {
              CopyVector(dir, &pt[i * 3]);
            }
            CopyVector(&pt[i * 3], tip);
            mask = 0x10000;
            if ((*((volatile int *)(&pt[-0xf]))) != 0) {
              mask = 0x90000;
            }
            if ((DispatchActorContactAtSphere(tip, 0xa0, 0, mask, 0, 0) != 0) ||
                (CastRayWorldAndActors(dir, tip) != 0)) {
              ((unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x1d])[i] =
                  2;
              g_abSpyroHornStrikeSlotEndAge[i] = ((
                  unsigned char *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i];
              dir[0] = -0x10;
              dir[1] = (GetRandomU32() & 0x1f) - 0x10;
              dir[2] = 0;
              RotateVectorByMatrix(&pt[-0xd], dir, dir);
              dir[2] = 0;
              ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                  1, 1, tip, dir);
              if ((*((volatile int *)(&pt[-0xf]))) == 0) {
                ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                    4, 0x4e, tip, 0);
              }
              __asm__("" : "=g"(hole) : "r"(mtx));
              dir[0] = 0x30;
              dir[1] = 0;
              dir[2] = 0;
              ApplyActiveGteRotation(dir, dir);
              ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                  ((*((volatile int *)(&pt[-0xf]))) * 5) + 5, 0x4f, tip, dir);
              __asm__ volatile("" : : "g"(hole));
              if (sfxDone == 0) {
                PlaySoundEffect(LEVEL_SFX(0x28), wp, 4, 0);
                sfxDone = 1;
              }
            }
          } else if ((count <
                      ((unsigned char
                            *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i]) &&
                     ((count + steps) >=
                      ((unsigned char
                            *)&g_anSpyroHornStrikeAnchorMtxBlock[-0x21])[i])) {
            SVectorToVector(tip, &path[(count - 1) * 4]);
            RotateVectorByMatrix(mtx, tip, tip);
            AddVector(tip, tip, &mtx[-0x29]);
            ZeroVector(dir);
            if (g_nSpyroHornStrikeVariant != 0) {
              ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                  1, 1, tip, dir);
            } else {
              ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                  1, 0, tip, dir);
            }
            if (g_nSpyroHornStrikeVariant == 0) {
              ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                  4, 0x4e, tip, 0);
            }
            ApplyActiveGteRotation(&g_anSpyroHornStrikeEndBurstDirs[i * 3],
                                   dir);
            ((void (*)(int, int, int *, int *))g_pfnLevelEmitParticles)(
                (g_nSpyroHornStrikeVariant * 5) + 5, 0x4f, tip, dir);
          }
        }
      } else {
        empty++;
      }
    } while ((++i) < 8);
    if (empty == 8) {
      g_bSpyroHornCollisionActive = 0;
    }
  }
}
