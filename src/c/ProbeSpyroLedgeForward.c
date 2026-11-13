#include "funcs.h"
#include "globals.h"
#include "spyro_block.h"
#include "spyro_state.h"

/* Airborne probe for a surface ahead of Spyro (0x8003e218, 0x100; JUMP /
   LIFT / FALL / GLIDE_DROP when no wall was touched this step). Cast a ray
   from Spyro's body anchor (SPYRO_RADIUS down in body space) to a point
   0x1C4 ahead / 0x1A4 down; on a hit whose surface normal rises at least
   0x17 (8-bit angle) above horizontal, i.e. a surface no steeper than a
   wall, latch the same wall-contact pair AdvanceSpyroPhysics sets on a wall
   slide: g_nSpyroWallSlideContributedFlag = 1 and the contact normal as the
   nudge (g_anSpyroWallSlideNudgeAccum). Spyro 1 has no ledge grab; the
   integrator just slides his velocity along that normal. */

void ProbeSpyroLedgeForward(void) {
  int from[4];
  int to[3];
  int *anchor;
  int *tp;
  int *cp;
  unsigned int len;
  from[2] = -SPYRO_RADIUS;
  to[0] = 0x1C4;
  from[1] = 0;
  from[0] = 0;
  to[1] = 0;
  to[2] = -0x1A4;
  RotateVectorByMatrix(g_anSpyroBodyMtx, from, from);
  anchor = SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_POS);
  AddVector(from, from, anchor);
  /* The do / while (0) ends cse's extended basic block (A200): the
     match needs the block boundary, not the loop. */
  do {
    tp = to;
    ApplyActiveGteRotation(tp, tp);
  } while (0);
  AddVector(tp, tp, anchor);
  if (CastRayWorldAndActors(from, tp) != 0) {
    cp = g_anCollisionHitNormal;
    len = VectorLength(cp, VEC_LEN_XY);
    if (((signed char)ArcTan2(g_anCollisionHitNormal[2], len, ATAN_ANGLE8)) >=
        0x17) {
      g_anSpyroBodyMtx[32] = 1;
      CopyVector(SPY_VIA(g_anSpyroBodyMtx, SPY_BODY_MTX, SPY_NUDGE), cp);
    }
  }
}
