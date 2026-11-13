/* angle.h — 12-bit / 8-bit angle constants and the wrapped-delta tests,
 * without vecmath.h's prototypes (for files that keep their own spellings
 * of those). vecmath.h includes it. SINE_EASE needs LookupSine declared. */

#ifndef OPEN_SPYRO_ANGLE_H
#define OPEN_SPYRO_ANGLE_H

/* A whole, half and quarter turn of a 12-bit / 8-bit angle, and its wrap
 * mask: a wrapped delta
 * (a - b) & mask past HALF is taken the short way round by subtracting TURN
 * (and one below -HALF by adding it). */
#define ANGLE12_TURN 0x1000
#define ANGLE12_HALF 0x800
#define ANGLE8_TURN 0x100
#define ANGLE8_HALF 0x80
#define ANGLE12_QUARTER 0x400
#define ANGLE8_QUARTER 0x40
#define ANGLE12_MASK 0xFFF
#define ANGLE8_MASK 0xFF
/* The short way round from angle b to angle a turns the angle up (a - b,
 * wrapped, is 0..HALF); the eased turns add or subtract by it. */
#define ANGLE12_SHORT_WAY_UP(a, b) ((((a) - (b)) & ANGLE12_MASK) < ANGLE12_HALF + 1)
#define ANGLE8_SHORT_WAY_UP(a, b) ((((a) - (b)) & ANGLE8_MASK) < ANGLE8_HALF + 1)

/* Between the 8-bit angles of actor rot bytes / headings and the 12-bit
 * angles of the camera, Spyro's body eulers and the sine LUT. */
#define ANGLE8_TO_12(a) ((a) << 4)
#define ANGLE12_TO_8(a) ((a) >> 4)
/* The camera's 12-bit yaw as an 8-bit heading, the way the actor code reads
 * it: the low halfword sign-extended, then >> 4 (the caller declares
 * g_nCameraEulerYaw). */
#define CAMERA_YAW8 ((unsigned short)g_nCameraEulerYaw << 16 >> 20)

/* A slow-in / slow-out ramp, FIXED_ONE - cos(phase): 0 at phase 0, 2.0
 * (0x2000) at a half turn (phase ANGLE12_HALF). The eased cutscene moves
 * scale a span by it >> 13. */
#define SINE_EASE(phase) (LookupSine((phase) - ANGLE12_QUARTER) + 0x1000)

#endif /* OPEN_SPYRO_ANGLE_H */
