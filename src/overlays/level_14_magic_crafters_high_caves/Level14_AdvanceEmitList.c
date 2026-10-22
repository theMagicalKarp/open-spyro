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

#define HS(n) (*(short *)(rec + (n)))
#define HU(n) (*(unsigned short *)(rec + (n)))
#define W(n) (*(int *)(rec + (n)))
#define POS ((short *)(rec + 4))
#define VEL ((short *)(rec + 0x18))

extern int abs(int n);
extern void VectorToSVectorShr2(short *dst, int *src);
extern void SVectorToVectorShl2(int *dst, short *src);
extern void AddSVector(short *dst, short *src, short *delta);
extern void FreeEmitListRecord(unsigned char *rec);

extern unsigned char *g_pEmitListBase;
extern short g_anSineLut[];        /* sine table */
extern short g_anCosineLut[];        /* cosine table */
extern int g_nFrameStep;            /* per-frame step count */
extern int g_nSpyroDrawSuppressed;
extern char *g_pActorListBase;
extern int g_anCameraPos[];          /* the camera world position */
extern short g_nCameraEulerYaw;
extern int g_nGemPickupTimer;
extern int g_nSpyroState;
extern int g_nSpyroInputLockoutCountdown[]; /* [-0x58] is the Spyro world position (g_anSpyroWorldPos) */
extern int g_nLevelReadyFlag;

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

  if (rec[1] == 0xFF) {
    return;
  }

  step2 = step * 2;
  step4 = step * 4;
  dir = dv;
  burst = 3;

  do {
    switch (rec[0]) {
    case 0x0:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x1:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step4;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      rec[0x2] += step;
      if (rec[0x2] >= 0x18 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x2:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step4;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x4:
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
      __asm__ volatile("" : : "r"(burst));
      SVectorToVectorShl2(pt, POS);
      if (HU(0x1E) & 8) {
        if (rec[0x2] >= 0x30) {
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
        if (rec[0x2] >= 0x30) {
          rec[0x2] = 0;
          HS(0x18) = (GetRandomU32() & 6) - 3;
          HS(0x1A) = (GetRandomU32() & 6) - 3;
          HU(0x1C) = 0;
          HS(0x1E) = 8;
          goto measure;
        } else {
          SVectorToVectorShl2(dv, VEL);
          if (HU(0x1E) & 2) {
            dv[0] += (rec[0x2] * LookupCosine(rec[0x2] << 7)) >> 7;
          } else {
            dv[0] -= (rec[0x2] * LookupCosine(rec[0x2] << 7)) >> 7;
          }
          if (HU(0x1E) & 4) {
            dv[1] += (rec[0x2] * LookupSine(rec[0x2] << 7)) >> 7;
          } else {
            dv[1] -= (rec[0x2] * LookupSine(rec[0x2] << 7)) >> 7;
          }
          dv[2] += rec[0x2] << 5;
          SubtractVector(dir, dir, pt);
          len = VectorLength(dir, 1);
          if (len >= 0x81) {
            ScaleVector3ByRatio(dir, len, 0x80);
          }
          AddVector(pt, pt, dir);
          goto commit;
        }
      } else {
        CopyVector(dir, (int *)(W(0x18) + 0xC));
        dv[0] += LookupCosine(rec[0x2] << 7) >> 2;
        dv[1] += LookupSine(rec[0x2] << 7) >> 2;
        dv[2] += LookupCosine((rec[0x2] << 7) + 0x200) >> 2;
        SubtractVector(dir, dir, pt);
        len = VectorLength(dir, 1);
        if (len < 0x401 || rec[0x2] >= 0x41) {
          int *owner = **(int ***)(rec + 0x18);

          if (owner[5] == 1) {
            owner[5] = 2;
            owner[0] = 0x20;
          }
          HS(0x1E) = (GetRandomU32() & 6) + 1;
          VectorToSVectorShr2(VEL, (int *)(W(0x18) + 0xC));
          spin = GetRandomU32() & 0x7F;
          rec[0x2] = 0;
          HU(0x1C) += spin;
          goto measure;
        }
        ScaleVector3ByRatio(dir, len, 0x100);
        AddVector(pt, pt, dir);
      }
    commit:
      __asm__ volatile("" : : "r"(burst));
      VectorToSVectorShr2(POS, pt);
      rec[0x2]++;
    measure:
      SubtractVector(dv, pt, g_anCameraPos);
      near = abs(dv[0]) + abs(dv[1]);
      len = near + abs(dv[2]);
      if (rec[0x1] == burst) {
        if (len >= 0x3001) {
          rec[0xC] = 0xFF;
          rec[0xD] = 0xFF;
          rec[0xE] = 0xFF;
          rec[0xA] = 4;
          rec[0x1] = 0;
          rec[0xF] = 0x40;
        }
      } else if (len < 0x2800) {
        rec[0x1] = burst;
        rec[0xC] = 0x80;
        rec[0xD] = 0x80;
        rec[0xE] = 0x80;
        rec[0x11] = 4;
        rec[0xF] = 0x2C;
        rec[0x10] = 0;
        rec[0xA] = (GetRandomU32() & 0xF) + 0x18;
        rec[0xB] = (GetRandomU32() & 0x7) + 0xF;
      }
      break;
    }

    case 0x5:
    {
      int rq;
      int rr;
      int jq;
      int jr;
      int dec;

      /* The short spawn drift: the record walks along its own jittered
       * velocity, plus a quarter of the record velocity vector. */
      if (rec[0x2] >= 0x30 || rec[0x3] == 0) {
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
        rec[0x2]++;
      }
      break;
    }

    case 0x8:
    {
      int len;
      int *home;
      int *bolt;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (g_pActorListBase + owner * 0x58), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      __asm__ volatile("" : : "r"(burst));
      SVectorToVectorShl2(p, POS);
      do {
      } while (0);
      if (rec[0x3] == 0) {
        goto expire;
      } else {
        bolt = v;
        CopyVector(bolt, (int *)(g_pActorListBase + rec[0x1E] * 0x58 + 0xC));
        v[0] += LookupCosine(rec[0x2] << 7) >> 1;
        v[1] += LookupSine(rec[0x2] << 7) >> 1;
        v[2] += LookupCosine((rec[0x2] << 7) - 0x100) >> 2;
        home = p;
        SubtractVector(bolt, bolt, home);
        len = VectorLength(bolt, 1);
        if (len < 0x401 || rec[0x2] >= 0x41) {
          goto expire;
        } else {
          ScaleVector3ByRatio(bolt, len, 0x80);
          AddVector(home, home, bolt);
          __asm__ volatile("" : : "r"(burst));
          VectorToSVectorShr2(POS, home);
          rec[0x2]++;
          home = (int *)g_anCameraPos;
          bolt = home;
          SubtractVector(v, p, g_anCameraPos);
          c = abs(v[0]) + abs(v[1]);
          len = c + abs(v[2]);
          if (rec[0x1] == burst) {
            if (len >= 0x3001) {
              rec[0xA] = 4;
              rec[0xF] = 0x40;
              rec[0x1] = 0;
              c = rec[0xC] * 2;
              rec[0xC] = c;
              rec[0xD] *= 2;
              rec[0xE] *= 2;
            }
          } else if (len < 0x2800) {
            rec[0x1] = burst;
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

    case 0x9:
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0xA:
    /* <- Level10_AdvanceEmitList */
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((g_nSpyroState == 0x1D && g_nLevelReadyFlag < 0) || g_nSpyroDrawSuppressed != 0 ||
          g_nSpyroInputLockoutCountdown[0] <= 0) {
        goto expire;
      } else {
        CopyVector(w, g_nSpyroInputLockoutCountdown - 0x58);
        w[0] += LookupCosine(HS(0x18)) >> 4;
        w[1] += LookupSine(HS(0x18)) >> 4;
        w[2] += 0x100;
        VectorToSVectorShr2(POS, w);
        HS(0x18) = HU(0x18) + 0x40;
        rec[0xA] = abs(LookupCosine((HS(0x18) - g_nCameraEulerYaw) & 0xFFF)) / 170;
      }
      break;

    case 0xB:
      AddSVector(POS, POS, (short *)(rec + 0x12));
      rec[0xC] -= 8;
      rec[0xD] -= 8;
      rec[0xE] -= 8;
      if (rec[0xC] == 0 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0xC:
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
      if ((W(0xC) & 0xFFFFFF) == 0 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x10:
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
      if (rec[0xC] == 0 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x11:
      /* The same ember with a late red cut-off at age 9. */
      AddSVector(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step2;
      if (rec[0x2] >= 9) {
        rec[0xC] -= 0x20;
      }
      rec[0xD] -= 8;
      rec[0xE] -= 4;
      rec[0x2]++;
      if (rec[0x2] >= 0xC || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x15:
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

    case 0x18:
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
      if (g_nGemPickupTimer < 0x40) {
        heat = g_nGemPickupTimer;
      }
      HS(0x18) = (rec[0x1D] * (0x48 - heat)) >> 3;
      HS(0x1A) = (((0x20 - rec[0x1D]) * heat) >> 4) + 8;
      HU(0x4) = HU(0x12) + ((g_anCosineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x6) = HU(0x14) + ((g_anSineLut[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x8) = HU(0x16) + 0x20 + HU(0x18);
      break;
    }

    case 0x1E:
      HU(0x8) += HU(0x1E);
      rec[0xA]++;
      rec[0xC] -= step2;
      rec[0xD] -= step2;
      rec[0xE] -= step2;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x1F:
      /* A plain drifter with a long life. */
      AddSVector(POS, POS, VEL);
      rec[0x2] += step;
      if (rec[0x2] >= 0x40 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;

    case 0x21:
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
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x42:
    {
      int i;
      int j;
      int r;

      for (i = 0; i < step; i++) {
        if (rec[0x2] & 1) {
          HU(0x4) += HU(0x10);
          HU(0x6) += HU(0x12);
          HU(0x8) += HU(0x14);
          if (rec[0x2] & 2) {
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
        rec[0x2]++;
      }
      if (rec[0x2] >= 0x18 || rec[0x3] == 0) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x45:
    {
      unsigned char *path;
      int *base;
      int *hop;
      int len;

      path = (unsigned char *)W(0x18);
      base = cur;
      SVectorToVectorShl2(base, POS);
      hop = adv;
      CopyVector(hop, (int *)&path[W(0x1C) * 0x10 + 8]);
      adv[0] += LookupCosine(rec[0x2] << 7) >> 2;
      adv[1] += LookupSine(rec[0x2] << 7) >> 2;
      adv[2] += LookupCosine((rec[0x2] << 7) + 0x200) >> 2;
      SubtractVector(hop, hop, base);
      len = VectorLength(hop, 1);
      if (len < 0x401) {
        W(0x1C)++;
        if (W(0x1C) >= path[0]) {
          FreeEmitListRecord(rec);
          break;
        }
      }
      base = adv;
      ScaleVector3ByRatio(base, len, 0x100);
      hop = cur;
      AddVector(hop, hop, base);
      VectorToSVectorShr2(POS, hop);
      rec[0x2]++;
      break;
    }

    case 0x46:
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
      if (rec[0x2] & 1) {
        d[2]--;
      }
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      rec[0x2]++;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        goto expire;
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

    case 0x47:
    {
      int d[3];
      int j;
      int r;

      rec[0x2]++;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
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
        rec[0x14] -= 2;
        rec[0x15] -= 2;
        rec[0x16] -= 4;
        rec[0x10] -= 3;
        rec[0x11] -= 3;
        rec[0x12] -= 3;
      }
      break;
    }

    case 0x48:
    {
      int d[3];
      int j;
      int r;

      rec[0x2]++;
      if (rec[0x2] >= 0x30 || rec[0x3] == 0) {
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

    case 0x49:
      if (rec[0x3] == 0 || rec[0x2] >= 0x21) {
        FreeEmitListRecord(rec);
        break;
      }
      HS(0x4) = (rec[0x1F] * LookupCosine((rec[0x1E] + rec[0x2]) << 6)) >> 10;
      HS(0x6) = (rec[0x1F] * LookupSine((rec[0x1E] + rec[0x2]) << 6)) >> 10;
      HS(0x8) = rec[0x2] * 2;
      rec[0x2]++;
      HU(0x4) += HU(0x18);
      HU(0x6) += HU(0x1A);
      HU(0x8) += HU(0x1C);
      break;

    case 0x4C:
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

    case 0x4D:
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
      rec[0x2]++;
      if (rec[0x2] >= 0x20) {
        FreeEmitListRecord(rec);
      }
      break;
    }

    case 0x4E:
    {
      int j;
      int k;
      int r;

      rec[0x2]++;
      if (rec[0x2] >= 0x28 || rec[0x3] == 0) {
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
        rec[0xC] -= 2;
        rec[0xD] -= 2;
        rec[0xE] -= 1;
      }
      break;
    }

    case 0x4F:
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
      rec[0x2]++;
      if (rec[0x2] >= 0x18 || rec[0x3] == 0) {
        goto expire;
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

    case 0x50:
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
      rec[0x2]++;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
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
  } while (rec[1] != 0xFF);
}
