#include "globals.h"

/* Dispatch a GPU primitive through the libgpu command table: first invoke the
   sync/begin entry at +0x3c with 0, then the submit entry at +0x14 with the
   primitive body (p+4) and the length byte stashed from p[3]. (0x8005fd04,
   96 bytes.) */
void func_8005FD04(unsigned char *p) {
  int len = p[3];
  (*(void (**)(int))((char *)g_pGpuDispatchTable + 0x3c))(0);
  (*(void (**)(unsigned char *, int))((char *)g_pGpuDispatchTable + 0x14))(
      p + 4, len);
}
