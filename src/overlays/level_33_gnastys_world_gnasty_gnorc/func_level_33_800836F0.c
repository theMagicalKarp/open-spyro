/* func_level_33_800836F0 (0x800836F0, level_33_gnastys_world_gnasty_gnorc
 * overlay, 0xf44 bytes).
 *
 * The emit-list ADVANCE pass -- the sibling of the emit dispatcher
 * func_level_33_80082B84.  Walks the 0x20-byte record stream at D_80075824
 * and ages one record per iteration: integrates its position, fades its
 * colour, ticks its life counter and, when the record has expired (or its
 * alive byte has been cleared), hands it back with func_80053608().  The
 * walk stops at the terminator record, whose class byte at +0x01 is 0xFF.
 *
 * Only `rec` is a real induction variable.  `pos` and `vel` are separate
 * pointers because the arms pass them to func_80017C84() as values, so
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
 *   0x18  spin angle               0x1C  wrap count    0x1E  phase toggle
 *
 * The Gnasty's-World 977-insn variant.  It drops every flight class
 * (0xD/0x13/0x1A/0x1B) and the ground swirls (0x6/0x15/0x18/0x4C/0x50), and
 * adds the three boss-arena classes 0x10, 0x11 and 0x19 -- the same trio the
 * level_33 emit dispatcher pushes.  The other fifteen arms transfer verbatim
 * from the matched level_5 (894) / level_11 (790) copies and the level_2
 * ground decode.
 *
 * Verified byte-identical inside the relinked
 * level_33_gnastys_world_gnasty_gnorc.ovl.
 */

#define HS(n) (*(short *)(rec + (n)))
#define HU(n) (*(unsigned short *)(rec + (n)))
#define W(n) (*(int *)(rec + (n)))
#define POS ((short *)(rec + 4))
#define VEL ((short *)(rec + 0x18))

extern void func_80017C84(short *dst, short *src, short *delta);
extern void func_80017BFC(short *dst, int *src);
extern void func_80017700(int *dst, int *src); /* CopyVector */
extern int func_80016C58(int a);               /* LookupSine */
extern int func_80016CB0(int a);               /* LookupCosine */
extern unsigned int func_8006272C(void);       /* GetRandomU32 */
extern int abs(int n);
extern void func_80053608(unsigned char *rec); /* FreeEmitListRecord */

extern unsigned char *D_80075824; /* g_pEmitListBase */
extern int D_80075814;            /* g_nSpyroDrawSuppressed */
extern short D_80076E20;
extern int D_80078AD0;
extern int D_80078BB8[]; /* [-0x58] is the Spyro world position (D_80078A58) */
extern int D_80078BBC;

void func_level_33_800836F0(int step) {
  unsigned char *rec = D_80075824;
  int v[3];

  if (rec[1] == 0xFF) {
    return;
  }

  do {
    switch (rec[0]) {
    case 0x0:
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0xC] -= step * 4;
      rec[0xD] -= step * 4;
      rec[0xE] -= step * 4;
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
      rec[0xA] += step * 4;
      rec[0xC] -= step * 4;
      rec[0xD] -= step * 4;
      rec[0xE] -= step * 4;
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
      rec[0xA] += step * 4;
      rec[0xC] -= step * 4;
      rec[0xD] -= step * 4;
      rec[0xE] -= step * 4;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20 || rec[0x3] == 0) {
        func_80053608(rec);
      }
      break;

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
        func_80017700(v, D_80078BB8 - 0x58);
        v[0] += func_80016CB0(HS(0x18)) >> 4;
        v[1] += func_80016C58(HS(0x18)) >> 4;
        v[2] += 0x100;
        func_80017BFC(POS, v);
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

    case 0x19:
      /* The plain aged spark: no colour ramp, no alive check. */
      func_80017C84(POS, POS, VEL);
      if (HS(0x1E) != 0) {
        HU(0x1C)++;
      }
      HS(0x1E) = 1 - HU(0x1E);
      rec[0xA] += step * 2;
      rec[0x2] += step;
      if (rec[0x2] >= 0x20) {
        func_80053608(rec);
      }
      break;

    case 0x21: {
      int c;

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

    default:
      break;
    }

    rec += 0x20;
  } while (rec[1] != 0xFF);
}
