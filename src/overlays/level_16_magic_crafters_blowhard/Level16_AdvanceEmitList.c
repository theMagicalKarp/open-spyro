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

      CopyVector(p, PLUME_ANCHOR);
      z = p[2] + PLUME_TOP * 0x40;
      amp = AGE * 0x40;
      ang = amp;
      if (PLUME_MAX_RADIUS < amp) {
        amp = PLUME_MAX_RADIUS;
      }
      p[0] += (amp * LookupCosine(ang)) >> 12;
      p[1] += (amp * LookupSine(ang)) >> 12;
      p[2] = POS[2] * 4 + 0x40;
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
        SPIN += SPIN_RATE;
        if (AGE >= 0xD) {
          SIZE = (0x18 - AGE) * 4;
        } else {
          SIZE = AGE * 4;
        }
        POS_U[0] += VEL_U[0];
        POS_U[1] += VEL_U[1];
        POS_U[2] += VEL_U[2];
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
        CopyVector(dir, &g_pActorListBase[TRACK_ACTOR].posX);
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
              FX = EMIT_FX_LINE;
              CLASS = EMIT_CLASS_DOT;
              c = COL_R * 2;
              COL_R = c;
              COL_G *= 2;
              COL_B *= 2;
            }
          } else if (len < 0x2800) {
            CLASS = 3;
            DEPTH_BIAS = 4;
            FX = EMIT_FX_QUAD;
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

    case EMIT_SPYRO_HURT_SPARKLE:
      /* A hurt sparkle: circles Spyro (0x40 a tick) while his post-hit
       * invulnerability runs; freed early if he is hidden or a hazard kills him. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < SPARX_GONE) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInvulnFrames[0] <= 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(w, SPYRO_POS_VIA_INVULN);
        w[0] += LookupCosine(ORBIT_ANGLE) >> 4;
        w[1] += LookupSine(ORBIT_ANGLE) >> 4;
        w[2] += 0x100;
        VectorToSVectorShr2(POS, w);
        ORBIT_ANGLE = ORBIT_ANGLE_U + 0x40;
        SIZE = abs(LookupCosine((ORBIT_ANGLE - g_nCameraEulerYaw) & 0xFFF)) / 170;
      }
      break;

    case EMIT_BURST_SPARK:
      AddSVector(POS, POS, BURST_VEL);
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
      if (COL_RGB == 0 || VISIBLE == 0) {
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
      POS_U[0] = SPIRAL_CENTRE_U[0] + ((g_anCosineLut[SWIRL_ANGLE] * SWIRL_RADIUS) >> 12);
      s = g_anSineLut[SWIRL_ANGLE] * SWIRL_RADIUS;
      POS_U[2] += *(const int *)&g_nFrameStep * 6;
      SPIN += 4;
      POS_U[1] = SPIRAL_CENTRE_U[1] + (s >> 12);
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
      RSWIRL_ANGLE += g_nFrameStep * 8;
      heat = 0x3F;
      if (g_nDragonRescueTimer < 0x40) {
        heat = g_nDragonRescueTimer;
      }
      RSWIRL_LIFT = (RSWIRL_BAND * (0x48 - heat)) >> 3;
      RSWIRL_RADIUS = (((0x20 - RSWIRL_BAND) * heat) >> 4) + 8;
      POS_U[0] = RSWIRL_CENTRE_U[0] + ((g_anCosineLut[RSWIRL_ANGLE] * RSWIRL_RADIUS) >> 12);
      POS_U[1] = RSWIRL_CENTRE_U[1] + ((g_anSineLut[RSWIRL_ANGLE] * RSWIRL_RADIUS) >> 12);
      POS_U[2] = RSWIRL_CENTRE_U[2] + 0x20 + RSWIRL_LIFT_U;
      break;
    }

    case EMIT_DUST_PUFF: {
      AddSVector(POS, POS, VEL);
      if (CLIMB_TOGGLE != 0) {
        VEL_U[2]++;
      }
      c = COL_R - step;
      CLIMB_TOGGLE = 1 - CLIMB_TOGGLE_U;
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
      d[0] = (HEAD[0] - POS[0]) >> 1;
      d[1] = (HEAD[1] - POS[1]) >> 1;
      d[2] = (HEAD[2] - POS[2]) >> 1;
      POS_U[0] += d[0];
      POS_U[1] += d[1];
      POS_U[2] += d[2];
      r = GetRandomU32();
      j = d[0] - 1;
      j += r & 2;
      d[0] = j;
      r = GetRandomU32();
      j = d[1] - 1;
      j += r & 2;
      d[1] = j;
      HEAD_U[0] += d[0];
      HEAD_U[1] += d[1];
      HEAD_U[2] += d[2];
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
          POS_U[0] += DRIFT_U[0];
          POS_U[1] += DRIFT_U[1];
          POS_U[2] += DRIFT_U[2];
          if (AGE & 2) {
            /* the `- 1` has to stay int-wide: folded into the unsigned short
             * it becomes `+ 0xFFFF`, which no longer fits an addiu and gets
             * hoisted out of the loop as a constant in a callee-saved reg. */
            r = GetRandomU32();
            j = DRIFT_U[0] - 1;
            j += r & 2;
            DRIFT_U[0] = j;
            r = GetRandomU32();
            j = DRIFT_U[1] - 1;
            j += r & 2;
            DRIFT_U[1] = j;
            r = GetRandomU32();
            j = DRIFT_U[2] - 1;
            j += r & 2;
            DRIFT_U[2] = j;
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

      d[0] = (HEAD[0] - POS[0]) >> 1;
      d[1] = (HEAD[1] - POS[1]) >> 1;
      d[2] = (HEAD[2] - POS[2]) >> 1;
      POS_U[0] += d[0];
      POS_U[1] += d[1];
      POS_U[2] += d[2];
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
      HEAD_U[0] += d[0];
      HEAD_U[1] += d[1];
      HEAD_U[2] += d[2];
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
        d[0] = HEAD[0] - POS[0];
        d[1] = HEAD[1] - POS[1];
        d[2] = HEAD[2] - POS[2];
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
        HEAD[0] = HEAD_U[0] + d[0];
        HEAD[1] = HEAD_U[1] + d[1];
        HEAD[2] = HEAD_U[2] + d[2];
        POS_U[0] = HEAD_U[0] - d[0];
        POS_U[1] = HEAD_U[1] - d[1];
        POS_U[2] = HEAD_U[2] - d[2];
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
        d[0] = (POS[0] - HEAD[0]) >> 1;
        d[1] = (POS[1] - HEAD[1]) >> 1;
        d[2] = (POS[2] - HEAD[2]) >> 1;
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
        POS_U[0] += d[0];
        POS_U[1] += d[1];
        POS_U[2] += d[2];
        HEAD[0] = POS_U[0] - d[0] * 2;
        HEAD[1] = POS_U[1] - d[1] * 2;
        HEAD[2] = POS_U[2] - d[2] * 2;
      }
      break;
    }

    case EMIT_RISING_SPIRAL_SMEAR: {
      int s;

      SWIRL_ANGLE += g_nFrameStep * 4;
      SWIRL_RADIUS += g_nFrameStep;
      POS_U[0] = SMEAR_CENTRE_U[0] + ((g_anCosineLut[SWIRL_ANGLE] * SWIRL_RADIUS) >> 12);
      s = g_anSineLut[SWIRL_ANGLE] * SWIRL_RADIUS;
      POS_U[2] += g_nFrameStep * 6;
      POS_U[1] = SMEAR_CENTRE_U[1] + (s >> 12);
      if (SWIRL_RADIUS >= 0xC1) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_DRIFT_STREAK: {
      short d[3];

      d[0] = (HEAD[0] - POS[0]) >> 1;
      d[1] = (HEAD[1] - POS[1]) >> 1;
      d[2] = (HEAD[2] - POS[2]) >> 1;
      POS_U[0] += d[0];
      POS_U[1] += d[1];
      POS_U[2] += d[2];
      d[1]++;
      HEAD_U[0] += d[0];
      HEAD_U[1] += d[1];
      HEAD_U[2] += d[2];
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
        POS_U[0] += DRIFT_U[0];
        POS_U[1] += DRIFT_U[1];
        POS_U[2] += DRIFT_U[2];
        r = GetRandomU32();
        j = DRIFT_U[0] - 1;
        j += r & 2;
        DRIFT_U[0] = j;
        r = GetRandomU32();
        j = DRIFT_U[1] - 1;
        j += r & 2;
        DRIFT_U[1] = j;
        r = GetRandomU32();
        k = DRIFT_U[2] - 1;
        k += r & 2;
        DRIFT_U[2] = k;
        if (DRIFT[2] >= 3) {
          DRIFT_U[2] = 2;
        }
        if (DRIFT[2] < -4) {
          DRIFT[2] = -4;
        }
        COL_R -= 2;
        COL_G -= 2;
        COL_B -= 1;
      }
      break;
    }

    case EMIT_FLAME_STREAK: {
      short d[3];

      d[0] = (HEAD[0] - POS[0]) >> 1;
      d[1] = (HEAD[1] - POS[1]) >> 1;
      d[2] = (HEAD[2] - POS[2]) >> 1;
      POS_U[0] += d[0];
      POS_U[1] += d[1];
      POS_U[2] += d[2];
      HEAD_U[0] += d[0];
      HEAD_U[1] += d[1];
      HEAD_U[2] += d[2];
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

      d[0] = HEAD_U[0] - POS_U[0];
      d[1] = HEAD_U[1] - POS_U[1];
      d[2] = HEAD_U[2] - POS_U[2];
      POS_U[0] += d[0];
      POS_U[1] += d[1];
      POS_U[2] += d[2];
      d[2]--;
      HEAD_U[0] += d[0];
      HEAD_U[1] += d[1];
      HEAD_U[2] += d[2];
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
