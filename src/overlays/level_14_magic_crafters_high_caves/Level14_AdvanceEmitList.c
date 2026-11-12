/* Level14_AdvanceEmitList (0x80089454, level_14_magic_crafters_high_caves
 * overlay, 0x1c5c bytes).
 *
 * The emit-list ADVANCE pass -- the 1815-insn high-caves variant, the largest
 * of the 35 and the last one in the family with undecoded arms.  Walks the
 * 0x20-byte record stream at g_pEmitListBase and ages one record per iteration,
 * handing expired records back with FreeEmitListRecord().  The walk stops at the
 * terminator record, whose class byte at +0x01 is 0xFF.
 *
 * 28 classes.  Arms 0x4/0x5 come from the matched Level15_AdvanceEmitList,
 * 0x8/0x10/0x11 and the seven-vector frame from the level_13 park, 0x1E/0x1F
 * from the level_27 park and the rest from level_0 / level_9 / level_10.
 * Two arms are new here:
 *   0x45  the guided stream-follower: reads a waypoint list through the
 *         pointer at +0x18, steps it by the wrap counter at +0x1C, adds a
 *         unit-circle wobble to the waypoint, and dies when the wrap counter
 *         reaches the list's length byte at [0].
 *   0x49  the two-axis orbit: radius rec[0x1F] swept by (rec[0x1E] + age) << 6
 *         into the tail point, with the head point tracking age * 2, all three
 *         then advanced by the per-record velocity halfwords at 0x18/0x1A/0x1C.
 *
 * As in level_13, `burst` is never allocated and the four empty asm inputs
 * reproduce the original's dead `li a3,3` reloads ahead of the vector calls.
 * `step2` is a named local declared first so its spill slot precedes step4's.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_modes.h"
#include "actor_type.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "emit_record.h"
#include "actor_states.h"

void Level14_AdvanceEmitList(int step) {
  unsigned char *rec = g_pEmitListBase;
  int pt[3];
  int dv[3];
  int qa[3];
  int qb[3];
  int p[3];
  int v[3];
  int w[3];
  int cur[3];
  int adv[3];
  int step2;
  int *dir;
  int step4;
  int burst;
  int c;
  int d;

  if (CLASS == EMIT_CLASS_END) {
    return;
  }

  step2 = step * 2;
  step4 = step * 4;
  dir = dv;
  burst = 3;

  do {
    switch (rec[0]) {
    case EMIT_SPARK_VEL:
      AddSVector(POS, POS, VEL);
      SPARK_Z_CLIMB();
      SIZE += step2;
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

    case EMIT_HOMING_ORB:
    {
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
      KEEP_IN_REG(burst);
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
          CLASS = EMIT_CLASS_DOT;
          FX = EMIT_FX_LINE;
        }
      } else if (len < 0x2800) {
        CLASS = burst;
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

    case EMIT_JITTER_DRIFT:
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

    case EMIT_TRACKING_SPARK:
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
        CopyVector(bolt, &g_pActorListBase[TRACK_ACTOR].posX);
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
              FX = EMIT_FX_LINE;
              CLASS = EMIT_CLASS_DOT;
              c = COL_R * 2;
              COL_R = c;
              COL_G *= 2;
              COL_B *= 2;
            }
          } else if (len < 0x2800) {
            CLASS = burst;
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
      SIZE += step2;
      AGE += step;
      if (AGED_OR_CULLED(0x20)) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPYRO_ORBIT_SPARKLE:
    /* <- Level10_AdvanceEmitList */
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < SPARX_GONE) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        goto expire;
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

    case EMIT_FADING_SPARK:
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

    case EMIT_SPARK_RGB:
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

    case EMIT_RISING_SPIRAL:
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

    case EMIT_RESCUE_SWIRL:
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

    case EMIT_RISING_PUFF:
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

    case EMIT_DUST_PUFF:
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

    case EMIT_TWINKLE:
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

    case EMIT_PATH_DOT:
    {
      unsigned char *path;
      int *base;
      int *hop;
      int len;

      path = (unsigned char *)PDOT_PATH;
      base = cur;
      SVectorToVectorShl2(base, POS);
      hop = adv;
      CopyVector(hop, (int *)&path[PDOT_WAYPOINT * 0x10 + 8]);
      adv[0] += LookupCosine(AGE << 7) >> 2;
      adv[1] += LookupSine(AGE << 7) >> 2;
      adv[2] += LookupCosine((AGE << 7) + 0x200) >> 2;
      SubtractVector(hop, hop, base);
      len = VectorLength(hop, VEC_LEN_XYZ);
      if (len < 0x401) {
        PDOT_WAYPOINT++;
        if (PDOT_WAYPOINT >= path[0]) {
          FreeEmitListRecord(rec);
          break;
        }
      }
      base = adv;
      ScaleVector3ByRatio(base, len, 0x100);
      hop = cur;
      AddVector(hop, hop, base);
      VectorToSVectorShr2(POS, hop);
      AGE++;
      break;
    }

    case EMIT_STREAK_BURST:
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

    case EMIT_STREAK_BURST_WHITE:
    {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x20)) {
        goto expire;
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

    case EMIT_CRAWLING_STREAK:
    {
      int d[3];
      int j;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x30)) {
        goto expire;
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

    case EMIT_ORBIT_DOT:
      if (VISIBLE == 0 || AGE >= 0x21) {
        FreeEmitListRecord(rec);
        break;
      }
      POS[0] = (ODOT_RADIUS * LookupCosine((ODOT_PHASE + AGE) << 6)) >> 10;
      POS[1] = (ODOT_RADIUS * LookupSine((ODOT_PHASE + AGE) << 6)) >> 10;
      POS[2] = AGE * 2;
      AGE++;
      POS_U[0] += ODOT_CENTRE_U[0];
      POS_U[1] += ODOT_CENTRE_U[1];
      POS_U[2] += ODOT_CENTRE_U[2];
      break;

    case EMIT_RISING_SPIRAL_SMEAR:
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

    case EMIT_DRIFT_STREAK:
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

    case EMIT_EMBER:
    {
      int j;
      int k;
      int r;

      AGE++;
      if (AGED_OR_CULLED(0x28)) {
        goto expire;
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

    case EMIT_FLAME_STREAK:
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

    case EMIT_FALLING_STREAK_BURST:
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
    goto next;

  expire:
    FreeEmitListRecord(rec);

  next:
    rec += 0x20;
  } while (CLASS != EMIT_CLASS_END);
}
