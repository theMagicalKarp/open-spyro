/* Level13_AdvanceEmitList (0x8008af54, level_13_magic_crafters_alpine_ridge
 * overlay, 0x1a84 bytes).
 *
 * The emit-list ADVANCE pass -- the 1697-insn alpine-ridge variant of the walk
 * matched in level_10 / level_11 / level_5.  Walks the 0x20-byte record stream
 * at g_pEmitListBase and ages one record per iteration: integrates its position,
 * fades its colour, ticks its life counter and, when the record has expired
 * (or its visible byte has been cleared (culled)), hands it back with FreeEmitListRecord().
 * The walk stops at the terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Only `rec` is a real induction variable; every field access is an ADDRESS
 * giv off it, which is how the whole record ends up addressed off `rec + 1`
 * with offsets 0x00..0x1D.
 *
 * Field map, in record-relative offsets:
 *   0x00  effect id                0x01  class / terminator
 *   0x02  age                      0x03  visible
 *   0x04/0x06/0x08  tail point     0x0A/0x0C/0x0E  head point (also the
 *                                  r/g/b triple for the fading classes)
 *   0x10..0x16  per-record velocity / colour bytes
 *   0x18  spin angle (a source pointer in classes 0x4 / 0x6, a byte angle
 *         pair in 0x15 / 0x4C)      0x1C  spark vel z   0x1E  phase toggle
 *
 * It carries the two wizard classes 0x4 and 0x5 matched in level_15, plus
 * 0x8 (the homing bolt) and 0x16 from the level_12 body and the boss-ember
 * pair 0x10 / 0x11.
 *
 * SEVEN stack vectors, and the declaration order IS the frame order: arm 0x4
 * takes the first two, arm 0x5 the next two, arm 0x8 the fifth and sixth and
 * arm 0xA the seventh.  Arm 0x8's pair is CSE'd inside the arm, not hoisted,
 * so it reads `v` directly where level_12's copy reads `dv`.  `dv` names &dv for arm 0x4's five direction-vector calls (the
 * original hoists that address into $fp in the preheader); the arm's first
 * and last uses of the same vector read `dv` directly, which is the
 * single-use frame address reload rematerialises as `addiu ..,$sp,0x20`.
 *
 * `burst` (the class-3 constant) never gets a hard register; reload
 * rematerialises it as `li a3,3` before each reference.  The four empty asm
 * statements that take it as an input are references that emit nothing, which
 * leaves the original's four dead `li a3,3` ahead of the vector calls.  The asm
 * use of `step4` gives it one more ref so it beats `dir` for $s7.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_type.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "emit_record.h"

void Level13_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int pt[3];
  int dv[3];
  int qa[3];
  int qb[3];
  int p[3];
  int v[3];
  int w[3];
  int step4;
  int *dir;
  int burst;
  int c;
  int d;

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  step4 = step * 4;
  dir = dv;
  KEEP_IN_REG(step4);
  burst = 3;

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_VEL_DIM:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RISE:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_HOMING_ORB:  /* <- Level15_AdvanceEmitList */
    {
      int len;
      int near;
      int spin;
      int rx;
      int ry;
      int jx;
      int jy;

      /* The wizard's projectile.  Three sub-states, selected by the flag
       * halfword at +0x1E: bit 3 = the free-flying spark trail, bit 0 = the
       * spiral orbit around its own drift point, and neither = the homing
       * run at the owner actor read through the pointer at +0x18.  All three
       * fall into the shared distance test at the bottom, which flips the
       * record between its normal and its "close" colour set (class byte 3).
       */
      KEEP_IN_REG(burst);
      SVectorToVectorShl2(pt, POS);
      if (HU(0x1E) & 8) {
        if (AGE >= 0x30) {
          FreeEmitListRecord(rec);
          break;
        }
        rx = GetRandomU32();
        jx = HU(0x18) - 3;
        jx += rx & 6;
        HU(0x18) = jx;
        ry = GetRandomU32();
        jy = HU(0x1A) - 3;
        jy += ry & 6;
        HU(0x1A) = jy;
        HU(0x1C)++;
        if (HS(0x1C) >= 0x11) {
          HU(0x1C) = 0x10;
        }
        pt[0] += HS(0x18);
        pt[1] += HS(0x1A);
        pt[2] -= HS(0x1C);
        goto commit;
      } else if (HU(0x1E) & 1) {
        if (AGE >= 0x30) {
          AGE = 0;
          HS(0x18) = (GetRandomU32() & 6) - 3;
          HS(0x1A) = (GetRandomU32() & 6) - 3;
          HU(0x1C) = 0;
          HS(0x1E) = 8;
          goto measure;
        } else {
          SVectorToVectorShl2(dv, VEL);
          if (HU(0x1E) & 2) {
            dv[0] += (AGE * LookupCosine(AGE << 7)) >> 7;
          } else {
            dv[0] -= (AGE * LookupCosine(AGE << 7)) >> 7;
          }
          if (HU(0x1E) & 4) {
            dv[1] += (AGE * LookupSine(AGE << 7)) >> 7;
          } else {
            dv[1] -= (AGE * LookupSine(AGE << 7)) >> 7;
          }
          dv[2] += AGE << 5;
          SubtractVector(dir, dir, pt);
          len = VectorLength(dir, VEC_LEN_XYZ);
          if (len >= 0x81) {
            ScaleVector3ByRatio(dir, len, 0x80);
          }
          AddVector(pt, pt, dir);
          goto commit;
        }
      } else {
        CopyVector(dir, (int *)(W(0x18) + 0xC));
        dv[0] += LookupCosine(AGE << 7) >> 2;
        dv[1] += LookupSine(AGE << 7) >> 2;
        dv[2] += LookupCosine((AGE << 7) + 0x200) >> 2;
        SubtractVector(dir, dir, pt);
        len = VectorLength(dir, VEC_LEN_XYZ);
        if (len < 0x401 || AGE >= 0x41) {
          int *owner = *(int **)W(0x18);

          if (owner[5] == 1) {
            owner[5] = 2;
            owner[0] = 0x20;
          }
          HS(0x1E) = (GetRandomU32() & 6) + 1;
          VectorToSVectorShr2(VEL, (int *)(W(0x18) + 0xC));
          spin = GetRandomU32() & 0x7F;
          AGE = 0;
          HU(0x1C) += spin;
          goto measure;
        }
        ScaleVector3ByRatio(dir, len, 0x100);
        AddVector(pt, pt, dir);
      }
    commit:
      KEEP_IN_REG(burst);
      VectorToSVectorShr2(POS, pt);
      AGE++;
    measure:
      SubtractVector(dv, pt, g_anCameraPos);
      near = abs(dv[0]) + abs(dv[1]);
      len = near + abs(dv[2]);
      if (CLASS == burst) {
        if (len >= 0x3001) {
          COL_R = 0xFF;
          COL_G = 0xFF;
          COL_B = 0xFF;
          SIZE = 4;
          CLASS = 0;
          rec[0xF] = 0x40;
        }
      } else if (len < 0x2800) {
        CLASS = burst;
        COL_R = 0x80;
        COL_G = 0x80;
        COL_B = 0x80;
        DEPTH_BIAS = 4;
        rec[0xF] = 0x2C;
        TEX_INDEX = 0;
        SIZE = (GetRandomU32() & 0xF) + 0x18;
        SPIN = (GetRandomU32() & 0x7) + 0xF;
      }
      break;
    }

    case EMIT_JITTER_DRIFT:  /* <- Level15_AdvanceEmitList */
    {
      int rq;
      int rr;
      int jq;
      int jr;
      int dec;

      /* The short spawn drift: the record walks along its own jittered
       * velocity, plus a quarter of the record velocity vector. */
      if (AGED_OR_CULLED(0x30)) {
        goto expire;
      } else {
        SVectorToVectorShl2(qa, POS);
        rq = GetRandomU32();
        jq = HU(0x18) - 3;
        jq += rq & 6;
        HU(0x18) = jq;
        rr = GetRandomU32();
        jr = HU(0x1A) - 3;
        dec = HU(0x1C) - 1;
        jr += rr & 6;
        HU(0x1A) = jr;
        HU(0x1C) = dec;
        SVectorToVectorShl2(qb, VEL);
        RShiftVector3(qb, 2);
        AddVector(qa, qa, qb);
        VectorToSVectorShr2(POS, qa);
        AGE++;
      }
      break;
    }

    case EMIT_TRACKING_SPARK:  /* <- Level12_AdvanceEmitList */
    {
      int len;
      int *home;
      int *bolt;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (pool slot `owner`), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      KEEP_IN_REG(burst);
      SVectorToVectorShl2(p, POS);
      EBB_BREAK();
      if (VISIBLE == 0) {
        goto expire;
      } else {
        bolt = v;
        CopyVector(bolt, &g_pActorListBase[rec[0x1E]].posX);
        v[0] += LookupCosine(AGE << 7) >> 1;
        v[1] += LookupSine(AGE << 7) >> 1;
        v[2] += LookupCosine((AGE << 7) - 0x100) >> 2;
        home = p;
        SubtractVector(bolt, bolt, home);
        len = VectorLength(bolt, VEC_LEN_XYZ);
        if (len < 0x401 || AGE >= 0x41) {
          goto expire;
        } else {
          ScaleVector3ByRatio(bolt, len, 0x80);
          AddVector(home, home, bolt);
          KEEP_IN_REG(burst);
          VectorToSVectorShr2(POS, home);
          AGE++;
          home = g_anCameraPos;
          bolt = home;
          SubtractVector(v, p, g_anCameraPos);
          c = abs(v[0]) + abs(v[1]);
          len = c + abs(v[2]);
          if (CLASS == burst) {
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
            CLASS = burst;
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

    case EMIT_SUPERCHARGE_TRAIL:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPYRO_ORBIT_SPARKLE:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < SPARX_GONE) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        goto expire;
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

    case EMIT_BURST_SPARK:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, &HS(0x12));
      FADE_COL(8);
      if (COL_R == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_ACTOR_SPARK:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
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

    case EMIT_FADING_SPARK:  /* <- Level18_AdvanceEmitList */
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

    case EMIT_SPARK_RGB:  /* <- Level18_AdvanceEmitList */
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

    case EMIT_RISING_SPIRAL:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFTING_MOTE:  /* <- Level12_AdvanceEmitList */
    {
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
      SPIN += rec[0x1E];
      d = COL_B - g_nFrameStep;
      if (d <= 0) {
        goto expire;
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

    case EMIT_RESCUE_SWIRL:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DUST_PUFF:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
      int c;

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

    case EMIT_TWINKLE:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_STREAK_BURST:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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
        goto expire;
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

    case EMIT_STREAK_BURST_WHITE:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x20)) {
        goto expire;
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

    case EMIT_CRAWLING_STREAK:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x30)) {
        goto expire;
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

    case EMIT_RISING_SPIRAL_SMEAR:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFT_STREAK:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_EMBER:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
      int j;
      int k;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x28)) {
        goto expire;
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

    case EMIT_FLAME_STREAK:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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
        goto expire;
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

    case EMIT_FALLING_STREAK_BURST:  /* <- Level15_AdvanceEmitList */
    /* <- Level10_AdvanceEmitList */
    {
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
    goto next;

  expire:
    FreeEmitListRecord(rec);

  next:
    rec += 0x20;
  } while (CLASS != EMIT_CLASS_END);
}
