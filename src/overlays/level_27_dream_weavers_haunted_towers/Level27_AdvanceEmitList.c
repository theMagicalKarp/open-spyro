/* Matching notes.
 *  - Arms 0x1D and 0x1F are freshly decoded here; everything else is donor C.
 *  - `aim` names &dv and `hold` names &pt.  Both are needed and their ORDER is
 *    load-bearing three times over: `step2 = ..; aim = dv; step4 = ..` puts
 *    aim's def between the two shifts so step4 keeps $fp and aim spills;
 *    declaring step2/step4 BEFORE aim gives aim the higher spill slot (0x70 vs
 *    0x78 follows pseudo number); and `SVectorToVectorShl2(dv, VEL)` must use the
 *    ARRAY name so that one site rematerialises instead of reloading.
 *  - The empty `do { } while (0);` after the arm-top `SVectorToVectorShl2(pt, POS)`
 *    is an A200b barrier and is worth 2 insns: without it the arm-top and the
 *    bit-3 `VectorToSVectorShr2(POS, pt)` share one CSE'd `addiu s0,sp,48`.
 *  - `ScaleVector3ByRatio` is written in BOTH branches and `AddVector` only once
 *    behind `shove:`.  That exact split is what makes step4 win $fp: with both
 *    calls inline &dv has one ref too many (4*23/1551 beats 4*23/1554) and it
 *    takes the register instead, costing an `lw` per arm across the function.
 *  - Arm 0x1D's patrol node is `(int *)(idx * 4 + (int)actor->state)` in INT
 *    arithmetic (A149): the pointer form canonicalises the addu to put the
 *    deref first, which ties the sum to the deref's register instead of
 *    idx's and swaps the lbu between $v0 and $a2.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_type.h"
#include "actor_modes.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "emit_record.h"
#include "level_update_actors.h"
#include "level_actor_states.h"

/* Level27_AdvanceEmitList (0x80086754, level_27_dream_weavers_haunted_towers
 * overlay, 0x1a84 bytes).
 *
 * The emit-list ADVANCE pass -- the 1697-insn haunted-towers variant of the
 * walk matched in level_10 / level_15.  Walks the 0x20-byte record stream at
 * g_pEmitListBase and ages one record per iteration: integrates its position, fades
 * its colour, ticks its life counter and, when the record has expired (or its
 * visible byte has been cleared), hands it back with FreeEmitListRecord().  The walk
 * stops at the terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Field layout: one named view per effect arm, in level_advance_emit_list.h
 * (POS / HEAD points, SIZE / SPIN / COL_* / FX, VEL, DRIFT, the swirl and
 * orb fields); RasterizeEmitList's reading of each class is in emit_record.h.
 *
 * Beyond the shared ground-level arm set this variant adds 0x1D (the ghost,
 * a three-sub-state patrol) and 0x1F (a long-lived plain drifter), and takes
 * 0xD from level_5, 0x10 / 0x11 from level_18, 0x1E from level_0 and
 * 0x7 / 0x41 from level_15.
 *
 * FOUR stack vectors, and the declaration order IS the frame order: arm 0x6
 * takes the first, arm 0xA the second and arm 0x1D the third and fourth.
 * Arm 0x1D addresses its pair by NAME: it uses each more than twice, so
 * loop.c hoists both addresses into the preheader and spills them, which is
 * what the original does (`addiu $t0,$sp,0x40; sw $t0,0x70($sp)`).
 */

