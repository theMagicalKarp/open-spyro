#include "globals.h"

/* libgpu MoveImage: validate the source RECT, reject empty transfers (-1),
   then fill the 5-word VRAM move packet — [2]=src x/y, [3]=dst (y<<16|x),
   [4]=w/h — and dispatch it through the GPU command table's image-transfer
   method at +0x8 with the queue context from +0x18. (0x8005faf0, 196 bytes.) */
extern char D_800118C0[]; /* "MoveImage" GPU debug tag */
extern void ValidateGpuRect(char *name, RECT *rect);
int MoveImage(RECT *rect, int x, int y) {
  int *p;
  int src;
  int new_var;
  char *tbl;
  ValidateGpuRect(D_800118C0, rect);
  if ((rect->w == (new_var = 0)) || (rect->h == new_var)) {
    return -1;
  }
  p = &g_anGpuMoveImagePacket[2];
  src = *((int *)(&rect->x));
  tbl = *((char *volatile *)(&g_pGpuDispatchTable));
  g_anGpuMoveImagePacket[3] = (y << 16) | (x & 0xFFFF);
  p[new_var] = src;
  g_anGpuMoveImagePacket[4] = *((int *)(&rect->w));
  return (*((int (**)(int, int *, int, int))(tbl + 0x8)))(
      *((int *)(tbl + 0x18)), p - 2, 0x14, new_var);
}
