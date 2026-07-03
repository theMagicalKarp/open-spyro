#include "globals.h"

typedef struct MemCardPathHead {
  char c[6];
} MemCardPathHead;

/* Fill `path` with the memcard device path for `port`: copy the 6-byte
   template (g_szMemCardPathTemplate) then patch the two hex digits at [2]/[3]
   with port/16 and port%16 (+'0'). Returns the low digit. (0x80067d74,
   96 bytes; the 6-byte struct copy is the lwl/lwr pair.) */
int FormatMemCardPath(int port, char *path) {
  int hi;
  int lo;
  *(MemCardPathHead *)path = *(MemCardPathHead *)g_szMemCardPathTemplate;
  hi = port / 16 + '0';
  lo = port % 16 + '0';
  path[2] = hi;
  path[3] = lo;
  return lo;
}
