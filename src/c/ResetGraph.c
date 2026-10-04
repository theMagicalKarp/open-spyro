#include "globals.h"

/* libgpu public ResetGraph thunk. mode&7 == 0/3/5 → full reset path: clear the
   debug/env state, GPU_cw the current dispatch table, run ResetGraphImpl, then
   cache the selected mode's screen resolution and invalidate the PutDrawEnv /
   PutDispEnv caches; returns the debug type. Any other mode → optional debug
   trace then dispatch table +0x34 (cancel) with 1. (0x8005f2a4, 388 bytes.)

   The queue-mode flag and the width/height pair are fields of libgpu's state
   block, so they are stored as record members: an in-struct store depends on
   the in-struct mode-table reads (sched.c true_dependence), which keeps the
   original's flag, lw/sh, lw/sh interleave instead of batching both loads. */

extern int WritePrintf(char *fmt, ...);
extern void memset(unsigned char *dst, int val, int n);
extern void FUN_8005ddc8(void);
extern void GPU_cw();
extern unsigned int ResetGraphImpl(unsigned int mode);
extern unsigned char g_abGpuDebugBlock[]; /* alias view of g_bGpuDebugType */
extern unsigned char D_800117A8[];        /* "ResetGraph:jtb=%08x,env=%08x\n" */
extern unsigned int D_80074A1C;           /* libgpu version id string block */
extern unsigned char D_800117C8[];        /* "ResetGraph(%d)...\n" */

int ResetGraph(int mode) {
  int m;
  int r;

  m = mode & 7;
  if (m == 3) {
    goto write_printf;
  }
  if (m < 4) {
    if (m == 0) {
      goto write_printf;
    }
    goto else_block;
  }
  if (m == 5) {
    goto reset_block;
  }
  goto else_block;

write_printf:
  WritePrintf(D_800117A8, &D_80074A1C, &g_bGpuDebugType);
reset_block:
  memset(&g_abGpuDebugBlock[0], 0, 0x80);
  FUN_8005ddc8();
  GPU_cw((int)g_pGpuDispatchTable & 0xffffff);
  r = ResetGraphImpl(mode);
  g_abGpuDebugBlock[0] = (unsigned char)r;
  ((struct { unsigned char v; } *)&g_bGpuQueueModeActive)->v = 1;
  ((struct { short v; } *)&g_nGpuScreenWidth)->v =
      g_anGpuModeHResTable[r & 0xff];
  ((struct { short v; } *)&g_nGpuScreenHeight)->v =
      g_anGpuModeVResTable[r & 0xff];
  memset(&g_abGpuDebugBlock[0x10], -1, 0x5c);
  memset(&g_abGpuDebugBlock[0x6c], -1, 0x14);
  return g_abGpuDebugBlock[0];

else_block:
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)())g_pfnGpuDebugPrintf)(D_800117C8, mode);
  }
  return (*(int (**)())((char *)g_pGpuDispatchTable + 0x34))(1);
}
