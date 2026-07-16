/* func_level_17_8007AEDC (0x8007AEDC, level_17_magic_crafters_crystal_flight overlay).
 *
 * Flight-level end-of-run results handler. When `reload` is set, re-uploads
 * the 0x100x0xE1 VRAM strip at (0x200,0) from the level image base
 * (D_800785F0 - 0x1C200, func_8005FA28) and kicks func_8005F764(0). Resets
 * the gamestate (D_800757D8 = 0), reseeds the light vector (func_80058C7C),
 * then banks rewards: for each of the four collectible classes whose tally
 * (D_80078630..3C) hit 8, an unclaimed per-course flag
 * (D_80078680[course*5 + i]) pays the class bonus from the course reward
 * table (D_8006E920[course*5 + i]) into the level gem total
 * (D_80077420[D_80075964]) and running score (D_80075860), 12 digits
 * (D_800756C8) per payout. A perfect run (tallies sum to 0x20) pays the
 * fifth "all clear" bonus (D_8006E930[course*5], flag D_80078684) and
 * records a best time (D_80078618[course], lower is better, 0 = unset)
 * from the run clock D_800758F4. Finally spins the reward digit ring
 * (D_80077DB8, 32 slots, glyphs from D_8006E998[i % 12]) and, if the
 * pending-scene word D_80076E28 is 0x8000000E, latches the scene byte
 * D_8006C588[D_80078AD0] into D_80076E90.
 * Identical body in the five flight-level overlays.
 */

extern void func_8005FA28();
extern void func_8005F764();
extern void func_80058C7C(void);

extern char *D_800785F0;
extern int D_800757D8;
extern int D_8007595C;
extern int D_80075964;
extern int D_800756C8;
extern int D_80075860;
extern int D_800758F4;
extern int D_80076E28;
extern int D_80076E90;
extern int D_80077420[];
extern unsigned char D_80077DB8[];
extern int D_80078618[];
extern int D_80078630;
extern int D_80078634;
extern int D_80078638;
extern int D_8007863C;
extern unsigned char D_80078680[];
extern unsigned char D_80078684[];
extern int D_80078AD0;
extern int D_80078AD4;
extern int D_8006E920[];
extern int D_8006E930[];
extern unsigned char D_8006E998[];
extern unsigned char D_8006C588[];

void func_level_17_8007AEDC(int reload) {
  int i;

  if (reload != 0) {
    struct {
      short x, y, w, h;
    } rect;
    int pad[2];
    rect.x = 0x200;
    rect.w = 0x100;
    rect.y = 0;
    rect.h = 0xE1;
    func_8005FA28(&rect, D_800785F0 - 0x1C200);
    func_8005F764(0);
  }
  D_800757D8 = 0;
  func_80058C7C();
  for (i = 0; i < 4; i++) {
    if ((&D_80078630)[i] == 8) {
      int idx = D_8007595C * 5;
      int fbase = idx + (int)D_80078680;
      unsigned char *flag = (unsigned char *)fbase + i;
      if (*flag == 0) {
        int row;
        D_80077420[D_80075964] +=
            *(int *)((i << 2) + (row = (int)&D_8006E920[idx]));
        D_80075860 += *(int *)((i << 2) + row);
        D_800756C8 += 0xC;
        *flag = 1;
      }
    }
  }
  if (D_80078630 + D_80078634 + D_80078638 + D_8007863C == 0x20) {
    int idx = D_8007595C * 5;
    if (D_80078684[idx] == 0) {
      int off;
      D_80077420[D_80075964] +=
          *(int *)((char *)D_8006E930 + (off = idx << 2));
      D_80075860 += *(int *)((char *)D_8006E930 + off);
      D_800756C8 += 0xC;
      D_80078684[idx] = 1;
    }
    {
      int *best = &D_80078618[D_8007595C];
      if (*best == 0 || D_800758F4 < *best) {
        *best = D_800758F4;
      }
    }
  }
  i = 0;
  if (D_800756C8 > 0) {
    do {
      D_80077DB8[i & 0x1F] = D_8006E998[i % 12];
      i += 1;
    } while (i < D_800756C8);
  }
  D_80078AD4 = 0;
  if (D_80076E28 == 0x8000000E) {
    D_80076E90 = D_8006C588[D_80078AD0];
  }
}
