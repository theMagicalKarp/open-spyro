#include "globals.h"

void func_8005EF94(void *p) {
  ((unsigned char *)p)[3] = 9;
  ((unsigned char *)p)[7] = 0x5C;
  *(int *)((char *)p + 0x24) = 0x55555555;
}
