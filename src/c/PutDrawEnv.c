#include "globals.h"

/* libgpu PutDrawEnv (0x8005fdd8, 0x104). Optionally trace, rebuild the env's
   embedded DR_ENV packet via SetDrawEnv, OR the OT-link tag word (+0x1C) full
   of 1s, enqueue it through the dispatch table's +0x8 method, then cache the
   whole 0x5C-byte DRAWENV as the current one (debug block +0xE). Returns env.
   Sibling of the matched func_8005FEDC. */
extern char D_80011910[]; /* "PutDrawEnv(%08x)...\n" */
extern int SetDrawEnv(unsigned char *prim, unsigned char *env);
extern unsigned char g_abGpuDebugStateBlock[];

typedef struct {
  int w0, w1, w2, w3;
} EnvWords16;

typedef struct {
  int w0, w1, w2;
} EnvWords12;

unsigned char *PutDrawEnv(unsigned char *env) {
  unsigned char *dbg = g_abGpuDebugStateBlock;
  unsigned char *prim;
  char *tbl;
  EnvWords16 *src;
  EnvWords16 *dst;
  EnvWords16 *end;

  if (*dbg >= 2) {
    ((void (*)(char *, unsigned char *))g_pfnGpuDebugPrintf)(D_80011910, env);
  }
  prim = env + 0x1C;
  SetDrawEnv(prim, env);
  tbl = (char *)g_pGpuDispatchTable;
  *(int *)(env + 0x1C) |= 0xFFFFFF;
  (*(void (**)(int, unsigned char *, int, int))(tbl + 0x8))(
      *(int *)(tbl + 0x18), prim, 0x40, 0);

  dst = (EnvWords16 *)(dbg + 0xE);
  src = (EnvWords16 *)env;
  end = src + 5;
  do {
    *dst++ = *src++;
  } while (src != end);
  *(EnvWords12 *)dst = *(EnvWords12 *)src;
  return env;
}
