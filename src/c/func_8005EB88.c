#include "globals.h"

/* Set the active graphics type, returning the previous value. */
int func_8005EB88(int type) {
  int old = g_nGraphType;
  g_nGraphType = type;
  return old;
}
