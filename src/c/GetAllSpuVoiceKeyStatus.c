#include "globals.h"

extern char *D_80073554; /* SPU voice register block base */

/* PSY-Q SpuGetAllKeysStatus equivalent: fills status[0..23] with each voice's
   key/envelope state (0 = off, 1 = on, 2 = release tail, 3 = attack ramp) from
   the live key-on mask bit and the voice envelope halfword at +0xc.
   (0x8005d9ec, 136 bytes.) */
void GetAllSpuVoiceKeyStatus(unsigned char *status) {
  int i;
  int n = 24;

  i = 0;
  do {
    unsigned short env = *(unsigned short *)(D_80073554 + i * 16 + 0xc);
    if (g_dwSpuLiveKeyOnMask & (1 << i)) {
      if (env != 0) {
        status[i] = 1;
      } else {
        status[i] = 3;
      }
    } else {
      if (env != 0) {
        status[i] = 2;
      } else {
        status[i] = 0;
      }
    }
    i++;
  } while (i < n);
}
