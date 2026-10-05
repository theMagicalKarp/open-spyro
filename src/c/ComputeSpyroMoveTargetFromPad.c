#include "globals.h"

extern int ArcTan2(int y, int x, int high_precision);

/* D-pad -> yaw table (16 entries, stride 2), indexed by the pad button nibble.
 */
extern short D_8006C5D0[];
extern int g_anSpyroMoveTargetYawBlock[]; /* &g_nSpyroMoveTargetYaw */

/* 0x8003d3b8 (0x174) — convert this substep's pad input (g_pPadSubstepState)
   into a movement target. Analog stick (bytes +0x16/+0x17 centred on 0x7f):
   ArcTan2 target yaw made camera-relative, speed scaled by |dx|+|dy|. D-pad
   (button nibble 0xf000): the yaw table plus g_nCameraEulerYaw at full speed.
   Neutral input holds the persistent yaw and zeroes speed.

   Match notes: the two analog yaw stores go through one held base (`target`,
   via the incomplete-array alias). The first store is volatile and so is the
   pad pointer reload, so the reload stays below it (true_dependence's
   both-volatile clause) but may rise above the plain second store. Each
   branch reloads the pad pointer into its own local (a function-wide `pad`
   would be one global-alloc pseudo in a1). Both stick bytes are read before
   the abs blocks, and |dx| is computed in place in `k` (the original reuses
   the 0x7f register, s0). */
void ComputeSpyroMoveTargetFromPad(int speed) {
  unsigned char *pad;
  unsigned char *stick;
  unsigned char *dpad;
  volatile int *target;
  int yaw;
  int k;
  int y;
  int x;
  int dy;

  pad = (unsigned char *)g_pPadSubstepState;
  if (*(int *)(pad + 0x10) != 0 &&
      (*(unsigned int *)(pad + 0x14) & 0xffff0000) != 0x7f7f0000) {
    k = 0x7f;
    yaw = ArcTan2(k - pad[0x17], k - pad[0x16], 1);
    target = (volatile int *)g_anSpyroMoveTargetYawBlock;
    *target = yaw;
    *(int *)target = (yaw + g_nCameraEulerYaw) & 0xfff;
    stick = *(unsigned char *volatile *)&g_pPadSubstepState;
    y = stick[0x17];
    x = stick[0x16];
    dy = k - y;
    if (dy < 0) {
      dy = -dy;
    }
    k = k - x;
    if (k < 0) {
      k = -k;
    }
    g_nSpyroSpeedTarget = speed * (dy + k) >> 7;
    if (speed < g_nSpyroSpeedTarget) {
      g_nSpyroSpeedTarget = speed;
    }
  } else {
    dpad = (unsigned char *)g_pPadSubstepState;
    if ((*(unsigned int *)(dpad + 4) & 0xf000) != 0) {
      g_nSpyroSpeedTarget = speed;
      g_nSpyroMoveTargetYaw =
          *(short *)((char *)D_8006C5D0 + ((*(int *)(dpad + 4) >> 0xb) & 0x1e));
      g_nSpyroMoveTargetYaw =
          (g_nSpyroMoveTargetYaw + g_nCameraEulerYaw) & 0xfff;
    } else {
      g_nSpyroSpeedTarget = 0;
      g_nSpyroBodyYawStep = 0;
      g_nSpyroMoveTargetYaw = g_abSpyroPersistentEuler[2] << 4;
    }
  }
}
