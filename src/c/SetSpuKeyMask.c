#include "globals.h"

/* Key-on/key-off the 24 SPU voices in `m` (0x8005c7d4, 0x1FC). op 1 keys
   on, op 0 keys off; anything else is ignored. In deferred mode
   (D_8007354C bit 0) the request lands in the pending key-on
   (D_800777A8/AA) or key-off (D_800777AC) mask pair — cancelling any
   opposite-side pending bits — and flags the per-frame flush; otherwise
   it hits the SPU key-on/key-off registers directly and updates the live
   key-on mask shadow. */
extern int D_8007354C;
extern char *D_80073554;            /* SPU register block base */
extern unsigned short D_800777A8[]; /* pending key-on mask (lo, hi) */
extern volatile unsigned short D_800777AA;
extern volatile unsigned short D_800777AC; /* pending key-off mask lo */
/* key-off mask hi halfword (D_800777AC + 2, no seeded symbol) */
#define D_800777AE (*((volatile unsigned short *)&D_800777AC + 1))

#define SPUREG(off) (*(volatile unsigned short *)(D_80073554 + (off)))
/* the deferred-op block is ordering-sensitive: every access volatile */
#define VFLUSH (*(volatile unsigned int *)&g_dwSpuDeferredFlushPending)
#define VKEYON (*(volatile unsigned int *)&g_dwSpuDeferredKeyOnMask)

void SetSpuKeyMask(int op, unsigned int m) {
  unsigned int mask;
  unsigned int hi;
  unsigned short m16;
  unsigned short h16;
  unsigned int inv;

  mask = m & 0xFFFFFF;
  m16 = mask;
  hi = mask >> 16;
  h16 = hi;
  if (op != 0) {
    if (op == 1) {
      if (D_8007354C & 1) {
        volatile unsigned short *pend = (volatile unsigned short *)D_800777A8;
        pend[0] = m16;
        D_800777AA = h16;
        VFLUSH = VFLUSH | 1;
        VKEYON = VKEYON | mask;
        if (D_800777AC & mask) {
          D_800777AC = D_800777AC & ~mask;
        }
        if (D_800777AE & hi) {
          D_800777AE = D_800777AE & ~hi;
        }
      } else {
        unsigned int live = g_dwSpuLiveKeyOnMask | mask;
        SPUREG(0x188) = m16;
        SPUREG(0x18A) = h16;
        g_dwSpuLiveKeyOnMask = live;
      }
    }
  } else {
    if (D_8007354C & 1) {
      volatile unsigned short *pend;
      D_800777AC = m16;
      D_800777AE = h16;
      inv = ~mask;
      VFLUSH = VFLUSH | 1;
      VKEYON = VKEYON & inv;
      pend = (volatile unsigned short *)D_800777A8;
      if (pend[0] & mask) {
        pend[0] = pend[0] & inv;
      }
      if (D_800777AA & hi) {
        D_800777AA = D_800777AA & ~hi;
      }
    } else {
      unsigned int live;
      SPUREG(0x18C) = m16;
      SPUREG(0x18E) = h16;
      live = g_dwSpuLiveKeyOnMask & ~mask;
      g_dwSpuLiveKeyOnMask = live;
    }
  }
}
