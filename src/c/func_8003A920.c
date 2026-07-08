#include "globals.h"

extern void ScaleVector3Sat();
extern void AddVector();
extern void RShiftVector3();

extern short D_80075280[];
extern short D_8006CBA4[];
extern short D_8006CBB4[];
extern short D_8006CBCC[];

/* 0x8003a920 — blend two vectors by a per-mode weight pair: pick the
   weight table from the record's mode byte (+2), scale vecA/vecB by the
   idx'th pair, sum into out, then >>10 to renormalize. */
void func_8003A920(int *out, unsigned char *rec, int idx, int *vecA,
                   int *vecB) {
  int va[4];
  int vb[4];
  short *tbl;
  short *entry;
  unsigned char sel;

  sel = rec[2];
  if (sel == 2) {
    tbl = D_80075280;
  } else if (sel == 4) {
    tbl = D_8006CBA4;
  } else {
    tbl = D_8006CBCC;
    if (sel == 6) {
      tbl = D_8006CBB4;
    }
  }

  entry = (short *)((idx << 2) + (int)tbl);
  ScaleVector3Sat(va, vecA, entry[0]);
  ScaleVector3Sat(vb, vecB, entry[1]);
  AddVector(out, va, vb);
  RShiftVector3(out, 0xA);
}
