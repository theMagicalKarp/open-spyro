#include "globals.h"

extern int GetGraphType(void);

/* PSY-Q SetDefDrawEnv: initialise a DRAWENV with clip rect (x,y,w,h), zeroed
   texture window and background colour, dither on, default tpage 10, and
   dfe set when the clip height fits a single field (PAL/NTSC threshold from
   GetGraphType); returns env. (0x8005ea94, 184 bytes.) */
DRAWENV *InitDefaultDrawEnv(DRAWENV *env, short x, short y, short w, int h) {
  int gtype = GetGraphType();
  env->clip.x = x;
  env->clip.y = y;
  env->clip.w = w;
  env->clip.h = h;
  env->tw.x = 0;
  env->tw.y = 0;
  env->tw.w = 0;
  env->tw.h = 0;
  env->r0 = 0;
  env->g0 = 0;
  env->b0 = 0;
  env->dtd = 1;
  env->dfe = gtype ? (h < 289) : (h < 257);
  env->ofs[0] = x;
  env->ofs[1] = y;
  env->tpage = 10;
  env->isbg = 0;
  return env;
}
