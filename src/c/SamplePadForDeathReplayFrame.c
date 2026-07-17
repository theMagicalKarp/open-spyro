#include "globals.h"

/* Pad sampler for the death-replay stream (0x800539fc, 0x26C). While the
   death sequence is recording (g_nDeathState == 2) it samples the real
   pad — inverted button halfword plus the calibrated right-stick bytes —
   and appends the packed frame at g_pDeathReplayCursor; otherwise it
   plays the next recorded frame back into the pad state (stick centered
   at 0x7F). Either way it then rebuilds pressed/held/released, the
   stick-active flag, fans the held state out to the 4 pad substep-ring
   slots, refreshes the input-idle flags and forces a 2-tick frame.
   Returns 2 (the tick count). */
extern void MapPadAxisToCalibratedRange(unsigned char *cal, int block);

extern unsigned int g_adwPadHeldBlock[]; /* g_dwPadHeld as an array view */
extern unsigned char
    g_abPadAnalogCalibratedBlock[]; /* calibrated-stick bytes */

int SamplePadForDeathReplayFrame(void) {
  int frame;
  unsigned int held;
  unsigned int rel;
  int i;
  int off;
  unsigned int *p;
  unsigned int *heldp;

  if (g_nDeathState == 2) {
    unsigned char *cal = g_abPadAnalogCalibratedBlock;
    unsigned char *cur;
    frame = ~((g_abPadRawReport[2] << 8) | g_abPadRawReport[3]);
    *(unsigned int *)cal = *(unsigned int *)&g_abPadRawReport[4];
    MapPadAxisToCalibratedRange(cal, (int)(cal - 0x14));
    frame &= 0xFFFF;
    frame |= cal[2] << 16;
    frame |= cal[3] << 24;
    cur = g_pDeathReplayCursor;
    g_pDeathReplayCursor = cur + 4;
    *(unsigned int *)cur = frame;
  } else {
    unsigned char *cal = (unsigned char *)&g_dwPadAnalogCalibrated;
    unsigned char *cur;
    cur = g_pDeathReplayCursor;
    g_pDeathReplayCursor = cur + 4;
    frame = *(int *)cur;
    cal[0] = 0x7F;
    cal[1] = 0x7F;
    cal[2] = frame >> 16;
    cal[3] = frame >> 24;
  }
  frame &= 0xFFFF;
  held = g_dwPadHeld;
  g_nPadType = 3;
  g_dwPadPressed = ~held & frame;
  rel = held & ~frame;
  g_dwPadHeld = frame;
  g_dwPadReleased = rel;
  if ((g_dwPadAnalogCalibrated & 0xFFFF0000) != 0x7F7F0000) {
    g_nPadStickActiveFlag = 1;
  } else {
    g_nPadStickActiveFlag = 0;
  }
  i = 0;
  heldp = g_adwPadHeldBlock;
  p = &heldp[0x14];
  off = 0;
  do {
    *(unsigned int *)((char *)g_anPadSubstepRing + off + 4) = heldp[0];
    *(unsigned int *)((char *)g_anPadSubstepRing + off + 8) = heldp[-2];
    *(unsigned int *)((char *)g_anPadSubstepRing + off + 0xC) = heldp[-1];
    *(unsigned int *)((char *)g_anPadSubstepRing + off) = heldp[1];
    i += 1;
    *p = heldp[3];
    p += 6;
    *(unsigned int *)((char *)g_anPadSubstepRing + off + 0x10) = heldp[2];
    off += 0x18;
  } while (i < 4);
  g_nPadAllInputIdleFlag = 1;
  g_nPadDirectionalIdleFlag = 1;
  if ((g_dwPadHeld & 0xF000) != 0 ||
      (g_dwPadAnalogCalibrated & 0xFFFF0000) != 0x7F7F0000) {
    g_nPadDirectionalIdleFlag = 0;
  }
  if (g_nPadDirectionalIdleFlag == 0 || (g_dwPadHeld & 0xF0FF) != 0) {
    g_nPadAllInputIdleFlag = 0;
  }
  g_nFrameTicks = 2;
  return 2;
}
