#include "globals.h"

extern void SubtractVector();
extern void CopyVector();
extern void ScaleVector3Sat();
extern void AddVector();
extern void RShiftVector3();
extern int VectorLength();
extern int ArcTan2_8bit(int y, int x);
extern void func_800529E4();

extern short D_80075280[];
extern short D_8006CBA4[];
extern short D_8006CBB4[];
extern short D_8006CBCC[];

/* 0x8003bfc0 — advance an actor one sub-step along its waypoint ring.
   `path` is the waypoint block (count byte [0], cursor byte [1], vec3s at
   +8 + i*0x10) and `step` the sub-step counter within the current leg, which
   takes `legs` frames. Once the counter reaches the leg length the cursor
   advances (wrapping to 0, returning 2 instead of 1) and the raw leg vector
   is published in `prev`. Otherwise the remaining leg is divided into the
   frames still to run; for the first steps of a leg that share of the new
   leg is cross-faded with `prev` through the per-length weight table
   (func_8003A920's tables). Flags bit 2 keeps the vertical component. The
   step vector is folded into the actor position and its yaw/pitch bytes
   (+0x46/+0x45) are re-aimed along it.
   Match notes: the empty do/while(0) keeps the idx load ahead of the *step
   load (the original's load-delay nop); `ret++` precedes the cursor test so
   reorg puts it in the branch delay slot; the divisor is a `$6` register
   local (the original's a2). */
int func_8003BFC0(unsigned char *actor, unsigned char *path, int *prev,
                  int *step, int legs, int flags) {
  int dir[4];
  int blend[4];
  short *tbl;
  int *pos;
  int idx;
  int t;
  int lim;
  int x;
  int n;
  register int span asm("$6");
  int ret;

  ret = 0;
  if (legs >= 9) {
    tbl = D_8006CBCC;
    lim = 7;
  } else if (legs >= 7) {
    tbl = D_8006CBB4;
    lim = 5;
  } else {
    lim = 1;
    if (legs >= 5) {
      tbl = D_8006CBA4;
      lim = 3;
    } else {
      tbl = D_80075280;
    }
  }

  idx = path[1];
  do {
  } while (0);
  t = *step;
  if (t >= legs) {
    SubtractVector(dir, path + (idx * 0x10 + 8), actor + 0xc);
    CopyVector(prev, dir);
    ret++;
    if (++path[1] >= path[0]) {
      path[1] = 0;
      ret++;
    }
    *step = 0;
  } else if (lim < t) {
    SubtractVector(dir, path + (idx * 0x10 + 8), actor + 0xc);
    n = *step - 1;
    span = legs - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    *step = *step + 1;
  } else {
    SubtractVector(dir, path + (idx * 0x10 + 8), actor + 0xc);
    n = *step - 1;
    span = legs - n;
    dir[0] = dir[0] / span;
    dir[1] = dir[1] / span;
    dir[2] = dir[2] / span;
    ScaleVector3Sat(dir, dir, ((short *)((*step << 2) + (int)tbl))[0]);
    ScaleVector3Sat(blend, prev, ((short *)((*step << 2) + (int)tbl))[1]);
    AddVector(dir, dir, blend);
    RShiftVector3(dir, 0xa);
    *step = *step + 1;
  }

  if (!(flags & 4)) {
    dir[2] = 0;
  }
  actor[0x46] = ArcTan2_8bit(dir[0], dir[1]);
  x = ArcTan2_8bit(dir[2], VectorLength(dir, 0));
  pos = (int *)(actor + 0xc);
  actor[0x45] = -x + 0x40;
  AddVector(pos, pos, dir);
  func_800529E4(actor, 2);
  return ret;
}
