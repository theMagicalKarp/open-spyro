/* angle.h — 12-bit / 8-bit angle constants and the wrapped-delta tests,
 * without vecmath.h's prototypes (for files that keep their own spellings
 * of those). vecmath.h includes it. SINE_EASE needs LookupSine declared. */

#ifndef OPEN_SPYRO_ANGLE_H
#define OPEN_SPYRO_ANGLE_H

/* A whole and a half turn of a 12-bit / 8-bit angle, and its wrap mask: a wrapped delta
 * (a - b) & mask past HALF is taken the short way round by subtracting TURN
 * (and one below -HALF by adding it). */
#define ANGLE12_TURN 0x1000
#define ANGLE12_HALF 0x800
#define ANGLE8_TURN 0x100
#define ANGLE8_HALF 0x80
#define ANGLE12_MASK 0xFFF
#define ANGLE8_MASK 0xFF
/* The short way round from angle b to angle a turns the angle up (a - b,
 * wrapped, is 0..HALF); the eased turns add or subtract by it. */
#define ANGLE12_SHORT_WAY_UP(a, b) ((((a) - (b)) & ANGLE12_MASK) < ANGLE12_HALF + 1)
#define ANGLE8_SHORT_WAY_UP(a, b) ((((a) - (b)) & ANGLE8_MASK) < ANGLE8_HALF + 1)

/* A slow-in / slow-out ramp, FIXED_ONE - cos(phase): 0 at phase 0, 2.0
 * (0x2000) at a half turn (phase ANGLE12_HALF). The eased cutscene moves
 * scale a span by it >> 13. */
#define SINE_EASE(phase) (LookupSine((phase) - 0x400) + 0x1000)

#endif /* OPEN_SPYRO_ANGLE_H */
