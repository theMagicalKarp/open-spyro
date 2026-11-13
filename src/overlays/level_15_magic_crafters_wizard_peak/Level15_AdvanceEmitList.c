/* Level15_AdvanceEmitList (0x80087b40, level_15_magic_crafters_wizard_peak overlay,
 * 0x1910 bytes).
 *
 * The emit-list ADVANCE pass -- the 1604-insn wizard-peak variant of the walk
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
 * Field layout: one named view per effect arm, in level_advance_emit_list.h
 * (POS / HEAD points, SIZE / SPIN / COL_* / FX, VEL, DRIFT, the swirl and
 * orb fields); RasterizeEmitList's reading of each class is in emit_record.h.
 *
 * Beyond the shared ground-level arm set this variant adds the two wizard
 * classes 0x4 and 0x5, and takes 0x7 / 0x41 from the level_12 body.
 *
 * SIX stack vectors, and the declaration order IS the frame order: arm 0x4
 * takes the first two, arm 0x5 the next two, arm 0x6 the fifth and arm 0xA
 * the sixth.  `dir` names &dv for arm 0x4's five direction-vector calls (the
 * original hoists that address into $fp in the preheader); the arm's first
 * and last uses of the same vector read `dv` directly, which is the
 * single-use frame address reload rematerialises as `addiu ..,$sp,0x20`.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_modes.h"
#include "spyro_state.h"
#include "emit_record.h"
#include "actor_states.h"

void Level15_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int pt[3];
  int dv[3];
  int qa[3];
  int qb[3];
  int p[3];
  int v[3];
  int *dir;
  int step4;

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  step4 = step * 4;
  dir = dv;

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_VEL_DIM:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RISE:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step4;
      FADE_COL(step4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_HOMING_ORB: {
      int len;
      int near;
      int rise;
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
      SVectorToVectorShl2(pt, POS);
      if (ORB_MODE_U & ORB_TRAIL) {
        if (AGE >= 0x30) {
          FreeEmitListRecord(rec);
          break;
        }
        rx = GetRandomU32();
        jx = VEL_U[0] - 3;
        jx += rx & 6;
        VEL_U[0] = jx;
        ry = GetRandomU32();
        jy = VEL_U[1] - 3;
        jy += ry & 6;
        VEL_U[1] = jy;
        VEL_U[2]++;
        if (VEL[2] >= 0x11) {
          VEL_U[2] = 0x10;
        }
        pt[0] += VEL[0];
        pt[1] += VEL[1];
        pt[2] -= VEL[2];
        goto commit;
      } else if (ORB_MODE_U & ORB_ORBIT) {
        if (AGE >= 0x30) {
          AGE = 0;
          VEL[0] = (GetRandomU32() & 6) - 3;
          VEL[1] = (GetRandomU32() & 6) - 3;
          VEL_U[2] = 0;
          ORB_MODE = ORB_TRAIL;
          goto measure;
        } else {
          SVectorToVectorShl2(dv, VEL);
          if (ORB_MODE_U & ORB_X_UP) {
            dv[0] += (AGE * LookupCosine(AGE << 7)) >> 7;
          } else {
            dv[0] -= (AGE * LookupCosine(AGE << 7)) >> 7;
          }
          if (ORB_MODE_U & ORB_Y_UP) {
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
        CopyVector(dir, &ORB_TARGET->posX);
        dv[0] += LookupCosine(AGE << 7) >> 2;
        dv[1] += LookupSine(AGE << 7) >> 2;
        dv[2] += LookupCosine((AGE << 7) + 0x200) >> 2;
        SubtractVector(dir, dir, pt);
        len = VectorLength(dir, VEC_LEN_XYZ);
        if (len < 0x401 || AGE >= 0x41) {
          TetherState *target = ORB_TARGET->state;

          if (target->mode == TCMD_BOUND) {
            target->mode = TCMD_FREED; /* the orb frees a bound guard */
            target->timer = TGUARD_ORB_FREE_FRAMES;
          }
          ORB_MODE = (GetRandomU32() & (ORB_X_UP | ORB_Y_UP)) + ORB_ORBIT;
          VectorToSVectorShr2(VEL, &ORB_TARGET->posX);
          rise = GetRandomU32() & 0x7F;
          AGE = 0;
          VEL_U[2] += rise;
          goto measure;
        }
        ScaleVector3ByRatio(dir, len, 0x100);
        AddVector(pt, pt, dir);
      }
    commit:
      VectorToSVectorShr2(POS, pt);
      AGE++;
    measure:
      SubtractVector(dv, pt, g_anCameraPos);
      near = abs(dv[0]) + abs(dv[1]);
      len = near + abs(dv[2]);
      if (CLASS == EMIT_CLASS_ALT_LIT_QUAD) {
        if (len >= 0x3001) {
          COL_R = 0xFF;
          COL_G = 0xFF;
          COL_B = 0xFF;
          SIZE = 4;
          CLASS = EMIT_CLASS_DOT;
          FX = EMIT_FX_LINE;
        }
      } else if (len < 0x2800) {
        CLASS = 3;
        COL_R = 0x80;
        COL_G = 0x80;
        COL_B = 0x80;
        DEPTH_BIAS = 4;
        FX = EMIT_FX_QUAD;
        TEX_INDEX = 0;
        SIZE = (GetRandomU32() & 0xF) + 0x18;
        SPIN = (GetRandomU32() & 0x7) + 0xF;
      }
      break;
    }

    case EMIT_JITTER_DRIFT: {
      int rq;
      int rr;
      int jq;
      int jr;
      int dec;

      /* The short spawn drift: the record walks along its own jittered
       * velocity, plus a quarter of the record velocity vector. */
      if (AGED_OR_CULLED(0x30)) {
        FreeEmitListRecord(rec);
      } else {
        SVectorToVectorShl2(qa, POS);
        rq = GetRandomU32();
        jq = VEL_U[0] - 3;
        jq += rq & 6;
        VEL_U[0] = jq;
        rr = GetRandomU32();
        jr = VEL_U[1] - 3;
        dec = VEL_U[2] - 1;
        jr += rr & 6;
        VEL_U[1] = jr;
        VEL_U[2] = dec;
        SVectorToVectorShl2(qb, VEL);
        RShiftVector3(qb, 2);
        AddVector(qa, qa, qb);
        VectorToSVectorShr2(POS, qa);
        AGE++;
      }
      break;
    }

    case EMIT_ACTOR_PLUME:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFT_SPARKLE:  /* <- Level12_AdvanceEmitList */
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

    case EMIT_SUPERCHARGE_TRAIL:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPYRO_HURT_SPARKLE:  /* <- Level10_AdvanceEmitList */
      /* A hurt sparkle: circles Spyro (0x40 a tick) while his post-hit
       * invulnerability runs; freed early if he is hidden or a hazard kills him. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < SPARX_GONE) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInvulnFrames[0] <= 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(v, SPYRO_POS_VIA_INVULN);
        v[0] += LookupCosine(ORBIT_ANGLE) >> 4;
        v[1] += LookupSine(ORBIT_ANGLE) >> 4;
        v[2] += 0x100;
        VectorToSVectorShr2(POS, v);
        ORBIT_ANGLE = ORBIT_ANGLE_U + 0x40;
        SIZE = abs(LookupCosine((ORBIT_ANGLE - g_nCameraEulerYaw) & ANGLE12_MASK)) / 170;
      }
      break;

    case EMIT_BURST_SPARK:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, BURST_VEL);
      FADE_COL(8);
      if (COL_R == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_ACTOR_SPARK:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_RISING_SPIRAL:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_RESCUE_SWIRL:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_DUST_PUFF:  /* <- Level10_AdvanceEmitList */
    {
      int c;

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

    case EMIT_ACTOR_WHITE_STREAK:  /* <- Level12_AdvanceEmitList */
    {
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

    case EMIT_TWINKLE:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_STREAK_BURST:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_STREAK_BURST_WHITE:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_CRAWLING_STREAK:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_RISING_SPIRAL_SMEAR:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFT_STREAK:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_EMBER:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_FLAME_STREAK:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_FALLING_STREAK_BURST:  /* <- Level10_AdvanceEmitList */
    {
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
