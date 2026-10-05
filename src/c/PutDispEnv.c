#include "globals.h"

/* The GP1(0x05) display-start word is built per arm with the y|x value and
   the 0x05000000 command in opposite registers (`$3`/`$2` in the get_dx arm,
   `$2`/`$3` in the 10-bit arm), joined by one `or` into a `$4` argument and
   a `$2` function pointer. jump.c then cross-jumps just the `or` + call tail,
   as in the original. Empty do/while(0) barriers keep the command `lui` last
   in each arm and the dispatch-table load after the `or`.
   The PutDispEnv cache shorts are VOLATILE reads (lhu + separate sll/sra; the
   first keeps its `la`). The 0x10 int compare is not volatile. The vertical
   start reads screen.y once into `y`. The display height test is a computed
   flag over a block-local `h`. All four range clamps are nested ternaries
   whose limit is itself a pad0 ternary. */

extern int FUN_80060e28(RECT *r);
extern int GetGraphType(void);
extern void *memcpy(void *dst, void *src, int n);
extern char D_80011944[]; /* "PutDispEnv(%08x)...\n" */

#define GPU_CW(x) ((*(void (**)(int))((char *)g_pGpuDispatchTable + 0x10))(x))
#define CACHE_S(off)                                                           \
  ((short)*(volatile unsigned short *)&g_abPutDispEnvCache[off])

/* 0x80060030 (0x4a8) — libgpu PutDispEnv: program the display start, the
   horizontal/vertical display range (only when env->screen differs from the
   cached environment) and the display mode (only when the mode-relevant
   fields differ), then cache the environment. */
DISPENV *PutDispEnv(DISPENV *env) {
  int hs;
  int he;
  int vs;
  int ve;
  int mode;
  int y;
  int big;

  mode = 0x08000000;
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, DISPENV *))g_pfnGpuDebugPrintf)(D_80011944, env);
  }
  if (g_bGpuDebugType == 1 || g_bGpuDebugType == 2) {
    register int yx asm("$3");
    register int cmd asm("$2");

    yx = ((env->disp.y & 0xfff) << 12) | (FUN_80060e28(&env->disp) & 0xfff);
    do {
    } while (0);
    cmd = 0x05000000;
    {
      register int arg asm("$4");
      register void (*fn)(int) asm("$2");

      arg = cmd | yx;
      do {
      } while (0);
      fn = *(void (**)(int))((char *)g_pGpuDispatchTable + 0x10);
      fn(arg);
    }
  } else {
    register int yx asm("$2");
    register int cmd asm("$3");

    yx = (env->disp.y & 0x3ff) << 10;
    yx |= env->disp.x & 0x3ff;
    do {
    } while (0);
    cmd = 0x05000000;
    {
      register int arg asm("$4");
      register void (*fn)(int) asm("$2");

      arg = yx | cmd;
      do {
      } while (0);
      fn = *(void (**)(int))((char *)g_pGpuDispatchTable + 0x10);
      fn(arg);
    }
  }
  if (CACHE_S(8) != env->screen.x || CACHE_S(10) != env->screen.y ||
      CACHE_S(12) != env->screen.w || CACHE_S(14) != env->screen.h) {
    env->pad0 = GetGraphType();
    hs = env->screen.x * 10 + 0x260;
    y = env->screen.y;
    vs = env->pad0 ? y + 0x13 : y + 0x10;
    he = env->screen.w ? hs + env->screen.w * 10 : hs + 0xa00;
    ve = env->screen.h ? vs + env->screen.h : vs + 0xf0;
    hs = hs < 0x1f4 ? 0x1f4 : (hs > 0xcda ? 0xcda : hs);
    he = he < hs + 0x50 ? hs + 0x50 : (he > 0xcda ? 0xcda : he);
    vs = vs < 0x10
             ? 0x10
             : (vs > (env->pad0 ? 0x136 : 0x100) ? (env->pad0 ? 0x136 : 0x100)
                                                 : vs);
    ve = ve < vs + 2
             ? vs + 2
             : (ve > (env->pad0 ? 0x138 : 0x102) ? (env->pad0 ? 0x138 : 0x102)
                                                 : ve);
    GPU_CW(0x06000000 | ((he & 0xfff) << 12) | (hs & 0xfff));
    GPU_CW(0x07000000 | ((ve & 0x3ff) << 10) | (vs & 0x3ff));
  }
  if (*(int *)&g_abPutDispEnvCache[0x10] != *(int *)&env->isinter ||
      CACHE_S(0) != env->disp.x || CACHE_S(2) != env->disp.y ||
      CACHE_S(4) != env->disp.w || CACHE_S(6) != env->disp.h) {
    env->pad0 = GetGraphType();
    if (env->pad0 == 1) {
      mode |= 0x08;
    }
    if (env->isrgb24) {
      mode |= 0x10;
    }
    if (env->isinter) {
      mode |= 0x20;
    }
    if (g_bGpuReverseFlag) {
      mode |= 0x80;
    }
    if (env->disp.w > 0x118) {
      if (env->disp.w <= 0x160) {
        mode |= 0x01;
      } else if (env->disp.w <= 0x190) {
        mode |= 0x40;
      } else if (env->disp.w <= 0x230) {
        mode |= 0x02;
      } else {
        mode |= 0x03;
      }
    }
    {
      int h = env->disp.h;
      big = env->pad0 ? h < 0x121 : h < 0x101;
    }
    if (!big) {
      mode |= 0x24;
    }
    GPU_CW(mode);
  }
  memcpy(g_abPutDispEnvCache, env, 0x14);
  return env;
}
