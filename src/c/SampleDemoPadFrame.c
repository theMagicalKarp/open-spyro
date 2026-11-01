#include "gamestate.h"
#include "globals.h"
#include "main_shared.h"
#include "pad.h"
#include "pad_buttons.h"

/* Pad sampler for the attract demo (0x800539fc, 0x26C). While a demo is
   being recorded (g_nDemoMode == DEMO_RECORD) it samples the real
   pad — inverted button halfword plus the calibrated right-stick bytes —
   and appends the packed frame at g_pDemoPadCursor; otherwise it
   plays the next recorded frame back into the pad state (stick centered
   at 0x7F). Either way it then rebuilds pressed/held/released, the
   stick-active flag, fans the held state out to the 4 pad substep-ring
   slots, refreshes the input-idle flags and forces a 2-tick frame.
   Returns 2 (the tick count). */

int SampleDemoPadFrame(void) {
  int frame;
  unsigned int held;
  unsigned int rel;
  int i;
  int off;
  unsigned int *p;
  unsigned int *heldp;

  if (g_nDemoMode == DEMO_RECORD) {
    unsigned char *cal = g_abPadAnalogCalibratedBlock;
    unsigned char *cur;
    frame = ~((g_abPadRawReport[2] << 8) | g_abPadRawReport[3]);
    *(unsigned int *)cal = *(unsigned int *)&g_abPadRawReport[4];
    MapPadAxisToCalibratedRange(cal, (int)(cal - 0x14));
    frame &= 0xFFFF;
    frame |= cal[PAD_AXIS_LX] << 16;
    frame |= cal[PAD_AXIS_LY] << 24;
    cur = g_pDemoPadCursor;
    g_pDemoPadCursor = cur + 4;
    *(unsigned int *)cur = frame;
  } else {
    unsigned char *cal = (unsigned char *)&g_dwPadAnalogCalibrated;
    unsigned char *cur;
    cur = g_pDemoPadCursor;
    g_pDemoPadCursor = cur + 4;
    frame = *(int *)cur;
    cal[0] = 0x7F;
    cal[1] = 0x7F;
    cal[PAD_AXIS_LX] = frame >> 16;
    cal[PAD_AXIS_LY] = frame >> 24;
  }
  frame &= 0xFFFF;
  held = g_dwPadHeld;
  g_nPadType = 3;
  g_dwPadPressed = ~held & frame;
  rel = held & ~frame;
  g_dwPadHeld = frame;
  g_dwPadReleased = rel;
  if ((g_dwPadAnalogCalibrated & PAD_LEFT_STICK_MASK) !=
      PAD_LEFT_STICK_CENTERED) {
    g_nPadStickActiveFlag = 1;
  } else {
    g_nPadStickActiveFlag = 0;
  }
  i = 0;
  heldp = g_adwPadHeldBlock;
  p = &heldp[0x14];
  off = 0;
  do {
    PAD_SUBSTEP(off)->held = heldp[0];
    PAD_SUBSTEP(off)->pressed = heldp[-2];
    PAD_SUBSTEP(off)->released = heldp[-1];
    PAD_SUBSTEP(off)->padType = heldp[1];
    i += 1;
    *p = heldp[3];
    p += 6;
    PAD_SUBSTEP(off)->stickActive = heldp[2];
    off += 0x18;
  } while (i < 4);
  g_nPadAllInputIdleFlag = 1;
  g_nPadDirectionalIdleFlag = 1;
  if ((g_dwPadHeld & PAD_DPAD) != 0 ||
      (g_dwPadAnalogCalibrated & PAD_LEFT_STICK_MASK) !=
          PAD_LEFT_STICK_CENTERED) {
    g_nPadDirectionalIdleFlag = 0;
  }
  if (g_nPadDirectionalIdleFlag == 0 ||
      (g_dwPadHeld & (PAD_DPAD | 0x00FF)) != 0) {
    g_nPadAllInputIdleFlag = 0;
  }
  g_nFrameTicks = 2;
  return 2;
}
