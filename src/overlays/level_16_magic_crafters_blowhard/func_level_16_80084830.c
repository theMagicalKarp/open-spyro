/* func_level_16_80084830 (0x80084830, level_16_magic_crafters_blowhard
 * overlay, 0x1710 bytes).
 *
 * The emit-list ADVANCE pass -- the 1476-insn Blowhard variant, 25 classes.
 * Walks the 0x20-byte record stream at D_80075824 and ages one record per
 * iteration: integrates its position, fades its colour, ticks its life
 * counter and, when the record has expired (or its alive byte has been
 * cleared), hands it back with func_80053608().  The walk stops at the
 * terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Head and arms 0x7/0x8/0x41 come from func_level_12_8008BE98, the ground
 * classes from level_2, 0x10/0x11 from level_33, the rest are shared.
 *
 * Three per-level decisions this variant needed:
 *  - FOUR stack vectors, not three.  Arm 0x6 gets its own (frame 0x10) and
 *    arm 0x8's position vector is a separate one (0x20); `dir` still names
 *    &v (0x30) and arm 0xA has 0x40.  Declaration order IS frame order, so
 *    the split has to be declared in that sequence or every sp-relative
 *    offset in the function shifts by 16.
 *  - An A200 cse barrier (`do { } while (0);`) between arm 0x8's first
 *    func_80017C24(p2, POS) and the direction-vector group.  Without it the
 *    whole arm shares ONE address pseudo: gcc either hoists it to the loop
 *    preheader and spills it or holds it in s0 from the first use, where the
 *    original rematerialises `addiu a0,sp,0x20` there and only takes s0 for
 *    the middle four uses.
 *  - Arm 0x8's `hold` carrier is MULTI-SET on purpose (A214).  The middle
 *    group reads p2 through it and the last statement reassigns it to
 *    D_80076DF8 -- the very argument the following call needs in a2 -- so
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
#define HS(n) (*(short *)(rec + (n)))
#define HU(n) (*(unsigned short *)(rec + (n)))
#define W(n) (*(int *)(rec + (n)))
#define POS ((short *)(rec + 4))
#define VEL ((short *)(rec + 0x18))

extern void func_80017C84(short *dst, short *src, short *delta);
extern void func_80017C24(int *dst, short *src);
extern void func_80017BFC(short *dst, int *src);
extern void func_80017758(int *dst, int *a, int *b); /* AddVector */
extern void func_8001778C(int *dst, int *a, int *b); /* SubVector */
extern void func_80017700(int *dst, int *src);       /* CopyVector */
extern int func_800171FC(int *v, int mode);          /* VectorLength */
extern void func_800175B8(int *v, int len, int scale);
extern int func_80016C58(int a);         /* LookupSine */
extern int func_80016CB0(int a);         /* LookupCosine */
extern unsigned int func_8006272C(void); /* GetRandomU32 */
extern int abs(int n);
extern void func_80053608(unsigned char *rec); /* FreeEmitListRecord */

extern unsigned char *D_80075824; /* g_pEmitListBase */
extern char *D_80075828;          /* g_pActorListBase */
extern int D_80075814;            /* g_nSpyroDrawSuppressed */
extern int D_800756CC;            /* per-frame step count */
extern short D_8006CBF8[];        /* sine table */
extern short D_8006CC78[];        /* cosine table */
extern short D_80076E20;
extern int D_80076DF8[];
extern int D_8007706C;
extern int D_80078AD0;
extern int D_80078BB8[]; /* [-0x58] is the Spyro world position (D_80078A58) */
extern int D_80078BBC;

