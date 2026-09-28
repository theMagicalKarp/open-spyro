#include "globals.h"

/* Master hardware-interrupt dispatcher hooked at the BIOS exception vector.
   Reads (I_MASK & enable-mask & I_STAT); for each of up to 11 IRQ sources ACKs
   the bit in I_STAT and calls g_apfnIrqHandlers[i]. Re-loops while any source
   remains pending, with a 0x800-cycle watchdog. (0x8005e03c, 488 bytes.)

   The pending-mask recompute pins the I_STAT pointer to $4 and the enable
   value to $3: as pseudos they tie on local-alloc priority and swap
   registers. The I_STAT read through the pinned pointer is non-volatile so
   it loads straight into an SImode value; a volatile HImode read becomes a
   subreg, which combine orders first in the AND. */

extern int WritePrintf();
extern void ReturnFromException(void);
extern unsigned short
    g_anLibapiIrqBlock[]; /* alias @0x80073924: [0]=g_nLibapiCallbackInstalled,
                             +4=g_apfnIrqHandlers */
extern unsigned char D_80011640[]; /* "unexpected interrupt(%04x)\n" */
extern unsigned char D_8001165C[]; /* "intr timeout(%04x:%04x)\n" */

void HandleHardwareInterrupt(void) {
  unsigned short *p;
  void (**base)();
  void (**h)();
  int i;
  int bit;
  unsigned int mask;
  unsigned int pend;

  p = g_anLibapiIrqBlock;
  if (p[0] == 0) {
    WritePrintf(D_80011640, *((volatile unsigned short *)g_pIStatReg));
    ReturnFromException();
  }
  g_nInIsrFlag = 1;
  {
    register unsigned short *is asm("$4") = g_pIStatReg;
    register unsigned int en asm("$3") =
        *((unsigned short *)(&g_nIrqEnableMask));
    pend = (*((volatile unsigned short *)g_pIMaskReg)) & (en & *is);
  }
  mask = pend;
  if (pend != 0) {
    bit = 1;
    base = (void (**)())(p + 2);
    do {
      i = 0;
      if (mask != 0) {
        h = base;
        while (1) {
          if (i >= 0xB) {
            break;
          }
          if (mask & 1) {
            *((volatile unsigned short *)g_pIStatReg) = ~(bit << i);
            if ((*h) != 0) {
              (*h)();
            }
          }
          h++;
          mask >>= 1;
          i++;
          if ((mask & 0xFFFF) == 0) {
            break;
          }
        }
      }
      {
        register unsigned short *is asm("$4") = g_pIStatReg;
        register unsigned int en asm("$3") =
            *((unsigned short *)(&g_nIrqEnableMask));
        pend = (*((volatile unsigned short *)g_pIMaskReg)) & (en & *is);
      }
      mask = pend;
    } while (pend != 0);
  }
  if (((*((volatile unsigned short *)g_pIStatReg)) &
       (*((volatile unsigned short *)g_pIMaskReg))) != 0) {
    int old = g_nIsrTimeoutCounter;
    g_nIsrTimeoutCounter = old + 1;
    if (old > 0x800) {
      WritePrintf(D_8001165C, *((volatile unsigned short *)g_pIStatReg),
                  *((volatile unsigned short *)g_pIMaskReg));
      g_nIsrTimeoutCounter = 0;
      *((volatile unsigned short *)g_pIStatReg) = 0;
    }
  } else {
    g_nIsrTimeoutCounter = 0;
  }
  g_nInIsrFlag = 0;
  ReturnFromException();
}
