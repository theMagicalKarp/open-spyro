#include "globals.h"

/* PSY-Q SetDefDispEnv: initialise a DISPENV display rectangle (x,y,w,h) with a
   zeroed screen rect and default (non-interlaced, 16-bit) flags; returns env.
   (0x8005eb4c, 60 bytes.) */
DISPENV *InitDefaultDispEnv(DISPENV *env, short x, short y, short w, int h) {
  env->disp.x = x;
  env->disp.y = y;
  env->disp.w = w;
  env->disp.h = h;
  env->screen.x = 0;
  env->screen.y = 0;
  env->screen.w = 0;
  env->screen.h = 0;
  env->isrgb24 = 0;
  env->isinter = 0;
  env->pad1 = 0;
  env->pad0 = 0;
  return env;
}
