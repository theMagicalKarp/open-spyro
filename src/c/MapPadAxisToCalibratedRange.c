#include "globals.h"
#include "pad.h"

/* 0x80053940: maps 4 raw 8-bit analog stick axes into the calibrated
   0..0xff range: outside the dead zone [deadLow, deadHigh] the deflection is
   scaled by the axis' Q7 gain away from center 0x7f (see
   FinalizePadCalibration, which fills the PadState tables). */
void MapPadAxisToCalibratedRange(unsigned char *axes, int cal) {
  int i;
  PadState *calib;
  unsigned int raw, hi, lo;
  int v;

  calib = (PadState *)cal;
  for (i = 0; i < 4; i++) {
    raw = axes[0];
    hi = calib->deadHigh[i];
    if (raw > hi) {
      int shifted = (((int)(raw - hi)) * calib->scaleHigh[i]) >> 7;
      v = shifted + 0x7f;
      if (v >= 0x100) {
        axes[0] = 0xff;
      } else {
        axes[0] = (unsigned char)v;
      }
    } else {
      lo = calib->deadLow[i];
      if (raw < lo) {
        v = 0x7f - (((int)(lo - raw) * calib->scaleLow[i]) >> 7);
        if (v < 0) {
          axes[0] = 0;
        } else {
          axes[0] = (unsigned char)v;
        }
      } else {
        axes[0] = 0x7f;
      }
    }
    axes += 1;
  }
}
