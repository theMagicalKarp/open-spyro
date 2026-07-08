#include "globals.h"

/* 0x80053940: maps 4 raw 8-bit analog stick axes into the calibrated
   0..0xff range using per-axis calibration bytes at +0x2c/+0x30 and per-axis
   u16 scale factors at +0x34/+0x3c (see FinalizePadCalibration). */
void MapPadAxisToCalibratedRange(unsigned char *param_1, int param_2) {
  int i;
  unsigned char *calib;
  unsigned int raw, hi, lo;
  int v;

  calib = (unsigned char *)param_2;
  for (i = 0; i < 4; i++) {
    unsigned char *row = i + (unsigned char *)param_2;
    raw = param_1[0];
    hi = row[0x2c];
    if (raw > hi) {
      int shifted =
          (((int)(raw - hi)) * *(unsigned short *)(calib + i * 2 + 0x34)) >> 7;
      v = shifted + 0x7f;
      if (v >= 0x100) {
        param_1[0] = 0xff;
      } else {
        param_1[0] = (unsigned char)v;
      }
    } else {
      lo = row[0x30];
      if (raw < lo) {
        v = 0x7f -
            (((int)(lo - raw) * *(unsigned short *)(calib + i * 2 + 0x3c)) >>
             7);
        if (v < 0) {
          param_1[0] = 0;
        } else {
          param_1[0] = (unsigned char)v;
        }
      } else {
        param_1[0] = 0x7f;
      }
    }
    param_1 += 1;
  }
}
