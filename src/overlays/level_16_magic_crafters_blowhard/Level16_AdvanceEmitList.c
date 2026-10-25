/* Level16_AdvanceEmitList (0x80084830, level_16_magic_crafters_blowhard
 * overlay, 0x1710 bytes).
 *
 * The emit-list ADVANCE pass -- the 1476-insn Blowhard variant, 25 classes.
 * Walks the 0x20-byte record stream at g_pEmitListBase and ages one record per
 * iteration: integrates its position, fades its colour, ticks its life
 * counter and, when the record has expired (or its alive byte has been
 * cleared), hands it back with FreeEmitListRecord().  The walk stops at the
 * terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Head and arms 0x7/0x8/0x41 come from Level12_AdvanceEmitList, the ground
 * classes from level_2, 0x10/0x11 from level_33, the rest are shared.
 *
 * Three per-level decisions this variant needed:
 *  - FOUR stack vectors, not three.  Arm 0x6 gets its own (frame 0x10) and
 *    arm 0x8's position vector is a separate one (0x20); `dir` still names
 *    &v (0x30) and arm 0xA has 0x40.  Declaration order IS frame order, so
 *    the split has to be declared in that sequence or every sp-relative
 *    offset in the function shifts by 16.
 *  - An A200 cse barrier (`do { } while (0);`) between arm 0x8's first
 *    SVectorToVectorShl2(p2, POS) and the direction-vector group.  Without it the
 *    whole arm shares ONE address pseudo: gcc either hoists it to the loop
 *    preheader and spills it or holds it in s0 from the first use, where the
 *    original rematerialises `addiu a0,sp,0x20` there and only takes s0 for
 *    the middle four uses.
 *  - Arm 0x8's `hold` carrier is MULTI-SET on purpose (A214).  The middle
 *    group reads p2 through it and the last statement reassigns it to
 *    g_anCameraPos -- the very argument the following call needs in a2 -- so
 *    the reassignment is free, and a multi-set pointer has no constant
 *    equivalence for cse to fold, which is what makes the last use of p2
 *    come out as a fresh `addiu a1,sp,0x20` instead of `move a1,s0`.
 *    That single instruction was the whole residue.
 *
 * Only `rec` is a real induction variable; every field access is an ADDRESS
 * giv off it, which is how the whole record ends up addressed off `rec + 1`
 * with offsets 0x00..0x1D.
 *
 * Verified byte-identical inside the relinked
 * level_16_magic_crafters_blowhard.ovl.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_type.h"
#include "match_asm.h"


void Level16_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int p[3];
  int p2[3];
  int v[3];
  int w[3];
  int c;
  int d;
  int *dir;
  int step4;

  if (CLASS == 0xFF) {
    return;
  }

  step4 = step * 4;
  dir = v;

  do {
    switch (rec[0]) {
    case 0x0:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x1:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step4;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      AGE += step;
      if (AGE >= 0x18 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x2:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step4;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x6: {
      int ang;
      int amp;
      int z;

      CopyVector(p, (int *)W(0x18));
      z = p[2] + HS(0x1E) * 0x40;
      amp = AGE * 0x40;
      ang = amp;
      if (HS(0x1C) < amp) {
        amp = HS(0x1C);
      }
      p[0] += (amp * LookupCosine(ang)) >> 12;
      p[1] += (amp * LookupSine(ang)) >> 12;
      p[2] = HS(0x8) * 4 + 0x40;
      if (z < p[2] || AGE >= 0xC9) {
        FreeEmitListRecord(rec);
      } else {
        VectorToSVectorShr2(POS, p);
        AGE++;
      }
      break;
    }

    case 0x7:
      /* The rune orbit: it rises and falls on a triangular ramp keyed to the
       * age byte and drifts by its own per-axis velocity words. */
      if (AGE >= 0x19 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        rec[0xB] += rec[0x1E];
        if (AGE >= 0xD) {
          rec[0xA] = (0x18 - AGE) * 4;
        } else {
          rec[0xA] = AGE * 4;
        }
        HU(0x4) += HU(0x18);
        HU(0x6) += HU(0x1A);
        HU(0x8) += HU(0x1C);
        AGE++;
      }
      break;

    case 0x8: {
      int len;
      int *hold;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (g_pActorListBase + owner * 0x58), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      SVectorToVectorShl2(p2, POS);
      if (ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(dir, (int *)(g_pActorListBase + rec[0x1E] * ACTOR_SIZE + ACTOR_OFFSET(posX)));
        EBB_BREAK();
        v[0] += LookupCosine(AGE << 7) >> 1;
        v[1] += LookupSine(AGE << 7) >> 1;
        v[2] += LookupCosine((AGE << 7) - 0x100) >> 2;
        hold = p2;
        SubtractVector(dir, dir, hold);
        len = VectorLength(dir, 1);
        if (len < 0x401 || AGE >= 0x41) {
          FreeEmitListRecord(rec);
        } else {
          ScaleVector3ByRatio(dir, len, 0x80);
          AddVector(hold, hold, dir);
          VectorToSVectorShr2(POS, hold);
          AGE++;
          hold = g_anCameraPos;
          SubtractVector(v, p2, hold);
          c = abs(v[0]) + abs(v[1]);
          len = c + abs(v[2]);
          if (CLASS == 3) {
            if (len >= 0x3001) {
              rec[0xA] = 4;
              rec[0xF] = 0x40;
              CLASS = 0;
              c = rec[0xC] * 2;
              rec[0xC] = c;
              rec[0xD] *= 2;
              rec[0xE] *= 2;
            }
          } else if (len < 0x2800) {
            CLASS = 3;
            rec[0x11] = 4;
            rec[0xF] = 0x2C;
            rec[0x10] = 0;
            c = rec[0xC] >> 1;
            rec[0xC] = c;
            rec[0xD] >>= 1;
            rec[0xE] >>= 1;
            rec[0xA] = (GetRandomU32() & 0xF) + 0x18;
            rec[0xB] = (GetRandomU32() & 0x7) + 0xF;
          }
        }
      }
      break;
    }

    case 0x9:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0xA:
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == 0x1D && g_nSparxHealth < 0) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(w, SPYRO_POS_VIA_LOCKOUT);
        w[0] += LookupCosine(HS(0x18)) >> 4;
        w[1] += LookupSine(HS(0x18)) >> 4;
        w[2] += 0x100;
        VectorToSVectorShr2(POS, w);
        HS(0x18) = HU(0x18) + 0x40;
        rec[0xA] = abs(LookupCosine((HS(0x18) - g_nCameraEulerYaw) & 0xFFF)) / 170;
      }
      break;

    case 0xB:
      AddSVector(POS, POS, &HS(0x12));
      rec[0xC] -= 8;
      rec[0xD] -= 8;
      rec[0xE] -= 8;
      if (rec[0xC] == 0 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0xC:
      rec[0xB] += 8;
      rec[0xA] += 2;
      if (rec[0xC] != 0) {
        rec[0xC] -= 8;
      }
      if (rec[0xD] != 0) {
        rec[0xD] -= 8;
      }
      if (rec[0xE] != 0) {
        rec[0xE] -= 8;
      }
      if ((W(0xC) & 0xFFFFFF) == 0 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x10:
      /* Gnasty's cauldron drip: a slow ember that dies when its red
       * channel bottoms out. */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0xC] -= 4;
      rec[0xD] -= 4;
      rec[0xE] -= 4;
      if (rec[0xC] == 0 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x11:
      /* The same ember with a late red cut-off at age 9. */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      if (AGE >= 9) {
        rec[0xC] -= 0x20;
      }
      rec[0xD] -= 8;
      rec[0xE] -= 4;
      AGE++;
      if (AGE >= 0xC || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x15: {
      int s;

      rec[0x18] += g_nFrameStep * 4;
      rec[0x19] += g_nFrameStep;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[rec[0x18]] * rec[0x19]) >> 12);
      s = g_anSineLut[rec[0x18]] * rec[0x19];
      HU(0x8) += *(const int *)&g_nFrameStep * 6;
      rec[0xB] += 4;
      HU(0x6) = HU(0x14) + (s >> 12);
      if (rec[0x19] >= 0xC1) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x18: {
      int heat;

      if (rec[0xC] < 0xF0) {
        rec[0xC] += 4;
      }
      if (rec[0xD] < 0xF0) {
        rec[0xD] += 4;
      }
      if (rec[0xE] < 0xF0) {
        rec[0xE] += 4;
      }
      rec[0x1C] += g_nFrameStep * 8;
      heat = 0x3F;
      if (g_nGemPickupTimer < 0x40) {
        heat = g_nGemPickupTimer;
      }
      HS(0x18) = (rec[0x1D] * (0x48 - heat)) >> 3;
      HS(0x1A) = (((0x20 - rec[0x1D]) * heat) >> 4) + 8;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x6) = HU(0x14) + ((g_anSineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x8) = HU(0x16) + 0x20 + HU(0x18);
      break;
    }

    case 0x21: {
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      c = rec[0xC] - step;
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      if (c < 0) {
        c = 0;
      }
      rec[0xC] = c;
      c = rec[0xD] - step;
      if (c < 0) {
        c = 0;
      }
      rec[0xD] = c;
      c = rec[0xE] - step;
      if (c < 0) {
        c = 0;
      }
      rec[0xE] = c;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x41: {
      unsigned short d[3];
      int j;
      int r;

      /* A short-lived midpoint relaxation -- the 0x46 shape without the
       * third jitter and without the colour ramp. */
      d[0] = (HS(0xA) - HS(0x4)) >> 1;
      d[1] = (HS(0xC) - HS(0x6)) >> 1;
      d[2] = (HS(0xE) - HS(0x8)) >> 1;
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      r = GetRandomU32();
      j = d[0] - 1;
      j += r & 2;
      d[0] = j;
      r = GetRandomU32();
      j = d[1] - 1;
      j += r & 2;
      d[1] = j;
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      AGE++;
      if (AGE >= 0x8 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x42: {
      int i;
      int j;
      int r;

      for (i = 0; i < step; i++) {
        if (AGE & 1) {
          HU(0x4) += HU(0x10);
          HU(0x6) += HU(0x12);
          HU(0x8) += HU(0x14);
          if (AGE & 2) {
            /* the `- 1` has to stay int-wide: folded into the unsigned short
             * it becomes `+ 0xFFFF`, which no longer fits an addiu and gets
             * hoisted out of the loop as a constant in a callee-saved reg. */
            r = GetRandomU32();
            j = HU(0x10) - 1;
            j += r & 2;
            HU(0x10) = j;
            r = GetRandomU32();
            j = HU(0x12) - 1;
            j += r & 2;
            HU(0x12) = j;
            r = GetRandomU32();
            j = HU(0x14) - 1;
            j += r & 2;
            HU(0x14) = j;
          }
        }
        AGE++;
      }
      if (AGE >= 0x18 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x46: {
      unsigned short d[3];
      int j;
      int r;

      d[0] = (HS(0xA) - HS(0x4)) >> 1;
      d[1] = (HS(0xC) - HS(0x6)) >> 1;
      d[2] = (HS(0xE) - HS(0x8)) >> 1;
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      r = GetRandomU32();
      j = d[0] - 1;
      j += r & 2;
      d[0] = j;
      r = GetRandomU32();
      j = d[1] - 1;
      j += r & 2;
      d[1] = j;
      if (AGE & 1) {
        d[2]--;
      }
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      AGE++;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        rec[0x10] -= 3;
        if (rec[0x10] < 0x80) {
          rec[0x10] = 0x80;
        }
        rec[0x11] -= 4;
        if (rec[0x11] < 0x60) {
          rec[0x11] = 0x60;
        }
        rec[0x12] = 0;
        rec[0x14] -= 3;
        if (rec[0x14] < 0xC0) {
          rec[0x14] = 0xC0;
        }
        rec[0x15] -= 4;
        if (rec[0x15] < 0x80) {
          rec[0x15] = 0x80;
        }
        rec[0x16] -= 6;
        if (rec[0x16] < 0x20) {
          rec[0x16] = 0x20;
        }
      }
      break;
    }

    case 0x47: {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        d[0] = HS(0xA) - HS(0x4);
        d[1] = HS(0xC) - HS(0x6);
        d[2] = HS(0xE) - HS(0x8);
        r = GetRandomU32();
        j = d[0] - 1;
        j += r & 2;
        d[0] = j;
        r = GetRandomU32();
        j = d[1] - 1;
        j += r & 2;
        d[1] = j;
        r = GetRandomU32();
        j = d[2] - 1;
        j += r & 2;
        d[2] = j;
        HS(0xA) = HU(0xA) + d[0];
        HS(0xC) = HU(0xC) + d[1];
        HS(0xE) = HU(0xE) + d[2];
        HU(0x4) = HU(0xA) - d[0];
        HU(0x6) = HU(0xC) - d[1];
        HU(0x8) = HU(0xE) - d[2];
        rec[0x14] -= 2;
        rec[0x15] -= 2;
        rec[0x16] -= 4;
        rec[0x10] -= 3;
        rec[0x11] -= 3;
        rec[0x12] -= 3;
      }
      break;
    }

    case 0x48: {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGE >= 0x30 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        d[0] = (HS(0x4) - HS(0xA)) >> 1;
        d[1] = (HS(0x6) - HS(0xC)) >> 1;
        d[2] = (HS(0x8) - HS(0xE)) >> 1;
        r = GetRandomU32();
        j = d[0] - 2;
        j += r & 4;
        d[0] = j;
        r = GetRandomU32();
        j = d[1] - 2;
        j += r & 4;
        d[1] = j;
        r = GetRandomU32();
        j = d[2] - 1;
        j += r & 2;
        d[2] = j;
        HU(0x4) += d[0];
        HU(0x6) += d[1];
        HU(0x8) += d[2];
        HS(0xA) = HU(0x4) - d[0] * 2;
        HS(0xC) = HU(0x6) - d[1] * 2;
        HS(0xE) = HU(0x8) - d[2] * 2;
      }
      break;
    }

    case 0x4C: {
      int s;

      rec[0x18] += g_nFrameStep * 4;
      rec[0x19] += g_nFrameStep;
      HU(0x4) = HU(0x10) + ((g_anCosineLut[rec[0x18]] * rec[0x19]) >> 12);
      s = g_anSineLut[rec[0x18]] * rec[0x19];
      HU(0x8) += g_nFrameStep * 6;
      HU(0x6) = HU(0x12) + (s >> 12);
      if (rec[0x19] >= 0xC1) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x4D: {
      short d[3];

      d[0] = (HS(0xA) - HS(0x4)) >> 1;
      d[1] = (HS(0xC) - HS(0x6)) >> 1;
      d[2] = (HS(0xE) - HS(0x8)) >> 1;
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      d[1]++;
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      AGE++;
      if (AGE >= 0x20) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x4E: {
      int j;
      int k;
      int r;

      AGE++;
      if (AGE >= 0x28 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        HU(0x4) += HU(0x10);
        HU(0x6) += HU(0x12);
        HU(0x8) += HU(0x14);
        r = GetRandomU32();
        j = HU(0x10) - 1;
        j += r & 2;
        HU(0x10) = j;
        r = GetRandomU32();
        j = HU(0x12) - 1;
        j += r & 2;
        HU(0x12) = j;
        r = GetRandomU32();
        k = HU(0x14) - 1;
        k += r & 2;
        HU(0x14) = k;
        if (HS(0x14) >= 3) {
          HU(0x14) = 2;
        }
        if (HS(0x14) < -4) {
          HS(0x14) = -4;
        }
        rec[0xC] -= 2;
        rec[0xD] -= 2;
        rec[0xE] -= 1;
      }
      break;
    }

    case 0x4F: {
      short d[3];

      d[0] = (HS(0xA) - HS(0x4)) >> 1;
      d[1] = (HS(0xC) - HS(0x6)) >> 1;
      d[2] = (HS(0xE) - HS(0x8)) >> 1;
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      AGE++;
      if (AGE >= 0x18 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        rec[0x10] -= 3;
        rec[0x11] -= 4;
        rec[0x14] -= 3;
        rec[0x15] -= 4;
        if (rec[0x16] >= 6) {
          rec[0x16] -= 6;
        }
      }
      break;
    }

    case 0x50: {
      short d[3];

      d[0] = HU(0xA) - HU(0x4);
      d[1] = HU(0xC) - HU(0x6);
      d[2] = HU(0xE) - HU(0x8);
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      d[2]--;
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      AGE++;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    default:
      break;
    }

    rec += 0x20;
  } while (CLASS != 0xFF);
}
