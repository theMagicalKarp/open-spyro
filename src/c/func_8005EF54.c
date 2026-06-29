#include "globals.h"

void func_8005EF54(void *p) {
  ((unsigned char *)p)[3] = 7;
  ((unsigned char *)p)[7] = 0x58;
  *(int *)((char *)p + 0x1C) = 0x55555555;
}
