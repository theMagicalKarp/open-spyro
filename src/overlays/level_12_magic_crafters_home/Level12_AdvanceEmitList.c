/* Level12_AdvanceEmitList (0x8008BE98, level_12_magic_crafters_home overlay,
 * 0x1438 bytes).
 *
 * The emit-list ADVANCE pass -- the Magic-Crafters-home 1294-insn variant,
 * the largest F16-free copy in the family.  Walks the 0x20-byte record
 * stream at g_pEmitListBase and ages one record per iteration: integrates its
 * position, fades its colour, ticks its life counter and, when the record
 * has expired (or its alive byte has been cleared), hands it back with
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
 *   0x02  age                      0x03  alive
 *   0x04/0x06/0x08  tail point     0x0A/0x0C/0x0E  head point (also the
 *                                  r/g/b triple for the fading classes)
 *   0x10..0x16  per-record velocity / colour bytes
 *   0x18  spin angle                0x1C  wrap count    0x1E  phase toggle
 *
 * Eighteen arms transfer verbatim from the matched level_0 hub copy (which
 * in turn carries the level_6 / level_2 donors).  Three are new:
 *   0x7   the rune orbit -- a triangular height ramp off the age byte;
 *   0x8   the wizard's homing bolt, the only arm in the family that reads
 *         the owner ACTOR (g_pActorListBase + owner * 0x58) and spirals around
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

    case EMIT_SPARK_VEL_DIM:
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

    case EMIT_SPARK_RISE:
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

    case EMIT_DRIFT_SPARKLE:
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

    case EMIT_TRACKING_SPARK: {
      int len;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (g_pActorListBase + owner * 0x58), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      SVectorToVectorShl2(p, POS);
      if (ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(dir, (int *)(g_pActorListBase + rec[0x1E] * ACTOR_SIZE + ACTOR_OFFSET(posX)));
        v[0] += LookupCosine(AGE << 7) >> 1;
        v[1] += LookupSine(AGE << 7) >> 1;
        v[2] += LookupCosine((AGE << 7) - 0x100) >> 2;
        SubtractVector(dir, dir, p);
        len = VectorLength(dir, 1);
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

    case EMIT_SPEED_DUST:
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

    case EMIT_SPYRO_ORBIT_SPARKLE:
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < 0) || g_nSpyroDrawSuppressed != 0 ||
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

    case EMIT_ACTOR_SPARK:
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

    case EMIT_DRIFTING_MOTE: {
      short a;

      AddSVector(POS, POS, VEL);
      a = HS(0x18);
      if (a > 0) {
        a--;
        HS(0x18) = a;
      } else if (a < 0) {
        a++;
        HS(0x18) = a;
      }
      a = HS(0x1A);
      if (a > 0) {
        a--;
        HS(0x1A) = a;
      } else if (a < 0) {
        a++;
        HS(0x1A) = a;
      }
      rec[0xB] += rec[0x1E];
      d = rec[0xE] - g_nFrameStep;
      if (d <= 0) {
        FreeEmitListRecord(rec);
      } else {
        rec[0xC] = d >> 1;
        rec[0xD] = d >> 1;
        rec[0xE] = d;
        AGE += step;
        if (AGE >= 0x40 || ALIVE == 0) {
          FreeEmitListRecord(rec);
        }
      }
      break;
    }

    case EMIT_RESCUE_SWIRL: {
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
      if (g_nDragonRescueTimer < 0x40) {
        heat = g_nDragonRescueTimer;
      }
      HS(0x18) = (rec[0x1D] * (0x48 - heat)) >> 3;
      HS(0x1A) = (((0x20 - rec[0x1D]) * heat) >> 4) + 8;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x6) = HU(0x14) + ((g_anSineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x8) = HU(0x16) + 0x20 + HU(0x18);
      break;
    }

    case EMIT_DUST_PUFF: {
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
      if (AGE >= 0x8 || ALIVE == 0) {
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
      if (AGE >= 0x18 || ALIVE == 0) {
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

    case EMIT_STREAK_BURST_WHITE: {
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
      if (AGE >= 0x20 || ALIVE == 0) {
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
