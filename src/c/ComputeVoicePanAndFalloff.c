#include "globals.h"

/* 3D positional sound pan + distance falloff. out[L,R] gains are derived from
   the pan byte (0x80 = center; wraps to full-L/full-R via the 0x40/0xbf
   window), scaled by the falloff fraction (vol<<8)/range applied to the
   listener-relative baseline, then each channel is clamped to [0,0x3fff]. When
   the mono-mix flag is set both channels collapse to their average.
   (0x80056c84, 320 bytes.) */

/* Body is decomp-permuter output from the 2026-07-25 sweep (it reads worse than
   the hand-written form it replaced); original hand-written body:
   git show 46305b0:src/c/ComputeVoicePanAndFalloff.c.wip
   Two things are load-bearing: `pad` sizes the 0x20 frame (A127 — without it
   the frame is 0x10 and the 5th argument reloads from 0x20(sp) instead of
   0x30(sp)), and the wrap arm must negate the PARAMETER and mask the COPY
   (`new_var`), not the other way round — that is what puts a2/v1 in the
   original's roles across the beqz and j delay slots. */

void ComputeVoicePanAndFalloff(short *out, int vol, int pan, unsigned int range,
                               short *base) {
  int pad[4];
  unsigned int lo;
  unsigned char new_var;
  int gainL;
  int gainR;
  short bL;
  short bR;
  short cur;
  new_var = (int)pan;
  if (range != 0) {
    if (((unsigned int)((pan - 0x40) & 0xFF)) < 0x80) {
      gainL = (-pan) & 0xFF;
      gainR = new_var & 0xFF;
    } else {
      gainL = (pan + 0x80) & 0xFF;
      gainR = (0x80 - pan) & 0xFF;
    }
    lo = ((unsigned int)(vol << 8)) / range;
    bL = base[0];
    out[0] =
        (short)(((int)(gainL *
                       ((short)(bL - (((unsigned int)(lo * bL)) >> 8))))) >>
                7);
    bR = base[1];
    out[1] =
        (short)(((int)(gainR *
                       ((short)(bR - (((unsigned int)(lo * bR)) >> 8))))) >>
                7);
    if (out[0] < 0) {
      out[0] = 0;
    } else if (out[0] >= 0x4000) {
      out[0] = 0x3FFF;
    }
    if (out[1] < 0) {
      out[1] = 0;
    } else if (out[1] >= 0x4000) {
      out[1] = 0x3FFF;
    }
    if (g_nSoundMonoMixFlag != 0) {
      cur = (short)(((unsigned int)(out[1] + out[0])) >> 1);
      out[0] = cur;
      out[1] = cur;
    }
  }
}
