/* 0x8003A420 (size 0x300): actor waypoint-follow step. Steers an actor toward
 * its current waypoint (wp[1] index into a 0x10-byte table at wp+8), clamps the
 * per-frame turn to +/-arg5, moves by the (matrix-rotated) capped speed,
 * resolves ground contact, then advances to the next waypoint when close
 * enough.
 *
 * The ground-probe result is a `$3` register local: as a pseudo its call-result
 * copy gives it a hard v0 preference, find_reg takes v0, and jump.c then
 * cross-jumps the two `vec[2] =` stores (-3 insns).  `ret = 0` sits ABOVE the
 * last SubtractVector so ret's range overlaps arg2's and it takes a fresh
 * callee-saved (s4) instead of sharing arg2's s1.  `abs()` is the builtin
 * (A160). */

extern int abs(int);
extern void SubtractVector(int *dst, int *a, int *b);
extern unsigned int VectorLength(int *vec, int include_z);
extern int ArcTan2(int y, int x, int high_precision);
extern void ApplyEulerRotation(unsigned int *eul, unsigned int *mtx,
                               unsigned int *unused);
extern void RotateVectorByMatrix(int *mtx, int *dst, int *src);
extern void AddVector(int *dst, int *a, int *b);
extern void CopyVector(int *dst, int *src);
extern void RShiftVector3(int *vec, unsigned int n);
extern int DispatchActorContactAtSphere(int *p, int r, int c, int d, int actor,
                                        int f);
extern int FindGroundHeightBelow(int *p, int z);
extern void EncodeCachedVecToActorDirCode(int actor);
extern void func_800529E4(char *actor, int mode);

int func_8003A420(char *actor, unsigned char *wp, int arg2, int cap, int arg4,
                  int arg5, int arg6, int *arg7) {
  unsigned char eul[3];
  int vec[3];
  int mtx[8];
  int len;
  int base;
  int delta;
  int absv;
  int ret;

  SubtractVector(vec, (int *)(wp + ((wp[1] << 4) + 8)), (int *)(actor + 0xC));
  eul[0] = 0;
  eul[1] = ArcTan2(VectorLength(vec, 0), vec[2], 0);
  if ((int)VectorLength(vec, 0) < arg2) {
    eul[2] = actor[0x46];
  } else {
    delta = ArcTan2(vec[0], vec[1], 0);
    base = actor[0x46];
    delta = (delta - base) & 0xFF;
    if (delta >= 0x81) {
      delta -= 0x100;
    }
    absv = abs(delta);
    if (arg6 < absv) {
      cap = 0;
    }
    if (delta < -arg5) {
      delta = -arg5;
    }
    if (arg5 < delta) {
      delta = arg5;
    }
    eul[2] = base + delta;
    actor[0x46] = base + delta;
  }
  len = VectorLength(vec, 1);
  if (len < cap) {
    cap = len;
  }
  ApplyEulerRotation((unsigned int *)eul, (unsigned int *)mtx, 0);
  vec[0] = cap;
  vec[1] = 0;
  vec[2] = 0;
  RotateVectorByMatrix(mtx, vec, vec);
  if (arg7 != 0) {
    AddVector(arg7, arg7, vec);
    CopyVector(vec, arg7);
    RShiftVector3(vec, 2);
    SubtractVector(arg7, arg7, vec);
    AddVector(vec, (int *)(actor + 0xC), arg7);
  } else {
    AddVector(vec, (int *)(actor + 0xC), vec);
  }
  if (arg4 != 0) {
    DispatchActorContactAtSphere(vec, arg4, 0, 0, (int)actor, 0);
  }
  vec[2] += 0x400;
  {
    register int ground asm("$3");
    ground = FindGroundHeightBelow(vec, 0x400);
    if (ground != 0) {
      vec[2] = ground;
    } else {
      vec[2] -= 0x400;
    }
  }
  CopyVector((int *)(actor + 0xC), vec);
  EncodeCachedVecToActorDirCode((int)actor);
  func_800529E4(actor, 2);
  ret = 0;
  SubtractVector(vec, (int *)(wp + ((wp[1] << 4) + 8)), (int *)(actor + 0xC));
  if ((int)VectorLength(vec, 1) < arg2) {
    actor[0x49] = 0;
    wp[1] = wp[1] + 1;
    if ((wp[1] & 0xFF) == wp[0]) {
      wp[1] = 0;
    }
    ret = wp[1] + 0x100;
  }
  return ret;
}
