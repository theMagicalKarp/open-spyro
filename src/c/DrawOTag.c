#include "globals.h"

extern char D_800118FC[]; /* "DrawOTag" GPU debug tag */

/* libgpu DrawOTag: optionally trace the call (debug level >= 2) then dispatch
   the ordering-table draw through the GPU command table — the submit method at
   +0x8, called with the table's primitive context (+0x18), the OT, and a zero
   length/flag pair. (0x8005fd64, 116 bytes.) */
void DrawOTag(u_long *p) {
  if (g_bGpuDebugLevel >= 2) {
    ((void (*)(char *, u_long *))g_pfnGpuDebugPrintf)(D_800118FC, p);
  }
  (*(void (**)(int, u_long *, int, int))((char *)g_pGpuDispatchTable + 0x8))(
      *(int *)((char *)g_pGpuDispatchTable + 0x18), p, 0, 0);
}
