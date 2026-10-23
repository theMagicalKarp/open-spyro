#include "globals.h"
#include "pad.h"

/* Reset the four analog-stick calibration channels to neutral: center 0x7F,
   range 0..0xFF. The three sub-tables live at +0x20/+0x24/+0x28 in the pad
   calibration block. */
void ResetPadStickCalibrationDefaults(unsigned char *cal) {
  int i;

  for (i = 0; i < 4; i++) {
    unsigned char *p = cal + i;
    p[PAD_STATE_OFFSET(stickLow)] = 0;
    p[PAD_STATE_OFFSET(stickNeutral)] = 0x7F;
    p[PAD_STATE_OFFSET(stickHigh)] = 0xFF;
  }
}
