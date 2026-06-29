#include "globals.h"

extern unsigned int BuildTextureWindow(unsigned char *rect);

/* libgpu SetTexWindow: stamp a texture-window primitive (len byte = 2) whose
   code word is the GP0(0xe2) command built from `rect`, then clear the trailing
   word (0x80060540, 60 bytes). */
void func_80060540(unsigned char *p, unsigned char *rect) {
  *(p + 3) = 2;
  *(unsigned int *)(p + 4) = BuildTextureWindow(rect);
  *(unsigned int *)(p + 8) = 0;
}
