#include "funcs.h"
#include "globals.h"
#include "spu.h"

/* Reconcile software voice rows with the SPU (0x80056e3c): probe all 24
   hardware voice key states into a stack buffer; for any voice the hardware
   reports keyed-on (1) whose row is free (no VOICE_ROW_BUSY bit in nFlags),
   mark the row VOICE_ROW_STOPPED and OR the voice bit into the pending hardware
   key-off mask. */

void SyncSoundVoicesWithHardware(void) {
  unsigned char status[24];
  unsigned int *pending;
  unsigned char *p;
  int i, one, idle, off;
  GetAllSpuVoiceKeyStatus(status);
  i = 0;
  one = 1;
  idle = VOICE_ROW_STOPPED;
  pending = &g_dwSpuPendingKeyOff;
  off = 0;
  p = status;
  do {
    int state = *p;
    if (state == one) {
      int mask = state << i;
      if ((VOICE_ROW_RAW(off, volatile unsigned short, nFlags) &
           VOICE_ROW_BUSY) == 0) {
        VOICE_ROW_RAW(off, volatile unsigned short, nFlags) = idle;
        *pending |= mask;
      }
    }
    off += VOICE_ROW_STRIDE;
    i += 1;
    p += 1;
  } while (i < SPU_VOICE_COUNT);
}
