/* Level16_AdvanceEmitList (0x80084830, level_16_magic_crafters_blowhard
 * overlay, 0x1710 bytes).
 *
 * The emit-list ADVANCE pass -- the 1476-insn Blowhard variant, 25 classes.
 * Walks the 0x20-byte record stream at g_pEmitListBase and ages one record per
 * iteration: integrates its position, fades its colour, ticks its life
 * counter and, when the record has expired (or its visible byte has been
 * cleared (culled)), hands it back with FreeEmitListRecord().  The walk stops at the
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
#include "spyro_state.h"
#include "emit_record.h"

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

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  step4 = step * 4;
  dir = v;

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_VEL_DIM:
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RISE:
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_ACTOR_PLUME: {
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

    case EMIT_DRIFT_SPARKLE:
      /* The rune orbit: it rises and falls on a triangular ramp keyed to the
       * age byte and drifts by its own per-axis velocity words. */
      if (AGED_OR_CULLED(0x19)) {
        FreeEmitListRecord(rec);
      } else {
        SPIN += rec[0x1E];
        if (AGE >= 0xD) {
          SIZE = (0x18 - AGE) * 4;
        } else {
          SIZE = AGE * 4;
        }
        HU(0x4) += HU(0x18);
        HU(0x6) += HU(0x1A);
        HU(0x8) += HU(0x1C);
        AGE++;
      }
      break;

    case EMIT_TRACKING_SPARK: {
      int len;
      int *hold;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (pool slot `owner`), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      SVectorToVectorShl2(p2, POS);
      if (VISIBLE == 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(dir, &g_pActorListBase[rec[0x1E]].posX);
        EBB_BREAK();
        v[0] += LookupCosine(AGE << 7) >> 1;
        v[1] += LookupSine(AGE << 7) >> 1;
        v[2] += LookupCosine((AGE << 7) - 0x100) >> 2;
        hold = p2;
        SubtractVector(dir, dir, hold);
        len = VectorLength(dir, VEC_LEN_XYZ);
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
          if (CLASS == EMIT_CLASS_ALT_LIT_QUAD) {
            if (len >= 0x3001) {
              SIZE = 4;
              rec[0xF] = 0x40;
              CLASS = 0;
              c = COL_R * 2;
              COL_R = c;
              COL_G *= 2;
              COL_B *= 2;
            }
          } else if (len < 0x2800) {
            CLASS = 3;
            DEPTH_BIAS = 4;
            rec[0xF] = 0x2C;
            TEX_INDEX = 0;
            c = COL_R >> 1;
            COL_R = c;
            COL_G >>= 1;
            COL_B >>= 1;
            SIZE = (GetRandomU32() & 0xF) + 0x18;
            SPIN = (GetRandomU32() & 0x7) + 0xF;
          }
        }
      }
      break;
    }

    case EMIT_SUPERCHARGE_TRAIL:
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPYRO_ORBIT_SPARKLE:
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < SPARX_GONE) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(w, SPYRO_POS_VIA_LOCKOUT);
        w[0] += LookupCosine(HS(0x18)) >> 4;
        w[1] += LookupSine(HS(0x18)) >> 4;
        w[2] += 0x100;
        VectorToSVectorShr2(POS, w);
        HS(0x18) = HU(0x18) + 0x40;
        SIZE = abs(LookupCosine((HS(0x18) - g_nCameraEulerYaw) & 0xFFF)) / 170;
      }
      break;

    case EMIT_BURST_SPARK:
      AddSVector(POS, POS, &HS(0x12));
      FADE_COL(8);
      if (COL_R == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_ACTOR_SPARK:
      SPIN += 8;
      SIZE += 2;
      if (COL_R != 0) {
        COL_R -= 8;
      }
      if (COL_G != 0) {
        COL_G -= 8;
      }
      if (COL_B != 0) {
        COL_B -= 8;
      }
      if ((W(0xC) & 0xFFFFFF) == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_FADING_SPARK:
      /* Gnasty's cauldron drip: a slow ember that dies when its red
       * channel bottoms out. */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      FADE_COL(4);
      if (COL_R == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RGB:
      /* The same ember with a late red cut-off at age 9. */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      if (AGE >= 9) {
        COL_R -= 0x20;
      }
      COL_G -= 8;
      COL_B -= 4;
      AGE++;
      if (AGED_OR_CULLED(0xC)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_RISING_SPIRAL: {
      int s;

      SWIRL_ANGLE += g_nFrameStep * 4;
      SWIRL_RADIUS += g_nFrameStep;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[SWIRL_ANGLE] * SWIRL_RADIUS) >> 12);
      s = g_anSineLut[SWIRL_ANGLE] * SWIRL_RADIUS;
      HU(0x8) += *(const int *)&g_nFrameStep * 6;
      SPIN += 4;
      HU(0x6) = HU(0x14) + (s >> 12);
      if (SWIRL_RADIUS >= 0xC1) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_RESCUE_SWIRL: {
      int heat;

      if (COL_R < 0xF0) {
        COL_R += 4;
      }
      if (COL_G < 0xF0) {
        COL_G += 4;
      }
      if (COL_B < 0xF0) {
        COL_B += 4;
      }
      JET_ANGLE += g_nFrameStep * 8;
      heat = 0x3F;
      if (g_nDragonRescueTimer < 0x40) {
        heat = g_nDragonRescueTimer;
      }
      HS(0x18) = (JET_SEGMENT * (0x48 - heat)) >> 3;
      HS(0x1A) = (((0x20 - JET_SEGMENT) * heat) >> 4) + 8;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[JET_ANGLE] * HS(0x1A)) >> 12);
      HU(0x6) = HU(0x14) + ((g_anSineLut[JET_ANGLE] * HS(0x1A)) >> 12);
      HU(0x8) = HU(0x16) + 0x20 + HU(0x18);
      break;
    }

    case EMIT_DUST_PUFF: {
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      c = COL_R - step;
      HS(0x1E) = 1 - HU(0x1E);
      SIZE += step * 2;
      if (c < 0) {
        c = 0;
      }
      COL_R = c;
      c = COL_G - step;
      if (c < 0) {
        c = 0;
      }
      COL_G = c;
      c = COL_B - step;
      if (c < 0) {
        c = 0;
      }
      COL_B = c;
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_ACTOR_WHITE_STREAK: {
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
      if (AGED_OR_CULLED(0x8)) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_TWINKLE: {
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
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_STREAK_BURST: {
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
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      } else {
        COL0_R -= 3;
        if (COL0_R < 0x80) {
          COL0_R = 0x80;
        }
        COL0_G -= 4;
        if (COL0_G < 0x60) {
          COL0_G = 0x60;
        }
        COL0_B = 0;
        COL1_R -= 3;
        if (COL1_R < 0xC0) {
          COL1_R = 0xC0;
        }
        COL1_G -= 4;
        if (COL1_G < 0x80) {
          COL1_G = 0x80;
        }
        COL1_B -= 6;
        if (COL1_B < 0x20) {
          COL1_B = 0x20;
        }
      }
      break;
    }

    case EMIT_STREAK_BURST_WHITE: {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x20)) {
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
        COL1_R -= 2;
        COL1_G -= 2;
        COL1_B -= 4;
        COL0_R -= 3;
        COL0_G -= 3;
        COL0_B -= 3;
      }
      break;
    }

    case EMIT_CRAWLING_STREAK: {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x30)) {
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

    case EMIT_RISING_SPIRAL_SMEAR: {
      int s;

      SWIRL_ANGLE += g_nFrameStep * 4;
      SWIRL_RADIUS += g_nFrameStep;
      HU(0x4) = HU(0x10) + ((g_anCosineLut[SWIRL_ANGLE] * SWIRL_RADIUS) >> 12);
      s = g_anSineLut[SWIRL_ANGLE] * SWIRL_RADIUS;
      HU(0x8) += g_nFrameStep * 6;
      HU(0x6) = HU(0x12) + (s >> 12);
      if (SWIRL_RADIUS >= 0xC1) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_DRIFT_STREAK: {
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

    case EMIT_EMBER: {
      int j;
      int k;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x28)) {
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
        COL_R -= 2;
        COL_G -= 2;
        COL_B -= 1;
      }
      break;
    }

    case EMIT_FLAME_STREAK: {
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
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      } else {
        COL0_R -= 3;
        COL0_G -= 4;
        COL1_R -= 3;
        COL1_G -= 4;
        if (COL1_B >= 6) {
          COL1_B -= 6;
        }
      }
      break;
    }

    case EMIT_FALLING_STREAK_BURST: {
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
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    default:
      break;
    }

    rec += 0x20;
  } while (CLASS != EMIT_CLASS_END);
}