void Level27_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int p[3];
  int v[3];
  int pt[3];
  int dv[3];
  int step2;
  int step4;
  int *aim;
  int *hold;

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  step2 = step * 2;
  step4 = step * 4;
  aim = dv;
  hold = pt;

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step2;
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

    case EMIT_DRIFT_SPARKLE:  /* <- Level15_AdvanceEmitList */
    /* <- Level12_AdvanceEmitList */
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
      SIZE += step2;
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

    case EMIT_SPARK_YELLOW:  /* <- Level5_AdvanceEmitList */
    {
      int c;

      if (AGED_OR_CULLED(0x1C)) {
        FreeEmitListRecord(rec);
      } else {
        AddSVector(POS, POS, VEL);
        SPARK_Z_CLIMB();
        SIZE += step2;
        if (AGE >= 0xD) {
          c = 0x80 - (AGE - 0xC) * 8;
          if (c < 0) {
            c = 0;
          }
          COL_R = c;
          c = 0x40 - (AGE - 0xC) * 4;
          if (c < 0) {
            c = 0;
          }
          COL_G = c;
          COL_B = 0;
        } else if (AGE >= 5) {
          COL_R = 0x80;
          COL_B = 0;
          COL_G = -0x80 - (AGE - 4) * 8;
        } else {
          COL_R = 0x80;
          COL_G = 0x80;
          c = 0x80 - AGE * 32;
          if (c < 0) {
            c = 0;
          }
          COL_B = c;
        }
        AGE += step;
      }
      break;
    }

    case EMIT_FADING_SPARK:  /* <- Level18_AdvanceEmitList */
      /* Gnasty's cauldron drip: a slow ember that dies when its red
       * channel bottoms out. */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step2;
      FADE_COL(4);
      if (COL_R == 0 || VISIBLE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RGB:  /* <- Level18_AdvanceEmitList */
      /* The same ember with a late red cut-off at age 9. */
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step2;
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

    case EMIT_PATH_WISP: {
      int len;
      int rise;
      int rx;
      int ry;
      int jx;
      int jy;
      FenceCaptainState *cap;
      char *wp;
      Actor *actor;
      int idx;

      /* The haunted-towers ghost.  Three sub-states, selected by the flag
       * halfword at +0x1E: bit 3 = the free-flying trail, bit 0 = the spiral
       * orbit around its own drift point, and neither = the patrol run, which
       * walks the waypoint list its fence captain (WISP_CAPTAIN) keeps for
       * guard WISP_GUARD and, on reaching the last waypoint, wakes that guard
       * (FGUARD_SIG_WAKE) and re-arms itself as an orbit.
       */
      SVectorToVectorShl2(pt, POS);
      EBB_BREAK();
      if (ORB_MODE_U & ORB_TRAIL) {
        if (AGE >= 0x18) {
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
        VectorToSVectorShr2(POS, pt);
        AGE++;
      } else if (ORB_MODE_U & ORB_ORBIT) {
        if (AGE >= 0x18) {
          AGE = 0;
          VEL[0] = (GetRandomU32() & 6) - 3;
          VEL[1] = (GetRandomU32() & 6) - 3;
          VEL_U[2] = 0;
          ORB_MODE = ORB_TRAIL;
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
          SubtractVector(aim, aim, hold);
          len = VectorLength(aim, VEC_LEN_XYZ);
          if (len >= 0x81) {
            ScaleVector3ByRatio(aim, len, 0x80);
          }
          goto shove;
        }
      } else {
        idx = WISP_GUARD;
        actor = &g_pActorListBase[WISP_CAPTAIN];
        cap = actor->state;
        wp = cap->wispPath[idx];
        actor = &g_pActorListBase[cap->link[idx]];
        SVectorToVectorShl2(hold, POS);
        CopyVector(aim, (int *)(wp + (WISP_WAYPOINT * 0x10 + 8)));
        dv[0] += LookupCosine(AGE << 7) >> 2;
        dv[1] += LookupSine(AGE << 7) >> 2;
        dv[2] += LookupCosine((AGE << 7) + 0x200) >> 2;
        SubtractVector(aim, aim, hold);
        len = VectorLength(aim, VEC_LEN_XYZ);
        if (len < 0x401) {
          WISP_WAYPOINT++;
          if (WISP_WAYPOINT >= *(unsigned char *)wp) {
            actor->subMode = FGUARD_SIG_WAKE;
            ORB_MODE = (GetRandomU32() & (ORB_X_UP | ORB_Y_UP)) + ORB_ORBIT;
            VectorToSVectorShr2(VEL, &actor->posX);
            rise = GetRandomU32() & 0x7F;
            AGE = 0;
            VEL_U[2] += rise;
            break;
          }
        }
        ScaleVector3ByRatio(aim, len, 0x100);
      shove:
        AddVector(hold, hold, aim);
        VectorToSVectorShr2(POS, hold);
        AGE++;
      }
      break;
    }

    case EMIT_RISING_PUFF:  /* <- Level0_AdvanceEmitList */
      POS_U[2] += RISE_SPEED_U;
      SIZE++;
      FADE_COL(step2);
      AGE += step;
      if (AGE >= 0x20) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPREAD_MOTE:
      /* A plain drifter with a long life. */
      AddSVector(POS, POS, VEL);
      AGE += step;
      if (AGED_OR_CULLED(0x40)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_DUST_PUFF:  /* <- Level10_AdvanceEmitList */
    {
      int c;

      AddSVector(POS, POS, VEL);
      if (CLIMB_TOGGLE != 0) {
        VEL_U[2]++;
      }
      c = COL_R - step;
      CLIMB_TOGGLE = 1 - CLIMB_TOGGLE_U;
      SIZE += step2;
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

    case EMIT_ACTOR_WHITE_STREAK:  /* <- Level15_AdvanceEmitList */
    /* <- Level12_AdvanceEmitList */
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
