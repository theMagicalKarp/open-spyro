#include "globals.h"

/* Reset the four analog-stick calibration channels to neutral: center 0x7F,
   range 0..0xFF. The three sub-tables live at +0x20/+0x24/+0x28 in the pad
   calibration block. */
void ResetPadStickCalibrationDefaults(unsigned char *cal) {
  int i;

  for (i = 0; i < 4; i++) {
    unsigned char *p = cal + i;
    p[0x24] = 0;
    p[0x20] = 0x7F;
    p[0x28] = 0xFF;
  }
}
