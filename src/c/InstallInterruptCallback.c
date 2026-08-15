#include "globals.h"

extern void ChangeClearPAD(int clear);
extern void ChangeClearRCnt(int spec, int clear);

/* 0x8005e224 (340 bytes) — PSY-Q InterruptCallback: install (callback != 0) or
   uninstall (callback == NULL) a per-IRQ handler at g_apfnIrqHandlers[irq],
   toggling the irq's bit in the I_MASK shadow and in the g_nIrqEnableMask
   enable shadow inside an I_MASK = 0 critical section.  irq 0 (VBlank) also
   re-arms the pad and root counter 3; irqs 4/5/6 re-arm root counters 0/1/2.
   Returns the callback that was installed before.  The callback-installed flag
   at -4 and the enable shadow at +0x2c are reached off the same held base as
   the handler table. */
int InstallInterruptCallback(unsigned int irq, int callback) {
  void **slot;
  void *prev;
  int imask;
  int bit;
  int nbit;
  char *base;

  base = (char *)g_apfnIrqHandlers;
  slot = (void **)(base + (irq << 2));
  prev = *slot;
  do {
  } while (0);
  if ((callback != (int)prev) &&
      (*(unsigned short *)((char *)g_apfnIrqHandlers - 4) != 0)) {
    imask = *(volatile unsigned short *)g_pIMaskReg;
    *(volatile unsigned short *)g_pIMaskReg = 0;
    if (callback != 0) {
      bit = 1;
      bit = bit << irq;
      *slot = (void *)callback;
      imask = imask | bit;
      *(unsigned short *)((char *)g_apfnIrqHandlers + 0x2c) =
          *(unsigned short *)((char *)g_apfnIrqHandlers + 0x2c) | bit;
    } else {
      nbit = ~(1 << irq);
      *slot = 0;
      imask = imask & nbit;
      *(unsigned short *)&g_nIrqEnableMask =
          *(unsigned short *)&g_nIrqEnableMask & nbit;
    }
    if (irq == 0) {
      ChangeClearPAD(callback == 0);
      ChangeClearRCnt(3, callback == 0);
    }
    if (irq == 4) {
      ChangeClearRCnt(0, callback == 0);
    }
    if (irq == 5) {
      ChangeClearRCnt(1, callback == 0);
    }
    if (irq == 6) {
      ChangeClearRCnt(2, callback == 0);
    }
    *(volatile unsigned short *)g_pIMaskReg = imask;
  }
  return (int)prev;
}
