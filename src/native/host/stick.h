/* src/native/host/stick.h — analog stick shaping (input.left_stick.*,
 * input.right_stick.*): dead zones and a response curve, applied to pad
 * reports before the game reads them.
 *
 * Kept free of SDL and the wasm module so it builds into the host unit tests
 * (native/tests). */

#ifndef OPEN_SPYRO_NATIVE_STICK_H
#define OPEN_SPYRO_NATIVE_STICK_H

/* input.<stick>.mode: how the host treats the stick. */
typedef enum StickMode {
  STICK_MODE_ORIGINAL, /* "original": the report passes through untouched */
  STICK_MODE_RADIAL,   /* "radial": the dead zones and curve below */
} StickMode;

/* One stick's settings; the rest are fractions of full deflection, used by
   STICK_MODE_RADIAL only. */
typedef struct StickShape {
  StickMode mode;
  double deadzone;       /* below this the stick reads centred */
  double outer_deadzone; /* within this of the edge it reads full */
  double curve; /* response exponent: 1 linear, > 1 finer near centre */
} StickShape;

/* The original game's feel: the bytes reach the game's own calibration
   unchanged, which ignores 0x30 of 0x80 per axis around the centre it
   snapshots and is linear past that. The radial values are the original's
   dead zone size, for when the mode is switched to radial. */
#define STICK_SHAPE_ORIGINAL {STICK_MODE_ORIGINAL, 0.375, 0.0, 1.0}

/* Shapes one stick's two report bytes (0x80 = centre) in place; a no-op in
   STICK_MODE_ORIGINAL. In STICK_MODE_RADIAL the output is encoded for the
   game's calibration (src/main/pad/pad.c: its dead zone is 0x30 either side
   of the centre it snapshots, 0x80 here), so the shaped deflection reaches
   the game unchanged and the game's own dead zone no longer applies. */
void stick_shape(unsigned char *x, unsigned char *y, const StickShape *shape);

/* Shapes both sticks of an analog pad report (id 0x73: bytes 4/5 right X/Y,
   6/7 left X/Y); other reports are left alone. */
void stick_shape_report(unsigned char *report, int len, const StickShape *left,
                        const StickShape *right);

#endif
