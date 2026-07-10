#include "globals.h"

/* OR `bits` into the +0x18 flags word of every actor in pickup group `group`
   that lies inside a box + cone around actor `actor` (0x8003b294): per-axis
   |dx|,|dy| < xyLim, ApproxDist2D < distLim, |dz| < zLim, and the 8-bit
   heading from `actor` to the member within angLim of actor's facing byte
   (+0x46). Members with type byte (+0x48) >= 0x80 are skipped. */
extern int ApproxDist2D(int *a, int *b);
extern int ArcTan2(int y, int x, int high_precision);
extern int AbsAngleDelta8(int a, int b);

void func_8003B294(int actor, int group, unsigned int bits, int distLim,
                   int zLim, int xyLim, int angLim) {
  unsigned short *ptr;
  int idx;
  char *rec;
  int d;

  if (group < g_nLevelPickupTableCount) {
    ptr = ((unsigned short **)g_pLevelPickupTableEntries)[group];
    do {
      idx = *ptr & 0x7FFF;
      rec = (char *)g_pActorListBase + idx * 0x58;
      if ((unsigned int)*(unsigned char *)(rec + 0x48) < 0x80) {
        d = *(int *)(actor + 0xC) - *(int *)(rec + 0xC);
        if (d > 0) {
          if (d >= xyLim) {
            continue;
          }
          goto xdone;
        }
        if (*(int *)(rec + 0xC) - *(int *)(actor + 0xC) < xyLim) {
          goto xdone;
        }
        continue;
      xdone:
        d = *(int *)(actor + 0x10) - *(int *)(rec + 0x10);
        if (d > 0) {
          if (d < xyLim) {
            goto ydone;
          }
          continue;
        }
        if (*(int *)(rec + 0x10) - *(int *)(actor + 0x10) >= xyLim) {
          continue;
        }
      ydone:
        if (ApproxDist2D((int *)(actor + 0xC), (int *)(rec + 0xC)) >= distLim) {
          continue;
        }
        d = *(int *)(rec + 0x14) - *(int *)(actor + 0x14);
        if (d > 0) {
          if (d < zLim) {
            goto zdone;
          }
          continue;
        }
        if (*(int *)(actor + 0x14) - *(int *)(rec + 0x14) >= zLim) {
          continue;
        }
      zdone:
        if (AbsAngleDelta8(
                *(unsigned char *)(actor + 0x46),
                ArcTan2(*(int *)(rec + 0xC) - *(int *)(actor + 0xC),
                        *(int *)(rec + 0x10) - *(int *)(actor + 0x10), 0)) <
            angLim) {
          *(unsigned int *)(rec + 0x18) |= bits;
        }
      }
    } while ((short)*ptr++ >= 0);
  }
}
