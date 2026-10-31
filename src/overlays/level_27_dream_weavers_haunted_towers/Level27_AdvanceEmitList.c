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
 *  - Arm 0x1D's patrol node is `(int *)(idx * 4 + *(int *)actor)` in INT
 *    arithmetic (A149): the pointer form canonicalises the addu to put the
 *    deref first, which ties the sum to the deref's register instead of
 *    idx's and swaps the lbu between $v0 and $a2.
 */

#include "vecmath.h"
#include "level_globals.h"
#include "level_advance_emit_list.h"
#include "actor_type.h"
#include "match_asm.h"
#include "spyro_state.h"
#include "emit_record.h"

/* Level27_AdvanceEmitList (0x80086754, level_27_dream_weavers_haunted_towers
 * overlay, 0x1a84 bytes).
 *
 * The emit-list ADVANCE pass -- the 1697-insn haunted-towers variant of the
 * walk matched in level_10 / level_15.  Walks the 0x20-byte record stream at
 * g_pEmitListBase and ages one record per iteration: integrates its position, fades
 * its colour, ticks its life counter and, when the record has expired (or its
 * alive byte has been cleared), hands it back with FreeEmitListRecord().  The walk
 * stops at the terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Field map, in record-relative offsets:
 *   0x00  effect id                0x01  class / terminator
 *   0x02  age                      0x03  alive
 *   0x04/0x06/0x08  tail point     0x0A/0x0C/0x0E  head point (also the
 *                                  r/g/b triple for the fading classes)
 *   0x10..0x16  per-record velocity / colour bytes
 *   0x18  actor id (class 0x1D) / spin angle / source pointer
 *   0x19  patrol node index        0x1A  waypoint index (class 0x1D)
 *   0x1C  wrap count               0x1E  phase toggle / flag bits
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

  if (CLASS == 0xFF) {
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
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_VEL_DIM:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_SPARK_RISE:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_ACTOR_PLUME:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFT_SPARKLE:  /* <- Level15_AdvanceEmitList */
    /* <- Level12_AdvanceEmitList */
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

    case EMIT_SPEED_DUST:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      AGE += step;
      if (AGE >= 0x20 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPYRO_ORBIT_SPARKLE:  /* <- Level10_AdvanceEmitList */
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == SPYRO_ST_HAZARD && g_nSparxHealth < 0) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        FreeEmitListRecord(rec);
      } else {
        CopyVector(v, SPYRO_POS_VIA_LOCKOUT);
        v[0] += LookupCosine(HS(0x18)) >> 4;
        v[1] += LookupSine(HS(0x18)) >> 4;
        v[2] += 0x100;
        VectorToSVectorShr2(POS, v);
        HS(0x18) = HU(0x18) + 0x40;
        rec[0xA] = abs(LookupCosine((HS(0x18) - g_nCameraEulerYaw) & 0xFFF)) / 170;
      }
      break;

    case 0xb:  /* <- Level10_AdvanceEmitList */
      AddSVector(POS, POS, &HS(0x12));
      rec[0xC] -= 8;
      rec[0xD] -= 8;
      rec[0xE] -= 8;
      if (rec[0xC] == 0 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_ACTOR_SPARK:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_SPARK_YELLOW:  /* <- Level5_AdvanceEmitList */
    {
      int c;

      if (AGE >= 0x1C || ALIVE == 0) {
        FreeEmitListRecord(rec);
      } else {
        AddSVector(POS, POS, VEL);
        if (HS(0x1E) != 0) {
          HU(0x1C)++;
        }
        HS(0x1E) = 1 - HU(0x1E);
        rec[0xA] += step2;
        if (AGE >= 0xD) {
          c = 0x80 - (AGE - 0xC) * 8;
          if (c < 0) {
            c = 0;
          }
          rec[0xC] = c;
          c = 0x40 - (AGE - 0xC) * 4;
          if (c < 0) {
            c = 0;
          }
          rec[0xD] = c;
          rec[0xE] = 0;
        } else if (AGE >= 5) {
          rec[0xC] = 0x80;
          rec[0xE] = 0;
          rec[0xD] = -0x80 - (AGE - 4) * 8;
        } else {
          rec[0xC] = 0x80;
          rec[0xD] = 0x80;
          c = 0x80 - AGE * 32;
          if (c < 0) {
            c = 0;
          }
          rec[0xE] = c;
        }
        AGE += step;
      }
      break;
    }

    case EMIT_FADING_SPARK:  /* <- Level18_AdvanceEmitList */
      /* Gnasty's cauldron drip: a slow ember that dies when its red
       * channel bottoms out. */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      rec[0xC] -= 4;
      rec[0xD] -= 4;
      rec[0xE] -= 4;
      if (rec[0xC] == 0 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPARK_RGB:  /* <- Level18_AdvanceEmitList */
      /* The same ember with a late red cut-off at age 9. */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
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

    case EMIT_RISING_SPIRAL:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_RESCUE_SWIRL:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_PATH_WISP: {
      int len;
      int spin;
      int rx;
      int ry;
      int jx;
      int jy;
      int *node;
      char *wp;
      char *actor;
      int idx;

      /* The haunted-towers ghost.  Three sub-states, selected by the flag
       * halfword at +0x1E: bit 3 = the free-flying trail, bit 0 = the spiral
       * orbit around its own drift point, and neither = the patrol run, which
       * walks a waypoint list hung off the actor named by +0x18 and, on
       * reaching the last waypoint, wakes the actor named by the node's +0x20
       * and re-arms itself as a trail.
       */
      SVectorToVectorShl2(pt, POS);
      EBB_BREAK();
      if (HU(0x1E) & 8) {
        if (AGE >= 0x18) {
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
        VectorToSVectorShr2(POS, pt);
        AGE++;
      } else if (HU(0x1E) & 1) {
        if (AGE >= 0x18) {
          AGE = 0;
          HS(0x18) = (GetRandomU32() & 6) - 3;
          HS(0x1A) = (GetRandomU32() & 6) - 3;
          HU(0x1C) = 0;
          HS(0x1E) = 8;
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
          SubtractVector(aim, aim, hold);
          len = VectorLength(aim, 1);
          if (len >= 0x81) {
            ScaleVector3ByRatio(aim, len, 0x80);
          }
          goto shove;
        }
      } else {
        idx = rec[0x19];
        actor = g_pActorListBase + rec[0x18] * ACTOR_SIZE;
        node = (int *)(idx * 4 + *(int *)actor);
        wp = (char *)node[1];
        actor = g_pActorListBase + node[8] * ACTOR_SIZE;
        SVectorToVectorShl2(hold, POS);
        CopyVector(aim, (int *)(wp + (rec[0x1A] * 0x10 + 8)));
        dv[0] += LookupCosine(AGE << 7) >> 2;
        dv[1] += LookupSine(AGE << 7) >> 2;
        dv[2] += LookupCosine((AGE << 7) + 0x200) >> 2;
        SubtractVector(aim, aim, hold);
        len = VectorLength(aim, 1);
        if (len < 0x401) {
          rec[0x1A]++;
          if (rec[0x1A] >= *(unsigned char *)wp) {
            ((Actor *)actor)->subMode = 1;
            HS(0x1E) = (GetRandomU32() & 6) + 1;
            VectorToSVectorShr2(VEL, &((Actor *)actor)->posX);
            spin = GetRandomU32() & 0x7F;
            AGE = 0;
            HU(0x1C) += spin;
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
      HU(0x8) += HU(0x1E);
      rec[0xA]++;
      rec[0xC] -= step2;
      rec[0xD] -= step2;
      rec[0xE] -= step2;
      AGE += step;
      if (AGE >= 0x20) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_SPREAD_MOTE:
      /* A plain drifter with a long life. */
      AddSVector(POS, POS, VEL);
      AGE += step;
      if (AGE >= 0x40 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case EMIT_DUST_PUFF:  /* <- Level10_AdvanceEmitList */
    {
      int c;

      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      c = rec[0xC] - step;
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
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

    case EMIT_ACTOR_WHITE_STREAK:  /* <- Level15_AdvanceEmitList */
    /* <- Level12_AdvanceEmitList */
    {
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

    case EMIT_TWINKLE:  /* <- Level10_AdvanceEmitList */
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
      if (AGE >= 0x18 || ALIVE == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case EMIT_STREAK_BURST:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_STREAK_BURST_WHITE:  /* <- Level10_AdvanceEmitList */
    {
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

    case 0x48:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_RISING_SPIRAL_SMEAR:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_DRIFT_STREAK:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_EMBER:  /* <- Level10_AdvanceEmitList */
    {
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

    case EMIT_FLAME_STREAK:  /* <- Level10_AdvanceEmitList */
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

    case EMIT_FALLING_STREAK_BURST:  /* <- Level10_AdvanceEmitList */
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
