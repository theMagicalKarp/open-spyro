#include "globals.h"

/* Reconcile software voice rows with the SPU (0x80056e3c): probe all 24
   hardware voice key states into a stack buffer; for any voice the hardware
   reports keyed-on (1) whose software flags halfword (+0x36, stride 0x1C)
   has neither active (0x1) nor idle (0x40) set, mark the row idle and OR the
   voice bit into the pending hardware key-off mask. */
extern void GetAllSpuVoiceKeyStatus();

void SyncSoundVoicesWithHardware(void) {
  unsigned char status[24];
  unsigned int *pending;
  unsigned char *p;
  int i, one, idle, off;
  GetAllSpuVoiceKeyStatus(status);
  i = 0;
  one = 1;
  idle = 0x40;
  pending = &g_dwSpuPendingKeyOff;
  off = 0;
  p = status;
  do {
    int state = *p;
    if (state == one) {
      int mask = state << i;
      if ((*(volatile unsigned short *)((char *)g_abSpuCommonAttr + off +
                                        0x36) &
           0xC1) == 0) {
        *(volatile unsigned short *)((char *)g_abSpuCommonAttr + off + 0x36) =
            idle;
        *pending |= mask;
      }
    }
    off += 0x1C;
    i += 1;
    p += 1;
  } while (i < 24);
}
