/* Level9_AdvanceEmitList (0x80083f2c, level_9_peace_keepers_ice_cavern overlay,
 * 0x1258 bytes).
 *
 * The emit-list ADVANCE pass -- the 1174-insn ice-cavern
 * variant of the walk matched in level_11 / level_5.  Walks the 0x20-byte
 * record stream at g_pEmitListBase and ages one record per iteration: integrates
 * its position, fades its colour, ticks its life counter and, when the record
 * has expired (or its visible byte has been cleared (culled)), hands it back with
 * FreeEmitListRecord().  The walk stops at the terminator record, whose class byte
 * at +0x01 is 0xFF.
 *
 * Only `rec` is a real induction variable.  `pos` and `vel` are separate
 * pointers because the arms pass them to AddSVector() as values, so
 * loop.c strength-reduces each into its own register; every field access is
 * an ADDRESS giv off `rec`, and loop.c picks the last one it scans -- the
 * `rec[1]` terminator test -- as the shared base, which is how the whole
 * record ends up addressed off `rec + 1` with offsets 0x00..0x1D.  Writing
 * the fields against a `rec + 1` pointer local instead costs a fifth
 * induction variable.
 *
 * Field layout: one named view per effect arm, in level_advance_emit_list.h
 * (POS / HEAD points, SIZE / SPIN / COL_* / FX, VEL, DRIFT, the swirl and
 * orb fields); RasterizeEmitList's reading of each class is in emit_record.h.
 *
 * The ground-level arm set: it drops the flight classes 0xD/0x13/0x1A/0x1B
 * and adds 0x6 (the orbiting spark, which reads a world point through the
 * pointer at +0x18), 0x15 and 0x4C (the two unit-circle swirls, which share
 * a cross-jumped sine tail), 0x18 (EMIT_RESCUE_SWIRL, whose radius and height track
 * g_nDragonRescueTimer), 0x46 and 0x50 (two midpoint-relaxation
 * classes).  Against level_2's set this copy adds EMIT_JITTER_BURST, EMIT_JITTER_SPARKLE and drops
 * EMIT_ACTOR_PLUME.
 *
 * Arm 0x15's third read of g_nFrameStep goes through a const-qualified deref.
 * That is load-bearing, not decoration: without it sched1 makes the load
 * TRUE-dependent on the `POS_U[0]` store three statements above (a store
 * through `rec` may alias the global), which lifts the whole `* 6` chain to
 * dependence depth 16 -- one above the `SPIN += 4` byte store -- and the
 * two independent chains come out complete-then-complete instead of the
 * original's interleave.  `true_dependence()` returns 0 outright for an
 * unchanging read, so the const drops the chain to depth 2 and the byte
 * store leads.  The same read stays plain at its other two use sites.
 *
 * Verified byte-identical inside the relinked
 * level_9_peace_keepers_ice_cavern.ovl.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "spyro_state.h"
#include "emit_record.h"

void Level9_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int v[3];

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 2;
      FADE_COL(step * 4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_VEL_DIM:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 4;
      FADE_COL(step * 4);
      AGE += step;
      if (AGED_OR_CULLED(0x18)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RISE:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step * 4;
      FADE_COL(step * 4);
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
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
        SIZE = abs(LookupCosine((ORBIT_ANGLE - g_nCameraEulerYaw) & 0xFFF)) / 170;
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

    case EMIT_JITTER_BURST: {
      int a[3];
      int b[3];
      int rq;
      int rr;
      int jq;
      int jr;
      int dec;

      /* The ice-cavern drift: the record walks along its own jittered spin
       * pair plus the record velocity vector.  0xF and 0x4B are the same body
       * on different stack vectors and share a cross-jumped tail. */
      if (AGED_OR_CULLED(0x30)) {
        FreeEmitListRecord(rec);
      } else {
        SVectorToVectorShl2(a, POS);
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
        SVectorToVector(b, VEL);
        AddVector(a, a, b);
        VectorToSVectorShr2(POS, a);
        AGE++;
      }
      break;
    }

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

    case EMIT_JITTER_SPARKLE: {
      int a[3];
      int b[3];
      int rq;
      int rr;
      int jq;
      int jr;
      int dec;

      /* The ice-cavern drift: the record walks along its own jittered spin
       * pair plus the record velocity vector.  0xF and 0x4B are the same body
       * on different stack vectors and share a cross-jumped tail. */
      if (AGED_OR_CULLED(0x30)) {
        FreeEmitListRecord(rec);
      } else {
        SVectorToVectorShl2(a, POS);
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
        SVectorToVector(b, VEL);
        AddVector(a, a, b);
        VectorToSVectorShr2(POS, a);
        AGE++;
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
