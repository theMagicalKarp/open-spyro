#include "globals.h"
#include "main_shared.h"

extern void AddVector();
extern void RShiftVector3();

/* 0x8003a920 — blend two vectors by a per-mode weight pair: pick the
   weight table from the record's mode byte (+2), scale vecA/vecB by the
   idx'th pair, sum into out, then >>10 to renormalize. */
void BlendVectorPairByMode(int *out, unsigned char *rec, int idx, int *vecA,
                           int *vecB) {
  int va[4];
  int vb[4];
  short *tbl;
  short *entry;
  unsigned char sel;

  sel = rec[2];
  if (sel == 2) {
    tbl = g_anTweenBlendWeights2;
  } else if (sel == 4) {
    tbl = g_anTweenBlendWeights4;
  } else {
    tbl = g_anTweenBlendWeights8;
    if (sel == 6) {
      tbl = g_anTweenBlendWeights6;
    }
  }

  entry = (short *)((idx << 2) + (int)tbl);
  ScaleVector3Sat(va, vecA, entry[0]);
  ScaleVector3Sat(vb, vecB, entry[1]);
  AddVector(out, va, vb);
  RShiftVector3(out, 0xA);
}
