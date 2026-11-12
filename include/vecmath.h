/* vecmath.h — main's fixed-point vector / angle / trig library (0x800169AC..),
 * shared by main C and every overlay.
 *
 * Vectors are int[3] (VECTOR-style, 12-bit fraction unless noted); angles are
 * 12-bit (0x1000 = 360deg) or 8-bit (0x100 = 360deg) where the name says so.
 *
 * These prototypes are the ones the matched callers already declared; a file
 * that needs a different spelling (an unsigned return, a struct pointer
 * parameter) keeps its own extern and does not include this header. */

#ifndef OPEN_SPYRO_VECMATH_H
#define OPEN_SPYRO_VECMATH_H

/* 1.0 in the 12-bit-fraction fixed point (a unit vector's length, the
 * SetVector3Magnitude / ScaleVector3ByRatio scale for "normalise" and "times a
 * 4.12 factor"). */
#define FIXED_ONE 0x1000

/* A whole and a half turn of a 12-bit / 8-bit angle, and its wrap mask: a wrapped delta
 * (a - b) & mask past HALF is taken the short way round by subtracting TURN
 * (and one below -HALF by adding it). */
#define ANGLE12_TURN 0x1000
#define ANGLE12_HALF 0x800
#define ANGLE8_TURN 0x100
#define ANGLE8_HALF 0x80
#define ANGLE12_MASK 0xFFF
#define ANGLE8_MASK 0xFF

/* Trig: sine/cosine of a 12-bit angle (result 12-bit fraction), linearly
 * interpolated between the 256 entries of g_anSineLut / g_anCosineLut. */
int LookupSine(int angle);
int LookupCosine(int angle);

/* 2-arg arctangent of (x, y) in the scale ATAN_ANGLE8 / ATAN_ANGLE12, and
 * the byte-LUT 8-bit sibling (g_abArcTanOctantLut). */
#ifndef ATAN_ANGLE8
#define ATAN_ANGLE8 0  /* ArcTan2: 8-bit angle, 0..0xFF per turn */
#define ATAN_ANGLE12 1 /* ArcTan2: 12-bit angle, 0..0xFFF per turn (interpolated) */
#endif
int ArcTan2(int x, int y, int scale);
/* A wrapped 12-bit angle (0..0xFFF) more than a quarter turn either way of 0
 * (0x401..0xBFF): the direction points behind. */
#define ANGLE12_BEHIND(a) ((unsigned)((a) - 0x401) < 0x7FF)
int ArcTan2_8bit(int x, int y);

/* libc abs (gcc expands it inline). */
int abs(int x);

/* Shortest angle deltas: absolute and signed, 8- and 12-bit. */
int AbsAngleDelta8(int a, int b);
int AbsAngleDelta12(int a, int b);
int SignedAngleDelta8(int a, int b);

/* Integer square root (GTE LZCS + g_anSqrtLUT) and the octagonal 2D distance
 * approximation max + 3/8 min. */
int IntegerSqrt(int n);
int ApproxDist2D(int *a, int *b);

/* Element-wise int[3] helpers. */
void ZeroVector(int *vec);
void CopyVector(int *dst, int *src);
void AddVector(int *dst, int *a, int *b);
void SubtractVector(int *dst, int *a, int *b);
void LShiftVector3(int *vec, int shift);
void RShiftVector3(int *vec, int shift);
/* dst = src shifted right (arithmetic) by 17 - LZCS(|x| | |y| | |z|) when that
 * is positive, so every component fits an int16 for the GTE. */
void FitVector3ToInt16(int *dst, int *src);
/* 1 when all three components are equal, else 0. */
int VectorsEqual(int *a, int *b);
/* GTE OP (sf = 0, components taken as int16): dst = a x b, and
 * dst = (a - origin) x (b - origin), the unnormalized normal of a triangle.
 * The three helpers above and these two have no callers in the EXE or the
 * overlays (library leftovers). */
void CrossProduct(int *dst, int *a, int *b);
void CrossProductOfEdges(int *origin, int *a, int *b, int *dst);

/* Length of an int[3] (GTE sqr + g_anSqrtLUT); withZ = 0 (VEC_LEN_XY) leaves Z out. */
#ifndef VEC_LEN_XY
#define VEC_LEN_XY 0  /* VectorLength: X/Y only (Z ignored) */
#define VEC_LEN_XYZ 1 /* VectorLength: all three components */
#endif
int VectorLength(int *vec, int withZ);

/* Scaling: by num/den (GTE gpl0), by a 12-bit factor with IR saturation, and
 * normalize-then-scale to a target magnitude. */
void ScaleVector3ByRatio(int *vec, int den, int num);
void ScaleVector3Sat(int *dst, int *src, int scale);
void SetVector3Magnitude(int *vec, int magnitude);

/* GTE rotations (Y/Z sign-flipped in and out): by an explicit matrix, by the
 * currently loaded matrix, and by the camera matrix. */
void RotateVectorByMatrix(int *mtx, int *src, int *dst);
void ApplyActiveGteRotation(int *dst, int *src);
void WorldToCameraRotate(int *src, int *dst);

/* The game's PRNG. */
unsigned int GetRandomU32(void);

#endif /* OPEN_SPYRO_VECMATH_H */
