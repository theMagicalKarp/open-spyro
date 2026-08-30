#include "globals.h"

/* Per-speed-bucket turn-rate table at 0x8006c5f0; row stride 0x1a bytes, the
   lean-indexed rate short sits at +0xc (short[6]). */
extern short D_8006C5F0[];

extern void IntegrateSpyroBodyEuler(int delta);
extern int AbsAngleDelta12(int a, int b);

/* 0x8003d6d0, 368 B: Steers Spyro's body yaw toward the move-target yaw using a
   lean accumulator g_nSpyroBodyYawStep (clamped -6..+6). Picks the turn rate
   from the 2D table (speed bucket selects the row, the lean the column). Resets
   the lean when the desired delta falls inside the table-indexed band, then
   integrates the chosen angular delta and reports the remaining |delta| via
   AbsAngleDelta12.

   The two yaw reads are shared locals with a second set in the recompute arm
   (A238): a single-set temp is boosted by sched1's birthing hoist and comes out
   of the block LAST, so both loads have to be multi-set for the target read to
   be emitted first. That puts both in global.c's allocno_compare, where the
   target read always loses (its live range is strictly the longer one), so the
   nested do/while(0) around the arm's target read weights that ref by 3 --
   6 refs / 6 insns == the body read's 4 / 4 -- and the tie falls to the lower
   allocno number, which is the target read. */
void TurnSpyroTowardMoveTargetWithLean(int bucket) {
  int delta;
  int rate;
  int targetYaw;
  int bodyYaw;
  short *base;
  short *row;

  targetYaw = g_nSpyroMoveTargetYaw;
  bodyYaw = g_nSpyroBodyYaw;
  delta = targetYaw - bodyYaw;
  delta = delta & 0xFFF;
  do {
    if (delta < 0x800) {
      g_nSpyroBodyYawStep = g_nSpyroBodyYawStep + 1;
      if (g_nSpyroBodyYawStep < 1) {
        g_nSpyroBodyYawStep = 1;
      }
      if (g_nSpyroBodyYawStep >= 7) {
        g_nSpyroBodyYawStep = 6;
      }
    } else {
      g_nSpyroBodyYawStep = g_nSpyroBodyYawStep - 1;
      if (g_nSpyroBodyYawStep < -6) {
        g_nSpyroBodyYawStep = -6;
      }
      if (g_nSpyroBodyYawStep >= 0) {
        g_nSpyroBodyYawStep = -1;
      }
    }
  } while (0);

  base = D_8006C5F0;
  row = (short *)((char *)base + bucket * 0x1a);
  rate = row[g_nSpyroBodyYawStep + 6];
  if (rate >= delta || rate + 0x1000 <= delta) {
    do {
      do {
        targetYaw = g_nSpyroMoveTargetYaw;
      } while (0);
    } while (0);
    bodyYaw = g_nSpyroBodyYaw;
    rate = (targetYaw - bodyYaw) & 0xFFF;
    if (rate > 0x800) {
      rate = rate - 0x1000;
    }
    g_nSpyroBodyYawStep = 0;
  }
  IntegrateSpyroBodyEuler(rate);
  AbsAngleDelta12(g_nSpyroMoveTargetYaw, g_nSpyroBodyYaw);
}
