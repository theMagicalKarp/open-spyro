#include "globals.h"

extern void *memcpy(void *dst, void *src, int n);

/* PSY-Q GetDrawEnv: copy the 92-byte cached DRAWENV back into the caller's
   buffer `env` and return it (0x8005fff8, 56 bytes). */
void *func_8005FFF8(void *env) {
  memcpy(env, g_abPutDrawEnvCache, 0x5C);
  return env;
}
