/* Level12_AdvanceEmitList (0x8008BE98, level_12_magic_crafters_home overlay,
 * 0x1438 bytes).
 *
 * The emit-list ADVANCE pass -- the Magic-Crafters-home 1294-insn variant,
 * the largest F16-free copy in the family.  Walks the 0x20-byte record
 * stream at g_pEmitListBase and ages one record per iteration: integrates its
 * position, fades its colour, ticks its life counter and, when the record
 * has expired (or its visible byte has been cleared (culled)), hands it back with
 * FreeEmitListRecord().  The walk stops at the terminator record, whose class
 * byte at +0x01 is 0xFF.
 *
 * Only `rec` is a real induction variable.  `pos` and `vel` are separate
 * pointers because the arms pass them to AddSVector() as values, so
 * loop.c strength-reduces each into its own register; every field access is
 * an ADDRESS giv off `rec`, and loop.c picks the last one it scans -- the
 * `rec[1]` terminator test -- as the shared base, which is how the whole
 * record ends up addressed off `rec + 1` with offsets 0x00..0x1D.
 *
 * Field map, in record-relative offsets:
 *   0x00  effect id                0x01  class / terminator
 *   0x02  age                      0x03  visible
 *   0x04/0x06/0x08  tail point     0x0A/0x0C/0x0E  head point (also the
 *                                  r/g/b triple for the fading classes)
 *   0x10..0x16  per-record velocity / colour bytes
 *   0x18  spin angle                0x1C  spark vel z   0x1E  phase toggle
 *
 * Eighteen arms transfer verbatim from the matched level_0 hub copy (which
 * in turn carries the level_6 / level_2 donors).  Three are new:
 *   0x7   the rune orbit -- a triangular height ramp off the age byte;
 *   0x8   the wizard's homing bolt, the only arm in the family that reads
 *         the owner ACTOR (pool slot `owner`) and spirals around
 *         the line to it, switching to its burst state (class byte 3) once
 *         it closes inside 0x2800;
 *   0x41  a short-lived midpoint relaxation -- the 0x46 shape with only two
 *         jitter draws and no colour ramp.
 *
 * Arm 0x8 is what puts the `v` vector's address in a callee-saved register
 * (it is passed to five calls inside one arm) and what makes this variant
 * 0x98 bytes of frame; both fall out of the arm, not out of the source.
 *
 * Verified byte-identical inside the relinked
 * level_12_magic_crafters_home.ovl.
 *
 * Two locals exist only to reproduce the original's preheader order:
 * `step4` names step * 4 (the amount every fading arm subtracts), so its
 * `sll s5,s4,2` is an explicit pre-loop insn rather than a loop.c movable,
 * and `dir` names &v for the five direction-vector calls in arm 0x8, which
 * puts `addiu s7,sp,32` after it.  Writing them in that order is also what
 * leaves arm 0x8's LAST call reading `v` directly -- a single-use frame
 * address that reload rematerialises (`addiu a0,sp,32`) instead of reading
 * the hoisted register.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_type.h"
#include "spyro_state.h"
#include "emit_record.h"

void Level12_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int p[3];
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

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (pool slot `owner`), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      SVectorToVectorShl2(p, POS);
      if (VISIBLE == 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(dir, &g_pActorListBase[rec[0x1E]].posX);
        v[0] += LookupCosine(AGE << 7) >> 1;
        v[1] += LookupSine(AGE << 7) >> 1;
        v[2] += LookupCosine((AGE << 7) - 0x100) >> 2;
        SubtractVector(dir, dir, p);
        len = VectorLength(dir, VEC_LEN_XYZ);
        if (len < 0x401 || AGE >= 0x41) {
          FreeEmitListRecord(rec);
        } else {
          ScaleVector3ByRatio(dir, len, 0x80);
          AddVector(p, p, dir);
          VectorToSVectorShr2(POS, p);
          AGE++;
          SubtractVector(v, p, g_anCameraPos);
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

    case EMIT_DRIFTING_MOTE: {
      short a;

      AddSVector(POS, POS, VEL);
      a = VEL[0];
      if (a > 0) {
        a--;
        VEL[0] = a;
      } else if (a < 0) {
        a++;
        VEL[0] = a;
      }
      a = VEL[1];
      if (a > 0) {
        a--;
        VEL[1] = a;
      } else if (a < 0) {
        a++;
        VEL[1] = a;
      }
      SPIN += SPIN_RATE;
      d = COL_B - g_nFrameStep;
      if (d <= 0) {
        FreeEmitListRecord(rec);
      } else {
        COL_R = d >> 1;
        COL_G = d >> 1;
        COL_B = d;
        AGE += step;
        if (AGED_OR_CULLED(0x40)) {
          FreeEmitListRecord(rec);
        }
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
      POS_U[0] = HU(0x12) + ((g_anCosineLut[JET_ANGLE] * HS(0x1A)) >> 12);
      POS_U[1] = HU(0x14) + ((g_anSineLut[JET_ANGLE] * HS(0x1A)) >> 12);
      POS_U[2] = HU(0x16) + 0x20 + HU(0x18);
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
