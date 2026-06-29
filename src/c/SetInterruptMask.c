#include "globals.h"

/* Write the hardware interrupt-mask register and return its previous value
   (0x8005df44, 28 bytes). The old halfword is read before the new mask is
   stored. */
unsigned short SetInterruptMask(unsigned short mask) {
  unsigned short prev = *(volatile unsigned short *)g_pIMaskReg;
  *(volatile unsigned short *)g_pIMaskReg = mask;
  return prev;
}
