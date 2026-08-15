#include "globals.h"

extern unsigned int VectorLength(int *vec, int include_z);
extern void TurnSpyroTowardMoveTargetWithLean(int lean);
extern int abs(int x);

/* held-base view of g_nSpyroMoveTargetYaw (0x80078b24): [0] is the move-target
   yaw, [0x14] (+0x50) is g_nSpyroBodyYaw. */
extern int g_anSpyroMoveTargetYawBlock[];

/* 0x8003d840 (236 bytes) — steer Spyro's move-target yaw from the analog
   stick.  While the stick is engaged (pad word +0x10 set and the raw axes at
   +0x16/+0x17 not both centred at 0x7f) and its magnitude is under 0x60, step
   the target yaw toward the body yaw by (magnitude * wrapped delta) >> 9; a
   delta past 0x100 also raises the snap flag.  Always followed by a lean
   update. */
void ApplyPadInputToSpyroBodyYaw(void) {
  int vec[3];
  unsigned char *pad;
  int mag;
  int delta;
  int yaw;
  int tgt;

  pad = (unsigned char *)g_pPadSubstepState;
  if ((*(int *)(pad + 0x10) != 0) &&
      ((*(unsigned int *)(pad + 0x14) & 0xFFFF0000) != 0x7F7F0000)) {
    vec[0] = pad[0x16] - 0x7F;
    vec[1] = pad[0x17] - 0x7F;
    vec[2] = 0;
    mag = VectorLength(vec, 0);
    if (mag < 0x60) {
      tgt = g_anSpyroMoveTargetYawBlock[0];
      do {
      } while (0);
      yaw = g_anSpyroMoveTargetYawBlock[0x14];
      delta = (tgt - yaw) & 0xFFF;
      if (delta > 0x800) {
        delta -= 0x1000;
      }
      if (abs(delta) > 0x100) {
        g_nSpyroStateFlags = 1;
      }
      g_anSpyroMoveTargetYawBlock[0] = (yaw + ((mag * delta) >> 9)) & 0xFFF;
    }
  }
  TurnSpyroTowardMoveTargetWithLean(0);
}
