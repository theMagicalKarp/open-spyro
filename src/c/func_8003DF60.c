#include "globals.h"

/* Reset Spyro's per-frame motion: copy between the world position and the
   previous-position slot (g_anSpyroPrevWorldPos, +0x8c), clear the motion
   vector (g_anSpyroMotionVec, +0x10c) and the frame motion length. The two
   position vectors are reached through a single base pointer, so they share one
   address register; the motion length keeps its own global. (0x8003df60,
   68 bytes.) */
void func_8003DF60(void) {
  int *p = g_anSpyroWorldPos;
  CopyVector(p, p + 0x23);
  ZeroVector(p + 0x43);
  g_nSpyroFrameMotionLength = 0;
}
