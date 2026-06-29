#include "globals.h"

/* Install root-counter callback `fn` for counter `idx`, skipping the store if
   it is already current. */
void func_8005E5D8(int idx, void *fn) {
  if (fn != g_apfnRcntCallbacks[idx]) {
    g_apfnRcntCallbacks[idx] = fn;
  }
}
