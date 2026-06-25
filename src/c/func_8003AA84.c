#include "globals.h"

/* Tick the +0x4E countdown byte on an actor. When the 0x10000 flag (+0x18) is
   set it counts UP by 0x18, wrapping back to 0x60 once it reaches 0x61. When
   the flag is clear it counts DOWN: by 2 while >= 0x21, by 1 below that, and
   holds at 0. */
void func_8003AA84(char *e) {
  if (*(int *)(e + 0x18) & 0x10000) {
    int v = (unsigned char)e[0x4E] + 0x18;
    e[0x4E] = v;
    if ((unsigned char)v >= 0x61) {
      e[0x4E] = 0x60;
    }
  } else {
    unsigned char b;
    if ((b = e[0x4E]) >= 0x21) {
      e[0x4E] = b - 2;
    } else if (b != 0) {
      e[0x4E] = b - 1;
    }
  }
}
