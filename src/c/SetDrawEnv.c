#include "globals.h"

#define p ((u_long *)dr_env)

extern unsigned int BuildDrawAreaTopLeft(short x, short y);
extern unsigned int BuildDrawAreaBottomRight(short x, short y);
extern unsigned int BuildDrawOffsetCommand(int x, int y);
extern unsigned int BuildDrawModeWord(int dfe, int dtd, unsigned int tpage);
extern unsigned int BuildTextureWindow(RECT *tw);

/* 0x800608e0 (0x290) — libgpu SetDrawEnv: same packet as func_800606C8,
   but the background clear uses a 0x60 fill-rectangle (relative to the draw
   offset) only when the clip x or width is not 64-pixel aligned, and the
   faster 0x02 VRAM fill at the absolute rectangle otherwise. Source notes as
   for func_800606C8. */
void SetDrawEnv(DR_ENV *packet, DRAWENV *drawenv) {
  DRAWENV *env = drawenv;
  DR_ENV *dr_env = packet;
  RECT rect;
  int n;

  p[1] = BuildDrawAreaTopLeft(env->clip.x, env->clip.y);
  p[2] = BuildDrawAreaBottomRight(env->clip.w + env->clip.x - 1,
                                  env->clip.y + env->clip.h - 1);
  p[3] = BuildDrawOffsetCommand(env->ofs[0], env->ofs[1]);
  p[4] = BuildDrawModeWord(env->dfe, env->dtd, env->tpage);
  p[5] = BuildTextureWindow(&env->tw);
  p[6] = 0xe6000000;
  n = 7;
  if (env->isbg) {
    rect.x = env->clip.x;
    rect.y = env->clip.y;
    rect.w = env->clip.w;
    rect.h = env->clip.h;
    rect.w =
        rect.w < 0
            ? 0
            : (rect.w > g_nGpuScreenWidth - 1 ? g_nGpuScreenWidth - 1 : rect.w);
    rect.h = rect.h < 0
                 ? 0
                 : (rect.h > g_nGpuScreenHeight - 1 ? g_nGpuScreenHeight - 1
                                                    : rect.h);
    if ((rect.x & 0x3f) || (rect.w & 0x3f)) {
      rect.x -= env->ofs[0];
      rect.y -= env->ofs[1];
      p[n++] = 0x60000000 | (env->b0 << 16) | (env->g0 << 8) | env->r0;
      p[n++] = *(u_long *)&rect.x;
      p[n++] = *(u_long *)&rect.w;
      rect.x += env->ofs[0];
      rect.y += env->ofs[1];
    } else {
      p[n++] = 0x02000000 | (env->b0 << 16) | (env->g0 << 8) | env->r0;
      p[n++] = *(u_long *)&rect.x;
      p[n++] = *(u_long *)&rect.w;
    }
  }
  ((unsigned char *)dr_env)[3] = n - 1;
}
