#include "globals.h"

extern int LookupCosine(int angle);
extern int LookupSine(int angle);
extern int ArcTan2(int y, int x, int high_precision);
extern unsigned int IntegerSqrt(unsigned int v);
extern void ApplyEulerRotation(unsigned char *euler, int *mtx, int flag);
extern void RotateVectorByMatrix(int *mtx, int *in, int *out);
extern void AddVector(int *dst, int *a, int *b);
extern void DampenSpyroFreeFallAttitude(void);

extern int D_80078CB0; /* pitch damping spring */
extern int D_80078CB4; /* roll damping spring */

/* Held base at g_nSpyroGroundSlope: the slope test, the first persistent-
   euler byte (-0xA4) and the three call-argument addresses (euler -0xA4, body
   matrix -0x7C, world position -0xB0) all come off one callee-saved reg. */
extern int g_anSpyroGroundSlopeBlock[];
#define SLOPE_BASE ((char *)g_anSpyroGroundSlopeBlock)

/* Second view of g_anSpyroGroundNormal for the reads after the second
   LookupCosine/LookupSine pair: with one symbol cse relates every access to
   one `la` pseudo and holds it across the calls (A231). */
extern int g_anSpyroGroundNormalPost[];

/* 0x8003dae4 (0x360) — per-frame glide/flight attitude controller near the
   ground (g_nSpyroGroundSlope < 0x17; otherwise DampenSpyroFreeFallAttitude).
   Projects the ground normal into Spyro's yaw frame, derives the target pitch
   and roll from it, spring-damps the body pitch/roll toward them (velocities
   in D_80078CB0/CB4), and when both persistent-euler snapshots are tilted
   past 0xc0+0x20 layers a ground-hug offset (rotated by the body euler
   matrix) onto g_anSpyroWorldPos.

   Match notes: the angle pair lives in an addressable `ang[2]` (stack 0x20)
   and the frame has one unreferenced 8-byte local after it; the euler and
   matrix pointers are ONE variable reassigned (a separate matrix pointer is
   local-allocated to s0 inside the last block and pushes the cosine temp to
   s1); `0x2000 - cos(pitch) - cos(roll)` is written in the order that fold
   reverses into the original's subtraction order. */

void UpdateSpyroFlightAttitudeNearGround(void) {
  int vec[3];
  int ang[2];
  int pad[2];
  int c;
  int s;
  char *p;

  if (g_anSpyroGroundSlopeBlock[0] >= 0x17) {
    DampenSpyroFreeFallAttitude();
  } else {
    c = LookupCosine(g_nSpyroBodyYaw);
    s = LookupSine(g_nSpyroBodyYaw);
    vec[0] =
        (g_anSpyroGroundNormal[0] * c + g_anSpyroGroundNormal[1] * s) >> 12;
    c = LookupCosine(g_nSpyroBodyYaw);
    s = LookupSine(g_nSpyroBodyYaw);
    vec[1] =
        (g_anSpyroGroundNormalPost[1] * c - g_anSpyroGroundNormalPost[0] * s) >>
        12;
    vec[2] = g_anSpyroGroundNormalPost[2];
    ang[0] =
        -ArcTan2(IntegerSqrt(vec[0] * vec[0] + vec[2] * vec[2]), vec[1], 1);
    ang[1] = -ArcTan2(vec[2], vec[0], 1);
    ang[0] = (ang[0] - g_nSpyroBodyPitch) & 0xfff;
    if (ang[0] > 0x800) {
      ang[0] -= 0x1000;
    }
    ang[1] = (ang[1] - g_nSpyroBodyRoll) & 0xfff;
    if (ang[1] > 0x800) {
      ang[1] -= 0x1000;
    }
    D_80078CB0 += ((ang[0] << 2) >> 4) - ((D_80078CB0 << 4) >> 6);
    D_80078CB4 += ((ang[1] << 2) >> 4) - ((D_80078CB4 << 4) >> 6);
    ang[0] = D_80078CB0 >> 2;
    ang[1] = D_80078CB4 >> 2;
    g_nSpyroBodyPitch = (g_nSpyroBodyPitch + ang[0]) & 0xfff;
    if (g_nSpyroBodyPitch > 0x800) {
      g_nSpyroBodyPitch -= 0x1000;
    }
    g_nSpyroBodyRoll = (g_nSpyroBodyRoll + ang[1]) & 0xfff;
    if (g_nSpyroBodyRoll > 0x800) {
      g_nSpyroBodyRoll -= 0x1000;
    }
    p = SLOPE_BASE - 0xa4;
    if ((unsigned char)(SLOPE_BASE[-0xa4] - 0x20) > 0xc0 &&
        (unsigned char)(g_abSpyroPersistentEuler[1] - 0x20) > 0xc0) {
      vec[1] = (-LookupSine(ang[0]) * 0x174) >> 12;
      vec[0] = (-LookupSine(ang[1]) * 0x174) >> 12;
      c = LookupCosine(ang[1]);
      vec[2] = ((0x2000 - LookupCosine(ang[0]) - c) * 0x174) >> 12;
      {
        char *euler = p;
        p = SLOPE_BASE - 0x7c;
        ApplyEulerRotation((unsigned char *)euler, (int *)p, 0);
      }
      RotateVectorByMatrix((int *)p, vec, vec);
      AddVector((int *)(SLOPE_BASE - 0xb0), (int *)(SLOPE_BASE - 0xb0), vec);
    }
  }
}
