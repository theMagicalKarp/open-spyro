#include "globals.h"

extern int ArcTan2(int y, int x, int high_precision);
extern int AbsAngleDelta8(int a, int b);

/* True when the entity's byte yaw (+0x46) points within `threshold` of the
   direction from the entity (+0xC,+0x10) to Spyro. */
int func_800381E8(int *e, int threshold) {
  return AbsAngleDelta8(ArcTan2(g_anSpyroWorldPos[0] - e[3],
                                g_anSpyroWorldPos[1] - e[4], 0),
                        ((unsigned char *)e)[0x46]) < threshold;
}