void func_level_16_80084830(int step) {
  unsigned char *rec = D_80075824;
  int p[3];
  int p2[3];
  int v[3];
  int w[3];
  int c;
  int d;
  int *dir;
  int step4;

  if (rec[1] == 0xFF) {
    return;
  }

  step4 = step * 4;
  dir = v;

  do {
    switch (rec[0]) {
    case 0x0:
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0xC] -= step4;
      rec[0xD] -= step4;
      rec[0xE] -= step4;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;

    case 0x1:
      func_80017C84(POS, POS, VEL);
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
        func_80053608(rec);
      }
      break;

    case 0x2:
      func_80017C84(POS, POS, VEL);
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
        func_80053608(rec);
      }
      break;

    case 0x6: {
      int ang;
      int amp;
      int z;

      func_80017700(p, *(int **)(rec + 0x18));
      z = p[2] + HS(0x1E) * 0x40;
      amp = rec[0x2] * 0x40;
      ang = amp;
      if (HS(0x1C) < amp) {
        amp = HS(0x1C);
      }
      p[0] += (amp * func_80016CB0(ang)) >> 12;
      p[1] += (amp * func_80016C58(ang)) >> 12;
      p[2] = HS(0x8) * 4 + 0x40;
      if (z < p[2] || rec[0x2] >= 0xC9) {
        func_80053608(rec);
      } else {
        func_80017BFC(POS, p);
        rec[0x2]++;
      }
      break;
    }

    case 0x7:
      /* The rune orbit: it rises and falls on a triangular ramp keyed to the
       * age byte and drifts by its own per-axis velocity words. */
      if (rec[0x2] >= 0x19 || rec[0x3] == 0) {
        func_80053608(rec);
      } else {
        rec[0xB] += rec[0x1E];
        if (rec[0x2] >= 0xD) {
          rec[0xA] = (0x18 - rec[0x2]) * 4;
        } else {
          rec[0xA] = rec[0x2] * 4;
        }
        HU(0x4) += HU(0x18);
        HU(0x6) += HU(0x1A);
        HU(0x8) += HU(0x1C);
        rec[0x2]++;
      }
      break;

    case 0x8: {
      int len;
      int *hold;

      /* The wizard's homing bolt: it spirals around the line to its owner
       * actor (D_80075828 + owner * 0x58), and switches to its burst state
       * (class byte 3) once it closes inside 0x2800. */
      func_80017C24(p2, POS);
      if (rec[0x3] == 0) {
        func_80053608(rec);
      } else {
        func_80017700(dir, (int *)(D_80075828 + rec[0x1E] * 0x58 + 0xC));
        do {
        } while (0);
        v[0] += func_80016CB0(rec[0x2] << 7) >> 1;
        v[1] += func_80016C58(rec[0x2] << 7) >> 1;
        v[2] += func_80016CB0((rec[0x2] << 7) - 0x100) >> 2;
        hold = p2;
        func_8001778C(dir, dir, hold);
        len = func_800171FC(dir, 1);
        if (len < 0x401 || rec[0x2] >= 0x41) {
          func_80053608(rec);
        } else {
          func_800175B8(dir, len, 0x80);
          func_80017758(hold, hold, dir);
          func_80017BFC(POS, hold);
          rec[0x2]++;
          hold = D_80076DF8;
          func_8001778C(v, p2, hold);
          c = abs(v[0]) + abs(v[1]);
          len = c + abs(v[2]);
          if (rec[0x1] == 3) {
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
            rec[0x1] = 3;
            rec[0x11] = 4;
            rec[0xF] = 0x2C;
            rec[0x10] = 0;
            c = rec[0xC] >> 1;
            rec[0xC] = c;
            rec[0xD] >>= 1;
            rec[0xE] >>= 1;
            rec[0xA] = (func_8006272C() & 0xF) + 0x18;
            rec[0xB] = (func_8006272C() & 0x7) + 0xF;
          }
        }
      }
      break;
    }

    case 0x9:
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;

    case 0xA:
      /* The homing sparkle: it only lives while Spyro is on screen. */
      if ((D_80078AD0 == 0x1D && D_80078BBC < 0) || D_80075814 != 0 ||
          D_80078BB8[0] <= 0) {
        func_80053608(rec);
      } else {
        func_80017700(w, D_80078BB8 - 0x58);
        w[0] += func_80016CB0(HS(0x18)) >> 4;
        w[1] += func_80016C58(HS(0x18)) >> 4;
        w[2] += 0x100;
        func_80017BFC(POS, w);
        HS(0x18) = HU(0x18) + 0x40;
        rec[0xA] = abs(func_80016CB0((HS(0x18) - D_80076E20) & 0xFFF)) / 170;
      }
      break;

    case 0xB:
      func_80017C84(POS, POS, (short *)(rec + 0x12));
      rec[0xC] -= 8;
      rec[0xD] -= 8;
      rec[0xE] -= 8;
      if (rec[0xC] == 0 || rec[0x3] == 0) {
        func_80053608(rec);
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
        func_80053608(rec);
      }
      break;

    case 0x10:
      /* Gnasty's cauldron drip: a slow ember that dies when its red
       * channel bottoms out. */
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0xC] -= 4;
      rec[0xD] -= 4;
      rec[0xE] -= 4;
      if (rec[0xC] == 0 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;

    case 0x11:
      /* The same ember with a late red cut-off at age 9. */
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      if (rec[0x2] >= 9) {
        rec[0xC] -= 0x20;
      }
      rec[0xD] -= 8;
      rec[0xE] -= 4;
      rec[0x2]++;
      if (rec[0x2] >= 0xC || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;

    case 0x15: {
      int s;

      rec[0x18] += D_800756CC * 4;
      rec[0x19] += D_800756CC;
      HU(0x4) = HU(0x12) + ((D_8006CC78[rec[0x18]] * rec[0x19]) >> 12);
      s = D_8006CBF8[rec[0x18]] * rec[0x19];
      HU(0x8) += *(const int *)&D_800756CC * 6;
      rec[0xB] += 4;
      HU(0x6) = HU(0x14) + (s >> 12);
      if (rec[0x19] >= 0xC1) {
        func_80053608(rec);
      }
      break;
    }

    case 0x18: {
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
      rec[0x1C] += D_800756CC * 8;
      heat = 0x3F;
      if (D_8007706C < 0x40) {
        heat = D_8007706C;
      }
      HS(0x18) = (rec[0x1D] * (0x48 - heat)) >> 3;
      HS(0x1A) = (((0x20 - rec[0x1D]) * heat) >> 4) + 8;
      HU(0x4) = HU(0x12) + ((D_8006CC78[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x6) = HU(0x14) + ((D_8006CBF8[rec[0x1C]] * HS(0x1A)) >> 12);
      HU(0x8) = HU(0x16) + 0x20 + HU(0x18);
      break;
    }

    case 0x21: {
      func_80017C84(POS, POS, VEL);
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
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;
    }

    case 0x41: {
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
      r = func_8006272C();
      j = d[0] - 1;
      j += r & 2;
      d[0] = j;
      r = func_8006272C();
      j = d[1] - 1;
      j += r & 2;
      d[1] = j;
      HU(0xA) += d[0];
      HU(0xC) += d[1];
      HU(0xE) += d[2];
      rec[0x2]++;
      if (rec[0x2] >= 0x8 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;
    }

    case 0x42: {
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
            r = func_8006272C();
            j = HU(0x10) - 1;
            j += r & 2;
            HU(0x10) = j;
            r = func_8006272C();
            j = HU(0x12) - 1;
            j += r & 2;
            HU(0x12) = j;
            r = func_8006272C();
            j = HU(0x14) - 1;
            j += r & 2;
            HU(0x14) = j;
          }
        }
        rec[0x2]++;
      }
      if (rec[0x2] >= 0x18 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;
    }

    case 0x46: {
      unsigned short d[3];
      int j;
      int r;

      d[0] = (HS(0xA) - HS(0x4)) >> 1;
      d[1] = (HS(0xC) - HS(0x6)) >> 1;
      d[2] = (HS(0xE) - HS(0x8)) >> 1;
      HU(0x4) += d[0];
      HU(0x6) += d[1];
      HU(0x8) += d[2];
      r = func_8006272C();
      j = d[0] - 1;
      j += r & 2;
      d[0] = j;
      r = func_8006272C();
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
        func_80053608(rec);
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

    case 0x47: {
      int d[3];
      int j;
      int r;

      rec[0x2]++;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        func_80053608(rec);
      } else {
        d[0] = HS(0xA) - HS(0x4);
        d[1] = HS(0xC) - HS(0x6);
        d[2] = HS(0xE) - HS(0x8);
        r = func_8006272C();
        j = d[0] - 1;
        j += r & 2;
        d[0] = j;
        r = func_8006272C();
        j = d[1] - 1;
        j += r & 2;
        d[1] = j;
        r = func_8006272C();
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

      rec[0x2]++;
      if (rec[0x2] >= 0x30 || rec[0x3] == 0) {
        func_80053608(rec);
      } else {
        d[0] = (HS(0x4) - HS(0xA)) >> 1;
        d[1] = (HS(0x6) - HS(0xC)) >> 1;
        d[2] = (HS(0x8) - HS(0xE)) >> 1;
        r = func_8006272C();
        j = d[0] - 2;
        j += r & 4;
        d[0] = j;
        r = func_8006272C();
        j = d[1] - 2;
        j += r & 4;
        d[1] = j;
        r = func_8006272C();
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

    case 0x4C: {
      int s;

      rec[0x18] += D_800756CC * 4;
      rec[0x19] += D_800756CC;
      HU(0x4) = HU(0x10) + ((D_8006CC78[rec[0x18]] * rec[0x19]) >> 12);
      s = D_8006CBF8[rec[0x18]] * rec[0x19];
      HU(0x8) += D_800756CC * 6;
      HU(0x6) = HU(0x12) + (s >> 12);
      if (rec[0x19] >= 0xC1) {
        func_80053608(rec);
      }
      break;
    }

    case 0x4D: {
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
        func_80053608(rec);
      }
      break;
    }

    case 0x4E: {
      int j;
      int k;
      int r;

      rec[0x2]++;
      if (rec[0x2] >= 0x28 || rec[0x3] == 0) {
        func_80053608(rec);
      } else {
        HU(0x4) += HU(0x10);
        HU(0x6) += HU(0x12);
        HU(0x8) += HU(0x14);
        r = func_8006272C();
        j = HU(0x10) - 1;
        j += r & 2;
        HU(0x10) = j;
        r = func_8006272C();
        j = HU(0x12) - 1;
        j += r & 2;
        HU(0x12) = j;
        r = func_8006272C();
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

    case 0x4F: {
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
        func_80053608(rec);
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

    case 0x50: {
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
        func_80053608(rec);
      }
      break;
    }

    default:
      break;
    }

    rec += 0x20;
  } while (rec[1] != 0xFF);
}
