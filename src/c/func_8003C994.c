#include "globals.h"

extern void SubtractVector(int *dst, int *a, int *b);
extern unsigned int VectorLength(int *vec, int include_z);
extern void func_8003BAD0(unsigned char *pos, int *wp, int speed, int a, int b,
                          int c);

/* 0x8003c994 (0x130) — advance an actor along its waypoint path record:
   measure the distance from `pos`+0xC to the current waypoint (path index
   byte +0x1, 0x10-byte waypoint records from +0x8); when closer than
   `threshold`, step the index (+0x6 halfword == -1 selects forward wrap
   [0..count), else backward wrap to count-1); then steer toward the (possibly
   new) waypoint with speed = dist>>4 clamped to `cap`. */
void func_8003C994(unsigned char *pos, unsigned char *path, int threshold,
                   int cap, int arg5, int arg6, int arg7) {
  int v[3];
  int dist;

  SubtractVector(v, (int *)(pos + 0xC), (int *)(path + ((path[1] << 4) + 8)));
  dist = VectorLength(v, 1);
  if (dist < threshold) {
    if (*(short *)(path + 6) == -1) {
      path[1] += 1;
      if (path[1] == path[0]) {
        path[1] = 0;
      }
    } else {
      path[1] -= 1;
      if (path[1] == 0xFF) {
        path[1] = path[0] - 1;
      }
    }
  }

  dist = dist >> 4;
  if (cap < dist) {
    dist = cap;
  }
  func_8003BAD0(pos, (int *)(path + ((path[1] << 4) + 8)), dist, arg6, arg7,
                arg5);
}
