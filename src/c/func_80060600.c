#include "globals.h"

extern unsigned int BuildDrawOffsetCommand(int x, int y);

/* libgpu SetDrawOffset: stamp a draw-offset primitive (len byte = 2) whose code
   word is the GP0(0xe5) command built from the (x, y) pair at `off`, then clear
   the trailing word (0x80060600, 68 bytes). */
void func_80060600(unsigned char *p, short *off) {
  *(p + 3) = 2;
  *(unsigned int *)(p + 4) = BuildDrawOffsetCommand(off[0], off[1]);
  *(unsigned int *)(p + 8) = 0;
}
