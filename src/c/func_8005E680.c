#include "globals.h"

extern int WritePrintf(char *fmt, ...);
extern int D_80074A0C;    /* DMA channel register block base (0x1F801080) */
extern char D_80011678[]; /* bus-error dump format */
extern char D_80011694[]; /* per-channel dump format */

typedef void (*DmaIrqCallback)(volatile unsigned int *);

/* DMA interrupt dispatcher (libgpu DMA IRQ path). Scans the DICR register's
   seven per-channel IRQ flags; for each raised channel it acks the flag
   (preserving the others) and fires the channel's callback, re-reading DICR
   until no flags remain. On a master/bus error it dumps each channel's MADR.
   (0x8005e680, 388 bytes.) */
void func_8005E680(void) {
  volatile unsigned int *reg;
  volatile unsigned int *dicr;
  DmaIrqCallback *p;
  DmaIrqCallback *base;
  DmaIrqCallback fn;
  unsigned int flags;
  int ch;
  int bit;
  unsigned int keep;

  flags =
      ((unsigned int)*(volatile unsigned int *)g_pDmaDicrReg >> 0x18) & 0x7F;
  if (flags != 0) {
    bit = 1;
    keep = 0xFFFFFF;
    base = (DmaIrqCallback *)g_apfnDmaCallbacks;
    do {
      ch = 0;
      if (flags != 0) {
        p = base;
        while (1) {
          if (ch >= 7) {
            break;
          }
          if (flags & 1) {
            dicr = (volatile unsigned int *)g_pDmaDicrReg;
            *dicr = *dicr & ((bit << (ch + 0x18)) | keep);
            fn = *p;
            if (fn != 0) {
              fn(dicr);
            }
          }
          p++;
          flags >>= 1;
          ch++;
          if (flags == 0) {
            break;
          }
        }
      }
      flags = ((unsigned int)*(volatile unsigned int *)g_pDmaDicrReg >> 0x18) &
              0x7F;
    } while (flags != 0);
  }
  reg = (volatile unsigned int *)g_pDmaDicrReg;
  if ((*reg & 0xFF000000) == 0x80000000 || (*reg & 0x8000)) {
    WritePrintf(D_80011678, *reg);
    for (ch = 0; ch < 7; ch++) {
      WritePrintf(D_80011694, ch, *(int *)(D_80074A0C + ch * 0x10));
    }
  }
}
