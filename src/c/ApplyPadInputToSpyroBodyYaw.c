#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"

/* 0x8003d840 (236 bytes) — steer Spyro's move-target yaw from the analog
   stick.  While the stick is engaged (pad word +0x10 set and the raw axes at
   +0x16/+0x17 not both centred at 0x7f) and its magnitude is under 0x60, step
   the target yaw toward the body yaw by (magnitude * wrapped delta) >> 9; a
   delta past 0x100 also raises the snap flag.  Always followed by a lean
   update, whose remaining |yaw delta| it returns. */
int ApplyPadInputToSpyroBodyYaw(void) {
  int vec[3];
  unsigned char *pad;
  int mag;
  int delta;
  int yaw;
  int tgt;

  pad = (unsigned char *)g_pPadSubstepState;
  if ((((PadSubstep *)pad)->stickActive != 0) &&
      ((((PadSubstep *)pad)->analog & 0xFFFF0000) != 0x7F7F0000)) {
    vec[0] = pad[0x16] - 0x7F;
    vec[1] = pad[0x17] - 0x7F;
    vec[2] = 0;
    mag = VectorLength(vec, VEC_LEN_XY);
    if (mag < 0x60) {
      tgt = g_anSpyroMoveTargetYawBlock[0];
      EBB_BREAK();
      yaw = g_anSpyroMoveTargetYawBlock[0x14];
      delta = (tgt - yaw) & 0xFFF;
      if (delta > 0x800) {
        delta -= 0x1000;
      }
      if (abs(delta) > 0x100) {
        g_nSpyroSubstate = 1;
      }
      g_anSpyroMoveTargetYawBlock[0] = (yaw + ((mag * delta) >> 9)) & 0xFFF;
    }
  }
  return TurnSpyroTowardMoveTargetWithLean(0);
}
