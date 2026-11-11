#include "funcs.h"
#include "globals.h"
#include "main_shared.h"
#include "match_asm.h"
#include "pad.h"
#include "spyro_block.h"
#include "spyro_state.h"

/* 0x8003d840 (236 bytes) — steer Spyro's move-target yaw from the analog
   stick.  While the stick is engaged (stickActive set and the left-stick axes
   not both centred at 0x7f) and its magnitude is under 0x60, step the target
   yaw toward the body yaw by (magnitude * wrapped delta) >> 9; a delta past
   0x100 also raises the snap flag.  Always followed by a lean update, whose
   remaining |yaw delta| it returns. */
int ApplyPadInputToSpyroBodyYaw(void) {
  int vec[3];
  unsigned char *pad;
  int mag;
  int delta;
  int yaw;
  int tgt;

  pad = (unsigned char *)g_pPadSubstepState;
  if ((((PadSubstep *)pad)->stickActive != 0) &&
      ((((PadSubstep *)pad)->analog.word & PAD_LEFT_STICK_MASK) !=
       PAD_LEFT_STICK_CENTERED)) {
    vec[0] = ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LX] - PAD_STICK_CENTER;
    vec[1] = ((PadSubstep *)pad)->analog.axis[PAD_AXIS_LY] - PAD_STICK_CENTER;
    vec[2] = 0;
    mag = VectorLength(vec, VEC_LEN_XY);
    if (mag < SPYRO_PIVOT_STICK) {
      tgt = g_anSpyroMoveTargetYawBlock[0];
      EBB_BREAK();
      yaw = g_anSpyroMoveTargetYawBlock[SPY_BODY_YAW - SPY_MOVE_TARGET_YAW];
      delta = (tgt - yaw) & 0xFFF;
      if (delta > 0x800) {
        delta -= 0x1000;
      }
      if (abs(delta) > SPYRO_PIVOT_TURN) {
        g_nSpyroSubstate = SPYRO_SUB_WALK_PIVOT;
      }
      g_anSpyroMoveTargetYawBlock[0] = (yaw + ((mag * delta) >> 9)) & 0xFFF;
    }
  }
  return TurnSpyroTowardMoveTargetWithLean(0);
}
