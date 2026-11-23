/* src/native/host/stick.c — analog stick shaping (stick.h). */

#include "stick.h"

#include <math.h>

enum {
  REPORT_ANALOG_PAD = 0x73, /* DualShock in analog mode */
  CENTRE = 0x80,
  /* The game's calibration (FinalizePadCalibration): the dead zone runs
     0x30 either side of the snapshotted centre, the rest scales to full. */
  GAME_DEAD = 0x30,
};

/* Byte -> -1..1 (0x80 = 0; 0x00 and 0xFF are both full). */
static double axis_in(unsigned char b) {
  double v = ((int)b - CENTRE) / 127.0;

  return v < -1.0 ? -1.0 : v > 1.0 ? 1.0 : v;
}

/* -1..1 -> the byte the game's calibration maps back to the same deflection:
   just past its dead zone for the smallest, the extreme byte for full. */
static unsigned char axis_out(double v) {
  double a = fabs(v);

  if (a < 0.5 / 127.0) {
    return CENTRE;
  }
  if (v > 0) {
    return (unsigned char)(CENTRE + GAME_DEAD + 1 +
                           lround(a * (0xFF - (CENTRE + GAME_DEAD + 1))));
  }
  return (unsigned char)(CENTRE - GAME_DEAD - 1 -
                         lround(a * (CENTRE - GAME_DEAD - 1)));
}

void stick_shape(unsigned char *x, unsigned char *y, const StickShape *shape) {
  double fx = axis_in(*x), fy = axis_in(*y);
  double m = hypot(fx, fy);
  double inner = shape->range[0], span = shape->range[1] - inner;
  double out;

  if (shape->mode == STICK_MODE_ORIGINAL) {
    return;
  }
  if (m <= inner || m == 0.0) {
    *x = *y = CENTRE;
    return;
  }
  out = span > 0.0 ? (m - inner) / span : 1.0;
  if (out > 1.0) {
    out = 1.0;
  }
  out = pow(out, shape->curve);
  *x = axis_out(fx / m * out);
  *y = axis_out(fy / m * out);
}

void stick_shape_report(unsigned char *report, int len, const StickShape *left,
                        const StickShape *right) {
  if (len < 8 || report[0] != 0 || report[1] != REPORT_ANALOG_PAD) {
    return;
  }
  stick_shape(&report[4], &report[5], right);
  stick_shape(&report[6], &report[7], left);
}
