#include "globals.h"

/* Replace the low 24 bits of *p with val (keeping the top byte), returning the
   merged word. */
int func_8005ED54(int *p, int val) {
  return *p = (*p & 0xFF000000) | (val & 0xFFFFFF);
}
