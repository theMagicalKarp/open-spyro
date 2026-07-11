#include "globals.h"

/* libgpu PutDrawEnv (0x8005fedc, 0x11C). Optionally trace, rebuild the env's
   embedded DR_ENV packet via SetDrawEnv, splice the caller's tag into the
   packet's OT link word (+0x1C), enqueue it through the dispatch table's +0x8
   method, then cache the whole 0x5C-byte DRAWENV as the current one (debug
   block +0xE). */
extern char D_80011928[]; /* GPU debug tag */
extern int SetDrawEnv(unsigned char *prim, unsigned char *env);
extern unsigned char g_abGpuDebugStateBlock[];

typedef struct {
  int w0, w1, w2, w3;
} EnvWords16;

typedef struct {
  int w0, w1, w2;
} EnvWords12;

void func_8005FEDC(int tag, unsigned char *env) {
  unsigned char *dbg = g_abGpuDebugStateBlock;
  unsigned char *prim;
  char *tbl;
  EnvWords16 *src;
  EnvWords16 *dst;
  EnvWords16 *end;

  if (*dbg >= 2) {
    ((void (*)(char *, int, unsigned char *))g_pfnGpuDebugPrintf)(D_80011928,
                                                                  tag, env);
  }
  prim = env + 0x1C;
  SetDrawEnv(prim, env);
  tbl = (char *)g_pGpuDispatchTable;
  *(int *)(env + 0x1C) = (*(int *)(env + 0x1C) & 0xFF000000) | (tag & 0xFFFFFF);
  (*(void (**)(int, unsigned char *, int, int))(tbl + 0x8))(
      *(int *)(tbl + 0x18), prim, 0x40, 0);

  dst = (EnvWords16 *)(dbg + 0xE);
  src = (EnvWords16 *)env;
  end = src + 5;
  do {
    *dst++ = *src++;
  } while (src != end);
  *(EnvWords12 *)dst = *(EnvWords12 *)src;
}
