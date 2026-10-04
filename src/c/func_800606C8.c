#include "globals.h"

#define p ((u_long *)dr_env)

extern unsigned int BuildDrawAreaTopLeft(short x, short y);
extern unsigned int BuildDrawAreaBottomRight(short x, short y);
extern unsigned int BuildDrawOffsetCommand(int x, int y);
extern unsigned int BuildDrawModeWord(int dfe, int dtd, unsigned int tpage);
extern unsigned int BuildTextureWindow(RECT *tw);

/* 0x800606c8 (0x218) — libgpu SetDrawEnv packet builder: fill `packet` with
   the GP0 commands for `drawenv` (clip top-left/bottom-right, draw offset,
   draw mode, texture window, mask setting) and, when the environment clears
   its background, a fill-rectangle of the clip area (size clamped to the
   screen mode, position relative to the draw offset). The packet length goes
   in the tag's top byte.

   The local copies of the two parameters, env first, set the prologue's
   homing order (A40); the clip rectangle is copied field by field (a struct
   assignment becomes lwl/lwr); the size clamps are the get_cs nested
   ternaries. */
void func_800606C8(DR_ENV *packet, DRAWENV *drawenv) {
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
    rect.x -= env->ofs[0];
    rect.y -= env->ofs[1];
    p[n++] = 0x60000000 | (env->b0 << 16) | (env->g0 << 8) | env->r0;
    p[n++] = *(u_long *)&rect.x;
    p[n++] = *(u_long *)&rect.w;
    rect.x += env->ofs[0];
    rect.y += env->ofs[1];
  }
  ((unsigned char *)dr_env)[3] = n - 1;
}
