#include "globals.h"

extern void *memcpy(void *dst, void *src, int n);

/* PSY-Q GetDispEnv: copy the 20-byte cached DISPENV back into the caller's
   buffer `env` and return it (0x800604d8, 56 bytes). */
void *func_800604D8(void *env) {
  memcpy(env, g_abPutDispEnvCache, 0x14);
  return env;
}
