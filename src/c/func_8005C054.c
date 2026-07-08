#include "globals.h"

/* SPU DMA-completion service (0x8005c054, _spu_FiDMA-style): flush the
   pending register write unless a DMA direction is latched, clear the
   transfer-mode bits (0x30) of the SPUCNT shadow (+0x1AA off the SPU
   register block), busy-wait up to 0xF00 spins for the hardware to drop
   them, then fire the user transfer callback if installed or deliver the
   SPU DMA event. */
extern void DelaySpuRegisterWrite(void);
extern void DeliverEvent();
extern char *D_80073554;
extern volatile int D_8007358C;

void func_8005C054(void) {
  char *regs;
  unsigned int count;
  if (g_nSpuPendingDmaDirection == 0) {
    DelaySpuRegisterWrite();
  }
  regs = D_80073554;
  *(volatile unsigned short *)(regs + 0x1AA) =
      *(volatile unsigned short *)(regs + 0x1AA) & 0xFFCF;
  count = 0;
  while ((*(volatile unsigned short *)(regs + 0x1AA) & 0x30) != 0) {
    count += 1;
    if (count >= 0xF01) {
      break;
    }
  }
  if (D_8007358C != 0) {
    ((void (*)())D_8007358C)();
  } else {
    DeliverEvent(0xF0000009, 0x20);
  }
}
