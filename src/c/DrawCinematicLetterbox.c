#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"

/* Draw two stacked black POLY_F4 cinematic bars (top y=0..h, bottom
   y=240-h..240) where h = g_nLetterboxBarHeight animates 0..0x16 driven by
   g_nGameplayBlocked — the "letterbox close"/"open" cutscene transition.
   Submits both bars via AddPrimToOT (0x80018f30, 0x1a4). */
void DrawCinematicLetterbox(void) {
  PolyF4 *q;
  int h;
  int h2;
  int tag;
  int code;
  if (g_nGameplayBlocked != 0) {
    if (g_nLetterboxBarHeight < 0x16) {
      g_nLetterboxBarHeight += g_nFrameStep;
    }
    if (g_nLetterboxBarHeight > 0x16) {
      g_nLetterboxBarHeight = 0x16;
    }
    tag = 0x5000000;
    code = 0x28;
  } else {
    if (g_nLetterboxBarHeight > 0) {
      g_nLetterboxBarHeight -= g_nFrameStep;
    }
    if (g_nLetterboxBarHeight < 0) {
      g_nLetterboxBarHeight = 0;
    }
    tag = 0x5000000;
    code = 0x28;
  }
  q = g_pPrimBufferWriteCursor;
  q->code = code;
  h = g_nLetterboxBarHeight;
  q->tag = tag;
  q->x0 = 0;
  q->x1 = 0x200;
  q->x2 = 0;
  q->x3 = 0x200;
  q->y0 = 0;
  q->y1 = 0;
  q->r0 = 0;
  q->g0 = 0;
  q->b0 = 0;
  q->y2 = h;
  q->y3 = h;
  AddPrimToOT((int)q);
  q++;
  g_pPrimBufferWriteCursor = q;
  q->code = code;
  h2 = g_nLetterboxBarHeight;
  q->y2 = 0xf0;
  q->y3 = 0xf0;
  q->tag = tag;
  q->x0 = 0;
  q->x1 = 0x200;
  q->x2 = 0;
  q->x3 = 0x200;
  q->r0 = 0;
  q->g0 = 0;
  q->b0 = 0;
  q->y0 = 0xf0 - h2;
  q->y1 = 0xf0 - h2;
  AddPrimToOT((int)q);
  q++;
  g_pPrimBufferWriteCursor = q;
}

/* MATCHED 2026-08-14.

   The last 2-insn residue was a coupled pair: the two shared header constants
   (0x5000000, 0x28) are DEFINED in the clamp-exit blocks in first-use order,
   while the two header STORES emit in source order — and the original needs
   those two orders opposite ([tag, code] defs, [code, tag] stores). Writing the
   constants as literals makes both orders one source decision, so either the
   stores or the constants came out reversed.

   The decoupling lever: give each constant a local that is ASSIGNED IN EVERY
   ARM of the preceding if/else. Two reaching definitions means cse cannot
   const-propagate the value to its use and re-create the def there (which is
   what defeated the earlier single-assignment `code`/`cb` locals), so the def
   stays where the source puts it — at the end of the clamp arms, in written
   order — and jump.c/reorg then duplicate it into the clamp-exit delay slots
   exactly as the original does. The store order is then free to be written
   independently.

   Also load-bearing, from the earlier passes:
     * `h` must be TWO locals, one per bar. The original reads the bar height
       into v0 in the top-bar block and v1 in the bottom one, which one shared
       multi-set local can never produce (one pseudo = one quantity); with a
       local per block each read is single-set and each gets its own register.
     * the top bar's tag store and the 0x28 store are separated by the height
       load, and a load cannot cross either store (both are MEMs off the same
       base, no alias info), so the two stores land in slots 1 and 4 of the
       block in SOURCE order. */
