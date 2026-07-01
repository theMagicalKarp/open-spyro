#include "globals.h"

/* Busy-wait delay used to space out consecutive SPU register writes: churns a
   volatile accumulator (*13 each pass) for 60 iterations so the values stay in
   memory and the loop isn't optimized away. (0x8005c720, 104 bytes.) */
void DelaySpuRegisterWrite(void) {
  volatile int i;
  volatile int acc;
  acc = 13;
  for (i = 0; i < 60; i++) {
    acc = acc * 13;
  }
}
