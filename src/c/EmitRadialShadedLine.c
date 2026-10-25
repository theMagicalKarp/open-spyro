#include "funcs.h"
#include "globals.h"
#include "gpu_prim.h"
#include "vecmath.h"

/* 0x8001844c (0xE8) - build a LINE_G2 prim between two screen points,
   shading each endpoint by its angular distance from the light-sweep
   angle (radial highlight), then queue it and bump the prim cursor.

   Both shades come off the 0x80 full-brightness base, and the far endpoint's
   shade must be a FRESH local: local-alloc ties a destination to a source that
   dies in the same insn, so `far = full - AbsAngleDelta8(...)` keeps the
   subtract in place in the base's register AND settles the whole-function
   callee-saved roles. Reusing a parameter for it costs the role rotation, and
   a self-decrement (`full = full - ...`) makes `full` a two-death quantity
   that local-alloc drops, so global-alloc puts it on a fifth callee-saved and
   the function overflows its slot. */
void EmitRadialShadedLine(int x0, int y0, int x1, int y1) {
  LineG2 *prim;
  int shade0;
  int shade;
  int full;
  int shade1;
  int dx;
  int dy;

  prim = g_pPrimBufferWriteCursor;
  prim->tag = 0x04000000;
  prim->code = 0x50;
  prim->x0 = x0;
  prim->y0 = y0;
  prim->x1 = x1;
  prim->y1 = y1;

  shade = x0 - 0x100;
  shade0 = AbsAngleDelta8(ArcTan2_8bit(shade, y0 - 0x78),
                          g_anLightSweepAngleBlock[0]);
  dx = x1 - 0x100;
  dy = y1 - 0x78;
  full = 0x80;
  shade0 = full - shade0;
  prim->r0 = shade0 + 0x60;
  prim->g0 = shade0 + 0x60;
  prim->b0 = shade0;

  shade1 =
      full - AbsAngleDelta8(ArcTan2_8bit(dx, dy), g_anLightSweepAngleBlock[0]);
  shade = shade1 + 0x60;
  prim->r1 = shade;
  prim->g1 = shade;
  prim->b1 = shade1;

  AddPrimToOT((int)prim);
  g_pPrimBufferWriteCursor = prim + 1;
}
