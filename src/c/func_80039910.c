#include "globals.h"

/* Drive an actor's tracked move toward its target (0x80039910, 0x198).
   Picks the probe mode/distance from the actor's +0x3B flag byte, runs the
   guided-move step (func_80039688) while *counter is live, then while
   *timer is live applies the vertical timer decay to the actor's Y
   (+0x14), clamping the timer at -0x104 and floor-snapping via the +0x38
   halfword once the timer runs out. Returns the move-step result, or 3
   when the floor snap landed. */
extern int func_80039688(int *actor, int a, int b, int dist, int c, int mode);
extern int func_80038400(int *actor, int a);
extern void EncodeCachedVecToActorDirCode(int *actor);

int func_80039910(int *actor, int *counter, int arg2, int *timer, int step,
                  int decay) {
  int ret;
  int mode;
  int dist;
  int b;
  int t;
  int r4;

  ret = 0;
  mode = 0x21;
  dist = 0x12C;
  if (timer == 0 || *timer == 0xFFFF) {
    mode = 0x25;
  }
  b = *((unsigned char *)actor + 0x3B);
  if (b == 0xFE) {
    mode = 1;
  } else if (b != 0) {
    mode &= ~0x20;
    dist = b << 2;
    if (b == 0xFF) {
      dist = 0;
    }
  }
  if (*counter != 0) {
    ret = func_80039688(actor, arg2, *counter, dist, 0x1F4, mode);
    t = *counter - step;
    *counter = t;
    if (t < 0) {
      *counter = 0;
    }
  }
  if (ret == 2) {
    return 2;
  }
  if (timer != 0 && *timer != 0xFFFF) {
    r4 = func_80038400(actor, 0x258);
    actor[5] += *timer;
    *timer -= decay;
    if (*timer < -0x104) {
      *timer = -0x104;
    }
    if (*timer <= 0) {
      t = r4 + *(short *)((char *)actor + 0x38);
      if (actor[5] < t) {
        actor[5] = t;
        ret = 3;
      }
    }
    EncodeCachedVecToActorDirCode(actor);
  }
  return ret;
}
