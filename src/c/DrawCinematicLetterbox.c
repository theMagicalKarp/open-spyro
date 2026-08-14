#include "globals.h"

extern void AddPrimToOT(int prim);

/* Draw two stacked black POLY_F4 cinematic bars (top y=0..h, bottom
   y=240-h..240) where h = g_nLetterboxBarHeight animates 0..0x16 driven by
   g_nGameplayBlocked — the "letterbox close"/"open" cutscene transition.
   Submits both bars via AddPrimToOT (0x80018f30, 0x1a4). */
void DrawCinematicLetterbox(void) {
  void *prim;
  int h;
  int h2;
  int tag;
  int pad;
  if (g_nGameplayBlocked != 0) {
    if (g_nLetterboxBarHeight < 0x16) {
      g_nLetterboxBarHeight += g_nFrameStep;
    }
    if (g_nLetterboxBarHeight > 0x16) {
      g_nLetterboxBarHeight = 0x16;
    }
    tag = 0x5000000;
    pad = 0x28;
  } else {
    if (g_nLetterboxBarHeight > 0) {
      g_nLetterboxBarHeight -= g_nFrameStep;
    }
    if (g_nLetterboxBarHeight < 0) {
      g_nLetterboxBarHeight = 0;
    }
    tag = 0x5000000;
    pad = 0x28;
  }
  prim = g_pPrimBufferWriteCursor;
  *((unsigned char *)(((int)prim) + 7)) = pad;
  h = g_nLetterboxBarHeight;
  *((int *)prim) = tag;
  *((short *)(((int)prim) + 8)) = 0;
  *((short *)(((int)prim) + 0xc)) = 0x200;
  *((short *)(((int)prim) + 0x10)) = 0;
  *((short *)(((int)prim) + 0x14)) = 0x200;
  *((short *)(((int)prim) + 0xa)) = 0;
  *((short *)(((int)prim) + 0xe)) = 0;
  *((unsigned char *)(((int)prim) + 4)) = 0;
  *((unsigned char *)(((int)prim) + 5)) = 0;
  *((unsigned char *)(((int)prim) + 6)) = 0;
  *((short *)(((int)prim) + 0x12)) = h;
  *((short *)(((int)prim) + 0x16)) = h;
  AddPrimToOT((int)prim);
  prim = (void *)(((int)prim) + 0x18);
  g_pPrimBufferWriteCursor = prim;
  *((unsigned char *)(((int)prim) + 7)) = pad;
  h2 = g_nLetterboxBarHeight;
  *((short *)(((int)prim) + 0x12)) = 0xf0;
  *((short *)(((int)prim) + 0x16)) = 0xf0;
  *((int *)prim) = tag;
  *((short *)(((int)prim) + 8)) = 0;
  *((short *)(((int)prim) + 0xc)) = 0x200;
  *((short *)(((int)prim) + 0x10)) = 0;
  *((short *)(((int)prim) + 0x14)) = 0x200;
  *((unsigned char *)(((int)prim) + 4)) = 0;
  *((unsigned char *)(((int)prim) + 5)) = 0;
  *((unsigned char *)(((int)prim) + 6)) = 0;
  *((short *)(((int)prim) + 0xa)) = 0xf0 - h2;
  *((short *)(((int)prim) + 0xe)) = 0xf0 - h2;
  AddPrimToOT((int)prim);
  prim = (void *)(((int)prim) + 0x18);
  g_pPrimBufferWriteCursor = prim;
}

/* MATCHED 2026-08-14.

   The last 2-insn residue was a coupled pair: the two shared header constants
   (0x5000000, 0x28) are DEFINED in the clamp-exit blocks in first-use order,
   while the two header STORES emit in source order — and the original needs
   those two orders opposite ([tag, pad] defs, [pad, tag] stores). Writing the
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
