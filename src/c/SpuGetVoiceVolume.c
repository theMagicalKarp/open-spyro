#include "globals.h"

extern short *D_80073554; /* SPU voice register shadow base */

/* libspu: read a voice's left/right volume registers (base + voice*0x10)
   back as signed values -- a register value of 0x4000 or more is the
   negative half of the range and has 0x8000 taken off. (0x8005dab4, 96
   bytes; splat used to split it into three pieces at its jump targets.)

   Matching notes: the registers are read once into unsigned shorts, so the
   compares zero-extend a copy (andi) while the plain arm stores the raw
   load. The 0x8000 goes through the int temp `t` first: a literal operand is
   turned into an add of -0x8000 at expand time, a register one stays a
   subtract (`ori 0x8000; subu`). */
void SpuGetVoiceVolume(int voice, short *left, short *right) {
  volatile unsigned short *reg;
  unsigned short l;
  unsigned short r;
  int t;

  reg = (volatile unsigned short *)(voice * 0x10 + (int)D_80073554);
  l = reg[0];
  r = reg[1];
  if (l >= 0x4000) {
    t = 0x8000;
    t = l - t;
    *left = t;
  } else {
    *left = l;
  }
  if (r >= 0x4000) {
    t = 0x8000;
    t = r - t;
    *right = t;
  } else {
    *right = r;
  }
}
