#include "globals.h"

/* 0x80060E28 (176 bytes) — map a display RECT's left edge into the
   debug-overlay coordinate space.  Debug type 1 mirrors about 0x400 when the
   GPU reverse flag is set, type 2 does the same at half scale; everything else
   returns r->x.

   The shared `subu; j; subu` tail lives in case 1 and case 2 jumps BACKWARD
   into it, which only an explicit label + goto produces (jump.c's forward walk
   keeps the LATER copy).  The empty do/while pins case 2's `mirrored = 0x400`
   below the r->w / 2 chain so v0 is free for the accumulator (B23 used in
   reverse).  The return value is one `result` variable joined at `done`: as a
   global allocno it takes v0 itself, which leaves the r->x / 2 chain local with
   no v0 suggestion, so the sign-adjusted sum ties to the extended x in v1 as in
   the original. */
int FUN_80060e28(RECT *r) {
  int span;
  int left;
  int mirrored;
  int result;

  switch (g_bGpuDebugType) {
  case 1:
    if (g_bGpuReverseFlag == 0) {
      break;
    }
    span = r->w;
    left = r->x;
    mirrored = 0x400;
  mirror:
    mirrored -= span;
    mirrored -= left;
    return mirrored;
  case 2:
    if (g_bGpuReverseFlag != 0) {
      span = r->w / 2;
      left = r->x;
      do {
      } while (0);
      mirrored = 0x400;
      goto mirror;
    }
    result = r->x / 2;
    goto done;
  }
  result = r->x;
done:
  return result;
}
